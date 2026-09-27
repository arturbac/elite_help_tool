#include <eht_extension.h>

namespace eht::extension
  {
auto make_extension(std::filesystem::path) -> std::unique_ptr<extension_t> { return nullptr; }
  }  // namespace eht::extension
