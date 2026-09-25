#include <overlay_feed.h>
#include <qformat.h>

#include <spdlog/spdlog.h>

#include <format>

namespace
  {
constexpr uint32_t colour_heading{0x9ad1ffu};
constexpr uint32_t colour_plain{0xddddddu};
constexpr uint32_t colour_alert{0xd9a34au};

///\brief bloki gasna gdy narzedzie zamilknie - lepiej brak napisu niz napis sprzed godziny
constexpr uint32_t block_ttl_ms{10000u};
///\brief niezmieniony obraz i tak trzeba powtarzac, inaczej wygasnie graczowi stojacemu w miejscu
constexpr std::chrono::seconds heartbeat{3};

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
