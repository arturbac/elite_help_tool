#pragma once
#include <events/event_holder.h>
#include <chrono>
#include <string>
#include <string_view>

// what reads the journal line by line and hands each event on

struct generic_state_t
  {
  std::string journal_dir_path_;

  generic_state_t(std::string_view journal_dir_path) : journal_dir_path_{journal_dir_path} {}

  virtual ~generic_state_t();

  auto discovery(std::string_view input) -> void;
  virtual auto handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) -> void = 0;
  ///\brief every line as the game wrote it, before any of it is read - for what needs the event whole
  virtual auto raw_line(std::string_view) -> void {}
  };
