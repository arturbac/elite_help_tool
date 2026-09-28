#include <overlay_exploration.h>
#include <eht_settings.h>

#include <algorithm>
#include <format>
#include <limits>
#include <ranges>

namespace overlay_exploration
  {
namespace
  {
  auto colour_heading() -> uint32_t { return eht::settings()->overlay.colours.heading.rgb; }

  auto colour_plain() -> uint32_t { return eht::settings()->overlay.colours.plain.rgb; }

  auto colour_alert() -> uint32_t { return eht::settings()->overlay.colours.alert.rgb; }

  auto colour_first() -> uint32_t { return eht::settings()->overlay.colours.first.rgb; }

  auto colour_below() -> uint32_t { return eht::settings()->exploration.below_worth.rgb; }
  auto colour_knowledge() -> uint32_t { return eht::settings()->exploration.new_knowledge.rgb; }

  ///\brief why one scan here is worth it - empty when the history has seen enough worlds like this one
  [[nodiscard]]
  auto knowledge_note(bio::knowledge_t const & knowledge, std::string_view star_type) -> std::string
    {
    using enum bio::novelty_e;
    switch(knowledge.novelty)
      {
      case never:      return "new: genus";
      case atmosphere: return "new: atmosphere";
      case warmer:     return std::format("new: {:.0f} K warmer", knowledge.beyond);
      case colder:     return std::format("new: {:.0f} K colder", knowledge.beyond);
      case heavier:    return std::format("new: {:.0f}% heavier", knowledge.beyond * 100.0);
      case lighter:    return std::format("new: {:.0f}% lighter", knowledge.beyond * 100.0);
      case star:       return std::format("new: {} star", star_type.substr(0, 1));
      case few:        return std::format("few like it ({})", knowledge.alike);
      case known:      break;
      }
    return {};
    }

  auto bio_worth() -> uint32_t { return eht::settings()->exploration.bio_worth; }

  [[nodiscard]]
  auto short_name(star_system_t const & system, std::string const & body_name) -> std::string
    {
    if(body_name.size() > system.name.size() + 1u and body_name.starts_with(system.name))
      return body_name.substr(system.name.size() + 1u);
    return body_name;
    }

  [[nodiscard]]
  auto planet_of(body_t const & body) -> planet_details_t const *
    { return std::get_if<planet_details_t>(&body.details); }

  ///\brief the star we arrived at - the only one scanned the moment we drop out of the jump
  [[nodiscard]]
  auto arrival_star(star_system_t const & system) -> body_t const *
    {
    body_t const * best{};
    for(body_t const & body: system.bodies)
      if(
        body.body_type() == body_type_e::star
        and (best == nullptr or body.distance_from_arrival_ls < best->distance_from_arrival_ls)
      )
        best = &body;
    return best;
    }

  [[nodiscard]]
  auto bio_signals(planet_details_t const & planet) -> uint32_t
    {
    uint32_t count{};
    for(events::signal_t const & signal: planet.signals_)
      if(signal.Type_Localised.contains("Biological"))
        count += signal.Count;
    return count;
    }

  [[nodiscard]]
  auto has_life(planet_details_t const & planet) -> bool
    { return bio_signals(planet) != 0u or not planet.genuses_.empty(); }

  ///\brief what one genus on one world is, or is likely to be, and what it pays
  struct genus_view_t
    {
    std::string text;
    uint32_t colour{};
    ///\brief the most it is likely to pay - known, guessed or the top of the family's prices
    uint32_t potential{};
    bool done{};
    ///\brief one scan would widen what is known of where the genus grows
    bool novel{};
    };

  [[nodiscard]]
  auto view_genus(
    star_system_t const & system,
    body_t const & body,
    events::genus_t const & genus,
    std::span<bio::species_record_t const> history,
    size_t guesses_shown
  ) -> genus_view_t
    {
    uint32_t const worth{bio_worth()};

    // the species is known once a sample was taken - nothing left to guess
    if(not genus.Species_Localised.empty())
      {
      uint32_t const value{bio::species_value(genus.Species_Localised).value_or(0u)};
      return genus_view_t{
        // known without being done means it was logged at least - the codex has it and its picture; what is
        // missing is only the samples that pay, often not worth the walk for a cheap species
        .text = std::format(
          "{}  {}  {}", genus.Species_Localised, short_credits(value), genus.Sampled ? "done" : "logged"
        ),
        .colour = genus.Sampled ? colour_below() : (value >= worth ? colour_first() : colour_below()),
        .potential = genus.Sampled ? 0u : value,
        .done = genus.Sampled
      };
      }

    std::vector<bio::candidate_t> guesses;
    std::string note;
    auto const world{bio::conditions_of(system, body)};
    if(world)
      {
      guesses = bio::predict(genus.Genus_Localised, *world, history);
      note = knowledge_note(
        bio::knowledge(genus.Genus_Localised, *world, history, eht::settings()->exploration.little_known), world->star_type
      );
      }
    // what the history does not know yet is worth a scan whatever it pays; it stands after the price, in its own colour
    auto const with_note = [&](genus_view_t view) -> genus_view_t
    {
      if(note.empty())
        return view;
      view.text += "  " + note;
      view.novel = true;
      if(view.colour != colour_first())
        view.colour = colour_knowledge();
      return view;
    };

    if(guesses.empty() or guesses.front().fit == bio::fit_e::unlike)
      {
      // never sampled under this sky - the family's whole price span is all there is to go by
      auto const range{organic_value_range(genus.Genus_Localised)};
      uint32_t const top{range ? range->second : 0u};
      return with_note(genus_view_t{
        .text = range
                  ? std::format(
                      "{} ?  {}-{}", genus.Genus_Localised, short_credits(range->first), short_credits(range->second)
                    )
                  : std::format("{} ?", genus.Genus_Localised),
        .colour = top >= worth ? colour_alert() : colour_below(),
        .potential = top,
        .done = false
      });
      }

    std::string text{std::format("{} ?", genus.Genus_Localised)};
    size_t const shown{std::max<size_t>(1u, guesses_shown)};
    for(auto const & [index, guess]: guesses | std::views::take(shown) | std::views::enumerate)
      {
      // the family's name is already in front - "Stratum ? Tectonicas" reads better than the name twice
      std::string_view kind{guess.species};
      if(kind.starts_with(genus.Genus_Localised) and kind.size() > genus.Genus_Localised.size() + 1u)
        kind.remove_prefix(genus.Genus_Localised.size() + 1u);
      text += std::format(
        "{}{} {}{}",
        index == 0 ? "  " : " | ",
        kind,
        short_credits(guess.value),
        guess.fit == bio::fit_e::fits ? std::format(" {:.0f}%", guess.share * 100.0) : std::string{" ~"}
      );
      }

    bio::candidate_t const & best{guesses.front()};
    // a likely species above the line is worth the landing; a good one merely possible is worth a look
    bool const any_worth{std::ranges::any_of(guesses, [worth](auto const & g) { return g.value >= worth; })};
    uint32_t colour{colour_below()};
    if(best.fit == bio::fit_e::fits and best.value >= worth)
      colour = colour_first();
    else if(any_worth)
      colour = colour_alert();
    uint32_t potential{};
    for(bio::candidate_t const & guess: guesses)
      potential = std::max(potential, guess.value);
    return with_note(genus_view_t{.text = std::move(text), .colour = colour, .potential = potential, .done = false});
    }

  ///\brief the most a body's life is likely to pay, to put the best landing first
  [[nodiscard]]
  auto body_potential(star_system_t const & system, body_t const & body, std::span<bio::species_record_t const> history)
    -> uint32_t
    {
    uint32_t best{};
    if(auto const * const planet{planet_of(body)}; planet != nullptr)
      for(events::genus_t const & genus: planet->genuses_)
        best = std::max(best, view_genus(system, body, genus, history, 1u).potential);
    return best;
    }
  }  // namespace

auto short_credits(uint64_t value) -> std::string
  {
  if(value >= 1'000'000u)
    return std::format("{:.1f}M", double(value) / 1'000'000.0);
  if(value >= 1'000u)
    return std::format("{}k", (value + 500u) / 1'000u);
  return std::format("{}", value);
  }

auto describe_arrival(star_system_t const & system) -> std::vector<overlay::line_t>
  {
  // in inhabited space everything was found long ago, and saying so at every jump is noise
  if(system.name.empty() or system.population != 0u)
    return {};

  std::vector<overlay::line_t> lines;
  body_t const * const star{arrival_star(system)};
  if(star == nullptr)
    lines.push_back(
      overlay::line_t{.text = std::format("{}: star not scanned yet", system.name), .color = colour_plain()}
    );
  else
    {
    auto const & details{std::get<star_details_t>(star->details)};
    lines.push_back(
      star->was_discovered
        ? overlay::
            line_t{.text = std::format("{}: {} discovered before - jump on", system.name, details.star_type), .color = colour_below()}
        : overlay::line_t{
            .text = std::format("{}: {} UNDISCOVERED", system.name, details.star_type), .color = colour_first()
          }
    );
    // the game says nothing of where its exclusion zone ends, and the distance to the star is only on the
    // HUD - the star's own size, in the HUD's unit, is the scale to read that distance against
    if(star->radius > 0.0)
      {
      constexpr double metres_per_ls{299'792'458.0};
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  star radius {:.0f} km = {} Ls",
            star->radius / 1'000.0,
            star->radius / metres_per_ls < 0.1 ? std::format("{:.3f}", star->radius / metres_per_ls)
                                               : std::format("{:.2f}", star->radius / metres_per_ls)
          ),
          .color = colour_plain()
        }
      );
      }
    }

  size_t scanned{};
  uint64_t worth{};
  for(body_t const & body: system.bodies)
    {
    // the belt's clusters come as scans of their own, but the honk counts only stars and planets
    if(body.name.contains("Belt Cluster"))
      continue;
    ++scanned;
    worth += body.value;
    }

  if(system.fss_complete or (system.body_count != 0u and scanned >= system.body_count))
    lines.push_back(
      overlay::line_t{
        .text = std::format("all {} bodies scanned, ~{} mapped", scanned, short_credits(worth)), .color = colour_plain()
      }
    );
  else if(system.body_count == 0u)
    lines.push_back(overlay::line_t{.text = "no discovery scan yet - honk", .color = colour_alert()});
  else
    lines.push_back(
      overlay::line_t{
        .text = std::format("FSS {}/{} bodies, ~{} so far", scanned, system.body_count, short_credits(worth)),
        .color = colour_alert()
      }
    );

  return lines;
  }

