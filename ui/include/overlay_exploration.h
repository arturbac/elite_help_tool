#pragma once

#include "logic.h"

#include <biology.h>
#include <overlay_protocol.h>

#include <optional>
#include <span>
#include <string>
#include <vector>

///\brief what the overlay says while exploring, stage by stage
///
/// The work in a new system goes in a fixed order, and each stage has one question to answer: is the
/// system new at all (the arrival star says it), how much of it is still dark (the honk against the
/// scans), what is worth mapping, where is life worth landing for, and - on the ground - where the next
/// sample may be taken. Every block here answers one of those, and only while it is still open.
namespace overlay_exploration
  {
///\brief where the commander stands, as Status.json has it
struct surface_view_t
  {
  ///\brief the body we are at, by its full name; empty away from any
  std::string body_name;
  ///\brief the place on its surface, only near it or on it
  std::optional<bio::surface_point_t> here;
  double planet_radius{};
  ///\brief the genetic sampler is what the commander holds
  bool sampler_in_hand{};
  };

///\brief credits the short way - 19.0M, 850k - a side band has no room for seven digits
[[nodiscard]]
auto short_credits(uint64_t value) -> std::string;

///\brief the arrival: whether anyone was here before, and how much of the system the scans have lit
[[nodiscard]]
auto describe_arrival(star_system_t const & system) -> std::vector<overlay::line_t>;

///\brief a notable stellar phenomenon in the system, and whether it was ever visited - lost among the
/// mining sites and installations of the FSS otherwise; shown in inhabited space too
[[nodiscard]]
auto describe_phenomena(star_system_t const & system) -> std::vector<overlay::line_t>;

///\brief the bodies worth the probes, best first
[[nodiscard]]
auto describe_mapping(star_system_t const & system) -> std::vector<overlay::line_t>;

///\brief the bodies with life, and what each genus is likely to be worth there
[[nodiscard]]
auto describe_life(star_system_t const & system, std::span<bio::species_record_t const> history)
  -> std::vector<overlay::line_t>;

///\brief on the ground: the sample in progress, how far the last ones are, and what is left on this body
///\detail empty when we stand on no body with life and hold no sampler - it is a head-up readout, and a
/// readout that stays up when there is nothing to read is only in the way
[[nodiscard]]
auto describe_sampling(
  star_system_t const & system,
  current_state_t::organic_sampling_t const & sampling,
  surface_view_t const & surface,
  std::span<bio::species_record_t const> history
) -> std::vector<overlay::line_t>;
  }  // namespace overlay_exploration
