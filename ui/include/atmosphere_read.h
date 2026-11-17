#pragma once

#include <atmosphere_print.h>

#include <filesystem>
#include <optional>

///\brief the print edworld_eht last read off a planet's atmosphere (`atmosphere` in its shm_dir, src/edworld_share.h of
/// edworld); the file read whole, when two reads agree and the writer was not inside; none without the file or another layout
[[nodiscard]]
auto read_atmosphere(std::filesystem::path const & path) -> std::optional<atmosphere_print::read_t>;