auto describe_mapping(star_system_t const & system) -> std::vector<overlay::line_t>
  {
  auto const cfg{eht::settings()};
  uint32_t const minimum{cfg->overlay.minimum_body_value};
  size_t const listed{cfg->overlay.lists.bodies};

  std::vector<body_t const *> candidates;
  size_t mapped{};
  for(body_t const & body: system.bodies)
    if(auto const * const planet{planet_of(body)}; planet != nullptr and body.value >= minimum)
      {
      if(planet->mapped)
        ++mapped;
      else
        candidates.push_back(&body);
      }

  if(candidates.empty())
    return {};

  std::ranges::sort(candidates, std::ranges::greater{}, [](body_t const * body) { return body->value; });

  uint64_t total{};
  for(body_t const * body: candidates)
    total += body->value;

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = mapped != 0u
                ? std::format("to map: {} bodies, {} ({} mapped)", candidates.size(), short_credits(total), mapped)
                : std::format("to map: {} bodies, {}", candidates.size(), short_credits(total)),
      .color = colour_heading()
    }
  );

  for(body_t const * body: candidates | std::views::take(listed))
    {
    auto const * const planet{planet_of(*body)};
    uint32_t const life{bio_signals(*planet)};
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{}  {}  {:.0f} ls{}{}{}",
          short_name(system, body->name),
          short_credits(body->value),
          body->distance_from_arrival_ls,
          planet->terraform_state != events::terraform_state_e::none ? "  terraformable" : "",
          planet->landable ? "  landable" : "",
          life != 0u ? std::format("  bio {}", life) : std::string{}
        ),
        // a first discovery is a bonus that cannot be had again later
        .color = body->was_discovered or planet->was_mapped ? colour_plain() : colour_first()
      }
    );
    }

  if(candidates.size() > listed)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", candidates.size() - listed), .color = colour_plain()}
    );
  return lines;
  }

