#include <atmosphere_read.h>
#include <edworld_share.h>

#include <cstddef>
#include <cstring>
#include <fstream>

auto read_atmosphere(std::filesystem::path const & path) -> std::optional<atmosphere_print::read_t>
  {
  for(int attempt{}; attempt != 3; ++attempt)
    {
    edworld::atmosphere_t share{};
    std::ifstream in{path, std::ios::binary};
    if(not in)
      return std::nullopt;
    in.read(reinterpret_cast<char *>(&share), sizeof share);
    if(in.gcount() != std::streamsize{sizeof share} or share.magic != edworld::atmosphere_magic
       or share.version != edworld::atmosphere_version)
      return std::nullopt;
    // the seqlock: the sequence read again after the rest, even and the same, or the writer was inside
    uint32_t sequence{};
    in.seekg(offsetof(edworld::atmosphere_t, sequence));
    in.read(reinterpret_cast<char *>(&sequence), sizeof sequence);
    if(sequence != share.sequence or sequence % 2u != 0u)
      continue;
    return atmosphere_print::read_t{
      .ok = share.ok != 0u,
      .known = share.known != 0u,
      .second = share.second != 0u,
      .kind = std::string{share.kind, strnlen(share.kind, sizeof share.kind)},
      .radius = share.radius,
      .r_g = share.r_g,
      .b_g = share.b_g,
      .unix_ms = share.unix_ms
    };
    }
  return std::nullopt;
  }
