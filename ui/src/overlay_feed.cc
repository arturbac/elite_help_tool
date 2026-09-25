#include <overlay_feed.h>
#include <qformat.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <format>
#include <ranges>

namespace
  {
constexpr uint32_t colour_heading{0x9ad1ffu};
constexpr uint32_t colour_plain{0xddddddu};
constexpr uint32_t colour_alert{0xd9a34au};
///\brief nieodkryte przez nikogo - to jest ten przypadek, dla ktorego warto sie zatrzymac
constexpr uint32_t colour_first{0x3cb371u};

///\brief bloki gasna gdy narzedzie zamilknie - lepiej brak napisu niz napis sprzed godziny
constexpr uint32_t block_ttl_ms{10000u};
///\brief niezmieniony obraz i tak trzeba powtarzac, inaczej wygasnie graczowi stojacemu w miejscu
constexpr std::chrono::seconds heartbeat{3};

///\brief ponizej tego progu schodzenie do ciala nie zwraca sie czasowo
constexpr uint32_t minimum_body_value{300000u};
///\brief pas boczny ma swoje granice, dluga lista i tak nie zostanie przeczytana w locie
constexpr size_t listed_bodies{5u};

[[nodiscard]]
auto same_content(overlay::frame_t const & left, overlay::frame_t const & right) -> bool
  {
  if(left.blocks.size() != right.blocks.size())
    return false;

  for(size_t block{}; block != left.blocks.size(); ++block)
    {
    if(
      left.blocks[block].corner != right.blocks[block].corner
      or left.blocks[block].lines.size() != right.blocks[block].lines.size()
    )
      return false;

    for(size_t line{}; line != left.blocks[block].lines.size(); ++line)
      if(left.blocks[block].lines[line].text != right.blocks[block].lines[line].text)
        return false;
    }
  return true;
  }

///\brief w nazwie ciala gra powtarza nazwe systemu - na pasie bocznym to sama strata miejsca
[[nodiscard]]
auto short_body_name(std::string const & system_name, std::string const & body_name) -> std::string
  {
  if(body_name.size() > system_name.size() + 1u and body_name.starts_with(system_name))
    return body_name.substr(system_name.size() + 1u);
  return body_name;
  }

///\brief warto zejsc tylko po to, czego jeszcze nie zmapowalismy i co cos daje
[[nodiscard]]
auto worth_mapping(body_t const & body) -> bool
  {
  auto const * const planet{std::get_if<planet_details_t>(&body.details)};
  return planet != nullptr and not planet->mapped and body.value >= minimum_body_value;
  }

[[nodiscard]]
auto describe_exploration(star_system_t const & system) -> std::vector<overlay::line_t>
  {
  std::vector<body_t const *> candidates;
  for(body_t const & body: system.bodies)
    if(worth_mapping(body))
      candidates.push_back(&body);

  if(candidates.empty())
    return {};

  std::ranges::sort(candidates, std::ranges::greater{}, [](body_t const * body) { return body->value; });

  uint64_t total{};
  for(body_t const * body: candidates)
    total += body->value;

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = std::format("worth mapping: {} bodies, {} Cr", candidates.size(), format_credits_value(uint32_t(total))),
      .color = colour_heading
    }
  );

  for(body_t const * body: candidates | std::views::take(listed_bodies))
    {
    auto const * const planet{std::get_if<planet_details_t>(&body->details)};
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{}  {} Cr  {:.0f} ls{}",
          short_body_name(system.name, body->name),
          format_credits_value(body->value),
          body->distance_from_arrival_ls,
          planet != nullptr and planet->landable ? "  landable" : ""
        ),
        // pierwsze odkrycie to premia, ktorej nie da sie odzyskac pozniej
        .color = body->was_discovered ? colour_plain : colour_first
      }
    );
    }

  if(candidates.size() > listed_bodies)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", candidates.size() - listed_bodies), .color = colour_plain}
    );

  return lines;
  }

[[nodiscard]]
auto describe_system(star_system_t const & system) -> std::vector<overlay::line_t>
  {
  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = system.name, .color = colour_heading});

  if(not system.controlling_faction.empty())
    lines.push_back(overlay::line_t{.text = system.controlling_faction, .color = colour_plain});

  if(not system.economy.empty() or not system.government.empty())
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{} / {}", system.economy.empty() ? "?" : system.economy, system.government.empty() ? "?" : system.government
        ),
        .color = colour_plain
      }
    );

  if(not system.security.empty())
    lines.push_back(overlay::line_t{.text = std::format("security: {}", system.security), .color = colour_plain});

  // system bez zakonczonego FSS to powod zeby zostac, a nie lecieć dalej
  if(not system.fss_complete and not system.bodies.empty())
    lines.push_back(
      overlay::line_t{
        .text = std::format("FSS incomplete, {} bodies known", system.bodies.size()), .color = colour_alert
      }
    );

  return lines;
  }
  }  // namespace

overlay_feed_t::overlay_feed_t(std::string socket_path) :
    server_{std::make_unique<overlay::server_t>(std::move(socket_path))}
  {
  if(server_->listening())
    spdlog::info("overlay feed listening");
  else
    spdlog::warn("overlay feed could not listen, in-game overlay will stay empty");
  }

[[nodiscard]]
auto overlay_feed_t::listening() const noexcept -> bool
  { return server_->listening(); }

[[nodiscard]]
auto overlay_feed_t::clients() const noexcept -> unsigned
  { return server_->clients(); }

auto overlay_feed_t::publish(current_state_t const & state) -> void
  {
  if(not server_->listening())
    return;

  overlay::frame_t frame{};

  if(not state.system.name.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::top_left, .ttl_ms = block_ttl_ms, .lines = describe_system(state.system)
      }
    );

  if(auto exploration{describe_exploration(state.system)}; not exploration.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_right, .ttl_ms = block_ttl_ms, .lines = std::move(exploration)
      }
    );

  if(not state.next_target.Name.empty() and state.next_target.Name != state.system.name)
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = block_ttl_ms,
        .lines = {overlay::line_t{
          .text = std::format(
            "next: {} ({})",
            state.next_target.Name,
            state.next_target.StarClass.empty() ? "?" : state.next_target.StarClass
          ),
          .color = colour_plain
        }}
      }
    );

  // gra dostaje ramke gdy sie zmienila albo gdy minal czas podtrzymania - nie co zdarzenie z journala
  auto const now{std::chrono::steady_clock::now()};
  if(same_content(frame, last_) and now - last_sent_ < heartbeat)
    return;

  frame.seq = ++sequence_;
  server_->publish(frame);
  last_ = std::move(frame);
  last_sent_ = now;
  }