auto describe_life(star_system_t const & system, std::span<bio::species_record_t const> history)
  -> std::vector<overlay::line_t>
  {
  struct entry_t
    {
    body_t const * body;
    uint32_t potential;
    };

  std::vector<entry_t> bodies;
  for(body_t const & body: system.bodies)
    if(auto const * const planet{planet_of(body)}; planet != nullptr and has_life(*planet))
      bodies.push_back(entry_t{.body = &body, .potential = body_potential(system, body, history)});

  if(bodies.empty())
    return {};

  std::ranges::sort(bodies, std::ranges::greater{}, &entry_t::potential);
  uint32_t const worth{bio_worth()};

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = std::format("life: {} bodies, worth landing from {}", bodies.size(), short_credits(worth)),
      .color = colour_heading()
    }
  );

  size_t const listed{eht::settings()->exploration.bio_bodies};
  for(entry_t const & entry: bodies | std::views::take(listed))
    {
    body_t const & body{*entry.body};
    auto const * const planet{planet_of(body)};
    uint32_t const signals{bio_signals(*planet)};

    std::vector<genus_view_t> genera;
    for(events::genus_t const & genus: planet->genuses_)
      genera.push_back(view_genus(system, body, genus, history, eht::settings()->exploration.candidates));
    bool const all_done{not genera.empty() and std::ranges::all_of(genera, &genus_view_t::done)};

    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{}  {} signals  {:.0f} ls  {:.2f} g  {:.0f} K{}{}",
          short_name(system, body.name),
          std::max<size_t>(signals, genera.size()),
          body.distance_from_arrival_ls,
          planet->surface_gravity / 9.80665,
          planet->surface_temperature,
          planet->atmosphere_type.empty() or planet->atmosphere_type == "None"
            ? std::string{}
            : std::format("  {}", planet->atmosphere_type),
          all_done ? "  all sampled" : ""
        ),
        .color = all_done                    ? colour_below()
                 : entry.potential >= worth    ? colour_first()
                 : std::ranges::any_of(genera, &genus_view_t::novel) ? colour_knowledge()
                                               : colour_plain()
      }
    );

    if(genera.empty())
      {
      // the scanner found life but not which - the probes tell the genera
      lines.push_back(overlay::line_t{.text = "    map it to learn the genera", .color = colour_plain()});
      continue;
      }
    if(all_done)
      continue;
    for(genus_view_t & view: genera)
      if(not view.done)
        lines.push_back(overlay::line_t{.text = "    " + view.text, .color = view.colour});
    }

  if(bodies.size() > listed)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", bodies.size() - listed), .color = colour_plain()}
    );
  return lines;
  }

