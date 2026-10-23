#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

///\brief where the commander was at a place in a journal, and what came next - read from the journal alone, its events
/// one line at a time through generic_state_t, the way the tailer and the window read them
///\detail the place is a byte of the journal: edworld_eht writes the journal's size beside each hologram model it samples
/// (sample_context.h), so a sample is looked up here afterwards. The state before is everything the journal said up to
/// that byte; the steps after are the events within the window that follow it, on the journal's own clock
namespace journal_context
  {
enum struct where_e : std::uint8_t
  {
  unknown,
  docked,
  normal_space,
  supercruise,
  landed,
  on_foot,
  srv,
  taxi
  };

[[nodiscard]]
auto where_name(where_e where) noexcept -> std::string_view;

struct place_t
  {
  std::string system;
  ///\brief the body last named: a supercruise exit, an approach, a touchdown's, the location's
  std::string body;
  ///\brief docked at, or the port the tower gave a pad at
  std::string station;
  std::string station_type;
  ///\brief the pad given and not yet left, 0 when none
  std::uint32_t pad{};
  std::string settlement;
  std::string ship;
  where_e where{where_e::unknown};
  };

///\brief one event after the place: its seconds after the last event before it, and what it said
struct step_t
  {
  std::int64_t seconds{};
  std::string event;
  std::string what;
  };

struct context_t
  {
  ///\brief the journal's time of the last event before the place, "" when none
  std::string last_before;
  place_t before;
  place_t after;
  std::vector<step_t> steps;
  };

///\brief the journal's text read up to `offset` bytes for the state before it, and on for `window` past its last event
/// before it; lines the journal cannot have written whole are skipped
[[nodiscard]]
auto read(std::string_view journal, std::uint64_t offset, std::chrono::seconds window) -> context_t;

///\brief the context as one JSON object
[[nodiscard]]
auto to_json(context_t const & context) -> std::string;
  }  // namespace journal_context
