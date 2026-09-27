#pragma once

#include "logic.h"

#include <overlay_protocol.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

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

  ///\brief what goes into the frame - the newest request once its moment has come, or none
  [[nodiscard]]
  auto capture_request() -> overlay::capture_t const &;

  ///\brief files the pictures the layer has finished
  ///\returns true when one was added, and the page is worth writing again
  auto collect() -> bool;

  ///\brief writes index.html from the database's finds and the pictures kept
  auto write_page(database_storage_t & db) -> void;

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
  ///\brief a request waiting for the sampler's rings to fade, and when it may go
  overlay::capture_t delayed_;
  std::chrono::steady_clock::time_point due_;
  std::vector<pending_t> pending_;
  std::vector<picture_t> pictures_;
  bool pictures_loaded_{};

  auto load_pictures() -> void;
  auto save_pictures() const -> void;
  };