auto describe_sampling(
  star_system_t const & system,
  current_state_t::organic_sampling_t const & sampling,
  surface_view_t const & surface,
  std::span<bio::species_record_t const> history
) -> std::vector<overlay::line_t>
  {
  if(surface.body_name.empty())
    return {};
  // the state keeps bodies by the name without the system's in front, Status.json writes it in full
  auto const body_it{system.body_by_name(short_name(system, surface.body_name))};
  if(body_it == system.bodies.end())
    return {};
  body_t const & body{*body_it};
  auto const * const planet{planet_of(body)};
  if(planet == nullptr)
    return {};

  bool const sampling_here{
    sampling.samples != 0u and sampling.system_address == system.system_address and sampling.body == body.body_id
  };
  // on the ground or low over it with life to take, or with the sampler out - otherwise nothing to say
  if(not sampling_here and not(has_life(*planet) and (surface.here or surface.sampler_in_hand)))
    return {};

  uint32_t const worth{bio_worth()};
  std::vector<overlay::line_t> lines;

  if(sampling_here)
    {
    uint32_t const value{bio::species_value(sampling.species).value_or(0u)};
    uint32_t const colour{value >= worth ? colour_first() : colour_below()};
    if(sampling.analysed)
      lines.push_back(
        overlay::line_t{.text = std::format("{}  {}  done", sampling.species, short_credits(value)), .color = colour}
      );
    else
      {
      lines.push_back(
        overlay::line_t{
          // the colour says whether it pays the landing - a head-up readout has no room for the words
          .text = std::format("{}  {}  {}/3", sampling.species, short_credits(value), sampling.samples),
          .color = colour
        }
      );

      // the codex bonus goes to the first to log it where nobody set foot before
      if(not planet->was_footfalled and not sampling.was_logged)
        lines.push_back(
          overlay::line_t{
            .text = std::format("first footfall: x5 = {}", short_credits(uint64_t{value} * 5u)), .color = colour_plain()
          }
        );

      if(sampling.samples < 3u)
        {
        uint32_t const range{bio::colony_range_m(sampling.genus)};
        if(sampling.points.empty() or not surface.here or surface.planet_radius <= 0.0)
          lines.push_back(
            overlay::line_t{
              .text = std::format("colony {} m - places of the earlier samples unknown", range), .color = colour_plain()
            }
          );
        else
          {
          // every earlier sample has to be a colony's range away, so the nearest one is all that decides
          double nearest{std::numeric_limits<double>::max()};
          for(bio::surface_point_t const & point: sampling.points)
            nearest = std::min(nearest, bio::surface_distance_m(point, *surface.here, surface.planet_radius));
          bool const far_enough{nearest >= double(range)};
          lines.push_back(
            overlay::line_t{
              .text = std::format("{}: {:.0f} of {} m", far_enough ? "sample" : "too close", nearest, range),
              .color = far_enough ? colour_first() : colour_alert()
            }
          );
          }
        }
      }
    }

  // what else grows here, the best first - the choice of what to walk to next
  std::vector<genus_view_t> left;
  for(events::genus_t const & genus: planet->genuses_)
    if(not(sampling_here and genus.Genus_Localised == sampling.genus))
      // head-up there is room for the likeliest guess only
      if(genus_view_t view{view_genus(system, body, genus, history, 1u)}; not view.done)
        left.push_back(std::move(view));
  std::ranges::sort(left, std::ranges::greater{}, &genus_view_t::potential);

  if(not left.empty())
    {
    lines.push_back(overlay::line_t{.text = "left here:", .color = colour_heading()});
    for(genus_view_t & view: left)
      lines.push_back(overlay::line_t{.text = "  " + view.text, .color = view.colour});
    }
  else if(not sampling_here and bio_signals(*planet) > planet->genuses_.size())
    lines.push_back(
      overlay::line_t{
        .text = std::format("{} signals, genera unknown - map the body", bio_signals(*planet)), .color = colour_plain()
      }
    );

  return lines;
  }
  }  // namespace overlay_exploration
