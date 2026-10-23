#include <journal_context.h>

#include "json_io_impl.h"

#include <events/event_holder.h>
#include <generic_state.h>

#include <format>
#include <type_traits>
#include <utility>

namespace journal_context
  {
// events the shared variant does not carry, read here for the place alone (glaze reads only types with linkage)
namespace detail
  {
struct touchdown_t
  {
  std::string Body;
  bool PlayerControlled{true};
  };

struct liftoff_t
  {
  std::string Body;
  };

struct load_game_t
  {
  std::string Ship;
  };

struct docking_requested_t
  {
  std::string StationName;
  std::string StationType;
  };

struct body_event_t
  {
  std::string Body;
  };
  }  // namespace detail

namespace
  {
using namespace detail;
using seconds_t = std::chrono::sys_seconds;

///\brief the place as the events move it; with steps set, each event is also told there
class tracker_t final : public generic_state_t
  {
public:
  place_t place;
  seconds_t last{};
  std::vector<step_t> * steps{};
  seconds_t since{};

  tracker_t() : generic_state_t{std::string_view{}} {}

  auto handle(seconds_t timestamp, events::event_holder_t && event) -> void override
    {
    last = timestamp;
    std::visit([this, timestamp]<typename T>(T const & e) { on(timestamp, e); }, event);
    }

  auto raw_line(std::string_view line) -> void override
    {
    std::string const text{line};
    events::generic_event_t g;
    if(eht::json::read_lenient(g, text))
      return;
    auto const read = [&text]<typename T>(T & value) -> bool { return not eht::json::read_lenient(value, text); };
    if(g.event == "Touchdown")
      {
      touchdown_t e;
      if(read(e))
        {
        place.where = where_e::landed;
        if(not e.Body.empty())
          place.body = e.Body;
        tell(g.timestamp, "Touchdown", e.Body);
        }
      }
    else if(g.event == "Liftoff")
      {
      liftoff_t e;
      if(read(e))
        {
        place.where = where_e::normal_space;
        tell(g.timestamp, "Liftoff", e.Body);
        }
      }
    else if(g.event == "LoadGame")
      {
      load_game_t e;
      if(read(e))
        {
        place = place_t{.ship = e.Ship};
        tell(g.timestamp, "LoadGame", e.Ship);
        }
      }
    else if(g.event == "DockingRequested")
      {
      docking_requested_t e;
      if(read(e))
        tell(g.timestamp, "DockingRequested", std::format("{} ({})", e.StationName, e.StationType));
      }
    else if(g.event == "ApproachBody" or g.event == "LeaveBody")
      {
      body_event_t e;
      if(read(e))
        {
        place.body = g.event == "ApproachBody" ? e.Body : std::string{};
        tell(g.timestamp, g.event, e.Body);
        }
      }
    }

private:
  auto tell(seconds_t timestamp, std::string_view event, std::string what) -> void
    {
    if(steps)
      steps->push_back(step_t{.seconds = (timestamp - since).count(), .event = std::string{event}, .what = std::move(what)});
    }

  auto on(seconds_t t, events::location_t const & e) -> void
    {
    place.system = e.StarSystem;
    place.body = e.Body;
    place.station = e.Docked ? e.StationName : std::string{};
    place.where = e.OnFoot ? where_e::on_foot : e.Docked ? where_e::docked : e.Taxi ? where_e::taxi : where_e::normal_space;
    tell(t, "Location", std::format("{}, {}", e.StarSystem, e.Body));
    }

  auto on(seconds_t t, events::fsd_jump_t const & e) -> void
    {
    place.system = e.StarSystem;
    place.body = e.Body;
    place.station.clear();
    place.settlement.clear();
    place.pad = 0;
    place.where = e.Taxi ? where_e::taxi : where_e::supercruise;
    tell(t, "FSDJump", e.StarSystem);
    }

  auto on(seconds_t t, events::supercruise_entry_t const &) -> void
    {
    if(place.where != where_e::taxi)
      place.where = where_e::supercruise;
    place.station.clear();
    place.settlement.clear();
    place.pad = 0;
    tell(t, "SupercruiseEntry", {});
    }

  auto on(seconds_t t, events::supercruise_exit_t const & e) -> void
    {
    if(place.where != where_e::taxi)
      place.where = where_e::normal_space;
    place.body = e.Body;
    tell(t, "SupercruiseExit", std::format("{} ({})", e.Body, e.BodyType));
    }

  auto on(seconds_t t, events::approach_settlement_t const & e) -> void
    {
    place.settlement = e.Name;
    tell(t, "ApproachSettlement", e.Name);
    }

  auto on(seconds_t t, events::docking_granted_t const & e) -> void
    {
    place.station = e.StationName;
    place.station_type = e.StationType;
    place.pad = e.LandingPad;
    tell(t, "DockingGranted", std::format("pad {} at {} ({})", e.LandingPad, e.StationName, e.StationType));
    }

  auto on(seconds_t t, events::docking_cancelled_t const & e) -> void
    {
    place.pad = 0;
    tell(t, "DockingCancelled", e.StationName);
    }

  auto on(seconds_t t, events::docked_t const & e) -> void
    {
    place.system = e.StarSystem;
    place.station = e.StationName;
    place.station_type = e.StationType;
    if(place.where != where_e::taxi)
      place.where = where_e::docked;
    tell(t, "Docked", std::format("{} ({})", e.StationName, e.StationType));
    }

  auto on(seconds_t t, events::undocked_t const & e) -> void
    {
    place.where = e.Taxi ? where_e::taxi : where_e::normal_space;
    place.pad = 0;
    tell(t, "Undocked", e.StationName);
    }

  auto on(seconds_t t, events::disembark_t const & e) -> void
    {
    place.where = where_e::on_foot;
    tell(t, "Disembark", e.StationName);
    }

  auto on(seconds_t t, events::embark_t const & e) -> void
    {
    place.where = e.Taxi ? where_e::taxi : e.SRV ? where_e::srv : not place.station.empty() ? where_e::docked : where_e::landed;
    tell(t, "Embark", e.Taxi ? "taxi" : e.SRV ? "SRV" : "ship");
    }

  auto on(seconds_t t, events::loadout_t const & e) -> void
    {
    place.ship = e.Ship;
    tell(t, "Loadout", e.Ship);
    }

  template<typename T>
  auto on(seconds_t, T const &) -> void
    {
    }
  };

///\brief the line's time, from its own text; false when it has none
auto line_time(std::string_view line, seconds_t & at) -> bool
  {
  events::generic_event_t g;
  if(eht::json::read_lenient(g, std::string{line}))
    return false;
  at = g.timestamp;
  return true;
  }

auto escape(std::string_view s) -> std::string
  {
  std::string out{"\""};
  for(char const c: s)
    if(c == '"' or c == '\\')
      (out += '\\') += c;
    else if(static_cast<unsigned char>(c) >= 0x20u)
      out += c;
  return out += '"';
  }

auto place_json(place_t const & p) -> std::string
  {
  return std::format(R"({{"system": {}, "body": {}, "station": {}, "station_type": {}, "pad": {}, "settlement": {}, )"
                     R"("ship": {}, "where": {}}})",
                     escape(p.system), escape(p.body), escape(p.station), escape(p.station_type), p.pad,
                     escape(p.settlement), escape(p.ship), escape(where_name(p.where)));
  }
  }  // namespace

auto where_name(where_e where) noexcept -> std::string_view
  {
  switch(where)
    {
    using enum where_e;
    case docked:       return "docked";
    case normal_space: return "normal space";
    case supercruise:  return "supercruise";
    case landed:       return "landed";
    case on_foot:      return "on foot";
    case srv:          return "SRV";
    case taxi:         return "taxi";
    case unknown:      break;
    }
  return "unknown";
  }

auto read(std::string_view journal, std::uint64_t offset, std::chrono::seconds window) -> context_t
  {
  tracker_t tracker;
  context_t context;
  std::size_t at{};
  bool after{};
  while(at < journal.size())
    {
    std::size_t const end{journal.find('\n', at)};
    if(end == std::string_view::npos)
      break;  // the game is still writing it
    std::string_view line{journal.substr(at, end - at)};
    if(not line.empty() and line.back() == '\r')
      line.remove_suffix(1);
    bool const past{at >= offset};
    at = end + 1;
    if(line.empty())
      continue;
    if(past and not after)
      {
      after = true;
      context.before = tracker.place;
      if(tracker.last != seconds_t{})
        context.last_before = std::format("{:%FT%TZ}", tracker.last);
      tracker.steps = &context.steps;
      tracker.since = tracker.last;
      }
    if(after)
      {
      seconds_t when{};
      if(line_time(line, when) and tracker.since != seconds_t{} and when - tracker.since > window)
        break;
      }
    tracker.discovery(line);
    }
  if(not after)
    {
    context.before = tracker.place;
    if(tracker.last != seconds_t{})
      context.last_before = std::format("{:%FT%TZ}", tracker.last);
    }
  context.after = tracker.place;
  return context;
  }

auto to_json(context_t const & context) -> std::string
  {
  std::string steps;
  for(step_t const & s: context.steps)
    steps += std::format(R"({}{{"seconds": {}, "event": {}, "what": {}}})", steps.empty() ? "" : ", ", s.seconds,
                         escape(s.event), escape(s.what));
  return std::format(R"({{"last_before": {}, "before": {}, "after": {}, "steps": [{}]}})", escape(context.last_before),
                     place_json(context.before), place_json(context.after), steps);
  }
  }  // namespace journal_context
