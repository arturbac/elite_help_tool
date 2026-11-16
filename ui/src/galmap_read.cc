#include <galmap_read.h>
#include <edworld_share.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <memory>

auto read_galmap(std::filesystem::path const & path) -> std::optional<galmap_read_t>
  {
  for(int attempt{}; attempt != 3; ++attempt)
    {
    auto map{std::make_unique<edworld::galmap_t>()};
    std::ifstream in{path, std::ios::binary};
    if(not in)
      return std::nullopt;
    in.read(reinterpret_cast<char *>(map.get()), sizeof(edworld::galmap_t));
    if(in.gcount() != std::streamsize{sizeof(edworld::galmap_t)} or map->magic != edworld::galmap_magic
       or map->version != edworld::galmap_version)
      return std::nullopt;
    // the seqlock: the sequence read again after the rest, even and the same, or the writer was inside
    uint32_t sequence{};
    in.seekg(offsetof(edworld::galmap_t, sequence));
    in.read(reinterpret_cast<char *>(&sequence), sizeof sequence);
    if(sequence != map->sequence or sequence % 2u != 0u)
      continue;
    galmap_read_t out{.labels = {}, .unix_ms = map->unix_ms};
    for(uint32_t i{}; i != std::min(map->count, edworld::max_galmap_names); ++i)
      out.labels.emplace_back(map->names[i].name, strnlen(map->names[i].name, sizeof map->names[i].name));
    return out;
    }
  return std::nullopt;
  }
