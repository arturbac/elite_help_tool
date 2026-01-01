#pragma once
#define SPDLOG_USE_STD_FORMAT
#include <filesystem>
#include <functional>
#include <string_view>
#include <stop_token>
namespace fs = std::filesystem;

using process_callback = std::function<void(std::string_view)>;

[[nodiscard]]
auto find_latest_journal(fs::path const & dir) -> std::optional<fs::path>;
auto tail_file(fs::path const & path, process_callback const & cb, std::stop_token stoken) -> void;

