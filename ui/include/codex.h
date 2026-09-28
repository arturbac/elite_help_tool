#pragma once

#include "logic.h"

#include <overlay_protocol.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace codex_files
  {
///\brief where the codex keeps its page and its pictures, relative to where the tool runs
[[nodiscard]]
auto codex_dir() -> std::filesystem::path;

///\brief where the layer writes and reads - beside the socket, the one place both sides of the game's container see
[[nodiscard]]
auto spool_dir() -> std::filesystem::path;

///\brief a name fit for a file - letters, digits and dashes, nothing a shell or a browser would stumble on
[[nodiscard]]
auto file_safe(std::string_view text) -> std::string;
  }  // namespace codex_files

///\brief the commander's own codex - every species sampled, where it grew, and what it looked like
///
/// A directory beside the tool holds a page, index.html, written from the database: genus by genus,
/// species by species, every world each was found on with its system, its sky and its star, and the
/// pictures the overlay took at the moment of sampling. The pictures and their descriptions live in the
/// directory itself, not in the database - a database can be rebuilt from journals, a picture cannot.
class codex_t final
  {
public:
  ///\brief a picture as the codex keeps it - the file and what was being sampled when it was taken
  struct picture_t
    {
    std::string file;
    std::string taken;
    std::string system;
    std::string body;
    uint64_t system_address{};
    uint32_t body_id{};
    std::string genus;
    std::string species;
    std::string variant;
    ///\brief Log or Sample - which of the three samples it was
    std::string scan;
    double latitude{};
    double longitude{};
    };

  codex_t();

  ///\brief asks the layer for a picture of the scan just seen; the request rides in every frame after
  auto ask_for_picture(current_state_t::organic_scan_seen_t const & scan) -> void;

  ///\brief what goes into the frame - the newest request, or none
  [[nodiscard]]
  auto capture_request() const -> overlay::capture_t const &
    { return request_; }

  ///\brief files the pictures the layer has finished
  ///\returns true when one was added, and the page is worth writing again
  auto collect() -> bool;

  ///\brief writes index.html from the database's finds and the pictures kept
  auto write_page(database_storage_t & db) -> void;

  ///\brief where the journals are - what a picture missing from the list is described out of again
  auto set_journal_dir(std::filesystem::path dir) -> void { journal_dir_ = std::move(dir); }

  ///\brief where the page is, for opening it from the tool
  [[nodiscard]]
  static auto page_path() -> std::filesystem::path;

private:
  struct pending_t
    {
    std::filesystem::path spool;
    picture_t picture;
    std::chrono::steady_clock::time_point asked;
    };

  overlay::capture_t request_;
  std::vector<pending_t> pending_;
  std::vector<picture_t> pictures_;
  bool pictures_loaded_{};
  std::filesystem::path journal_dir_;

  auto load_pictures() -> void;
  auto describe_unlisted() -> void;
  auto save_pictures() const -> void;
  };
