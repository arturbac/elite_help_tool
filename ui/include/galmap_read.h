#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

///\brief the names edworld_eht read off the galaxy map (`galmap` in its shm_dir, src/edworld_share.h of edworld)
struct galmap_read_t
  {
  std::vector<std::string> labels;
  ///\brief when edworld read them (unix ms); old once the map is closed, edworld writes nothing then
  int64_t unix_ms{};
  };

///\brief the file read whole, when two reads agree and the writer was not inside; none without the file or another layout
[[nodiscard]]
auto read_galmap(std::filesystem::path const & path) -> std::optional<galmap_read_t>;
