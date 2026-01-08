#include <database_import_state.h>
#include <spdlog/spdlog.h>
#include <simple_enum/std_format.hpp>
#include <stralgo/stralgo.h>
using namespace std::string_view_literals;

namespace
  {
template<typename... Args>
void critical_abort(std::format_string<Args...> fmt, Args &&... args)
  {
  spdlog::default_logger_raw()->debug(fmt, std::forward<Args>(args)...);
  std::abort();
  }

void process_factions(database_storage_t & db, std::span<events::faction_info_t> factions)
  {
  for(events::faction_info_t & f: factions)
    {
    info::faction_info_t new_faction_data{info::to_native(std::move(f))};
    auto res{db.load_faction(new_faction_data.name)};
    if(not res)
      critical_abort("failed to load cation info for {}", new_faction_data.name);
    if(not *res)
      {
      // no data add
      spdlog::info("adding faction {}", new_faction_data.name);
      if(auto updres{db.update_faction_info(new_faction_data)}; not updres)
        critical_abort("failed to add faction data for {}", new_faction_data.name);
      }
    else
      {
      info::faction_info_t old_faction_data{std::move(**res)};
      if(old_faction_data != new_faction_data)
        {
        new_faction_data.oid = old_faction_data.oid;
        spdlog::info("updating faction {}", new_faction_data.name);
        if(auto updres{db.update_faction_info(new_faction_data)}; not updres)
          critical_abort("failed to update faction data for {}", new_faction_data.name);
        }
      }
    }
  }
  }  // namespace

void database_import_state_t::handle(std::chrono::sys_seconds timestamp, events::event_holder_t && e)
  {
  state_t & state{*this->state};
  std::visit(
    [&state]<typename T>(T & event)
    {
      if constexpr(std::same_as<T, events::start_jump_t>)
        {
        if(event.JumpType == events::jump_type_e::Hyperspace)
          {
          spdlog::info("jump to [{}] {}", *event.StarClass, *event.StarSystem);
          auto res{state.db_.load_system(*event.SystemAddress)};
          if(not res) [[unlikely]]
            critical_abort("error loading system {} {}", *event.SystemAddress, *event.StarSystem);

          std::optional loaded{std::move(*res)};
          if(loaded)
            {
            state.system = std::move(*loaded);
            spdlog::info(
              "system loaded [{}] {} bodies:{}", state.system.star_type, state.system.name, state.system.bodies.size()
            );
            }
          else
            {
            state.system = star_system_t{
              .system_address = *event.SystemAddress,
              .name = *event.StarSystem,
              .star_type = *event.StarClass,
              .system_location = {},
              .bary_centre = {},
              .bodies = {},
              .sub_class = {},
              .fss_complete = {}
            };
            if(auto res2{state.db_.store(state.system)}; not res2) [[unlikely]]
              critical_abort("error string system {} {}", *event.SystemAddress, *event.StarSystem);
            }
          }
        }
      else if constexpr(std::same_as<T, events::location_t>)
        {
        // after reloading game start at this system
        spdlog::info("location {}: {}", event.SystemAddress, event.StarSystem);
        auto res{state.db_.load_system(event.SystemAddress)};
        if(not res) [[unlikely]]
          critical_abort("error loading system {} {}", event.SystemAddress, event.StarSystem);

        state.buffered_signals.clear();
        if(std::optional loaded{std::move(*res)}; loaded)
          {
          state.system = std::move(*loaded);
          spdlog::info(
            "system loaded [{}] {} bodies:{}", state.system.star_type, state.system.name, state.system.bodies.size()
          );
          }
        else
          {
          state.system = star_system_t{
            .system_address = event.SystemAddress,
            .name = event.StarSystem,
            .star_type = {},  //*event.StarClass,
            .system_location = event.StarPos,
            .bary_centre = {},
            .bodies = {},
            .sub_class = {},
            .fss_complete = {}
          };
          if(auto res2{state.db_.store(state.system)}; not res2) [[unlikely]]
            critical_abort("error string system {} {}", event.SystemAddress, event.StarSystem);
          }
        // add/update factions database
        if(not event.Factions.empty())
          process_factions(state.db_, event.Factions);
        }
      else if constexpr(std::same_as<T, events::fsd_jump_t>)
        {
        if(state.system.system_address != event.SystemAddress)
          critical_abort("jump without start jump {}", state.system.system_address, event.SystemAddress);
        else if(state.system.system_location != event.StarPos)
          {
          state.system.system_location = event.StarPos;
          if(auto res{state.db_.store_system_location(state.system.system_address, state.system.system_location)};
             not res)
            critical_abort("failed to store system location {}", state.system.system_address);
          }
        // add/update factions database
        if(not event.Factions.empty())
          process_factions(state.db_, event.Factions);
        }
      else if constexpr(std::same_as<T, events::fss_discovery_scan_t>)
        {
        spdlog::info("discovery system {} body:{} nonbody:{}", event.SystemName, event.BodyCount, event.NonBodyCount);
        state.system.bodies.reserve(event.BodyCount);
        }
      else if constexpr(std::same_as<T, events::scan_detailed_scan_t>)
        {
        if(state.system.fss_complete)
          return;

        if(auto it{std::ranges::find(state.system.bodies, event.BodyID, body_body_id_proj)};
           it != state.system.bodies.end())
          {
          spdlog::info("already have fss scan for body [{}]{} ", event.BodyID, event.BodyName);
          return;
          }

        state.system.bodies.emplace_back(to_body(std::move(event)));
        body_t & body{state.system.bodies.back()};
        body.value = exploration::aprox_value(body);

        std::visit(
          [&body, &state]<typename U>(U & details)
          {
            if constexpr(std::same_as<U, planet_details_t>)
              {
              if(auto it{
                   std::ranges::find(state.buffered_signals, body.body_id, [](auto const & bs) { return bs.body_id; })
                 };
                 state.buffered_signals.end() != it)
                {
                details.signals_ = std::move(it->signals_);
                details.genuses_ = std::move(it->genuses_);
                state.buffered_signals.erase(it);
                spdlog::info("signals attached to body late");
                }
              spdlog::info(
                "[{}]{} {} {} {}{}{}{} ",
                body.body_id,
                body.name,
                details.terraform_state,
                details.planet_class,
                details.atmosphere,
                body.was_discovered ? " was discovered" : "",
                details.was_mapped ? " was mapped" : "",
                details.was_footfalled ? " was footfalled" : ""
              );
              }
            else
              spdlog::info("{}{}", body.name, body.was_discovered ? " was_discovered" : "");
          },
          body.details
        );

        if(auto res{state.db_.store(state.system.system_address, body)}; not res)
          critical_abort("failed to store body {}: {}", state.system.system_address, body.name);

        // handle rings
        if(not event.Rings.empty())
          {
          std::vector<ring_t> rings;
          std::ranges::transform(
            event.Rings,
            std::back_inserter(rings),
            [&event](events::ring_t & ring) -> ring_t
            {
              return ring_t{
                .name = std::string(stralgo::right(ring.Name, 6)),
                .ring_class = ring.RingClass,
                .mass_mt = ring.MassMT,
                .inner_rad = ring.InnerRad,
                .outer_rad = ring.OuterRad,
                .parent_body_id = event.BodyID,
                .body_id = -1
              };
            }
          );
          if(auto res{state.db_.store(state.system.system_address, rings)}; not res)
            critical_abort("failed to store rings for {}: {}", state.system.system_address, body.name);

          state.system.rings.insert(state.system.rings.end(), rings.begin(), rings.end());
          }
        }
      else if constexpr(std::same_as<T, events::scan_bary_centre_t>)
        {
        state.system.bary_centre.emplace_back(
          bary_centre_t{
            .body_id = event.BodyID,
            .semi_major_axis = event.SemiMajorAxis,
            .eccentricity = event.Eccentricity,
            .orbital_inclination = event.OrbitalInclination,
            .periapsis = event.Periapsis,
            .orbital_period = event.OrbitalPeriod,
            .ascending_node = event.AscendingNode,
            .mean_anomaly = event.MeanAnomaly
          }
        );
        auto const & bc{state.system.bary_centre.back()};
        if(auto res{state.db_.store(state.system.system_address, bc)}; not res)
          critical_abort("failed to store bary_centre {}: {}", state.system.system_address, bc.body_id);
        }
      else if constexpr(std::same_as<T, events::fss_body_signals_t>)
        {
        spdlog::info("Signals {}", event.BodyName);
        for(events::signal_t const & signal: event.Signals)
          spdlog::info("   {}: {}", signal.Type_Localised, signal.Count);

        auto it{state.system.body_by_id(event.BodyID)};
        if(it != state.system.bodies.end())
          {
          planet_details_t & details{std::get<planet_details_t>(it->details)};
          details.signals_ = std::move(event.Signals);

          if(auto res{state.db_.store(state.system.system_address, event.BodyID, details.signals_)}; not res)
            critical_abort("failed to store signals for {}: {}", state.system.system_address, event.BodyID);
          }
        else
          {
          spdlog::info("buffering signals for {}: {}", state.system.system_address, event.BodyID);
          state.buffered_signals.emplace_back(event.BodyID, std::move(event.Signals));
          }
        }
      else if constexpr(std::same_as<T, events::dss_body_signals_t>)
        {
        if(stralgo::ends_with(event.BodyName, "Ring"sv))
          {
          if(auto it{state.system.ring_by_id(event.BodyID)}; it != state.system.rings.end())
            {
            ring_t & ring{*it};
            ring.signals_ = std::move(event.Signals);
            if(auto res{state.db_.store(state.system.system_address, event.BodyID, ring.signals_)}; not res)
              critical_abort("failed to store signals for ring {}: {}", state.system.system_address, event.BodyID);
            }
          else
            critical_abort(
              "ring was not found for {}: {} {}", state.system.system_address, event.BodyID, event.BodyName
            );
          }
        else if(auto it{state.system.body_by_id(event.BodyID)}; it != state.system.bodies.end())
          {
          planet_details_t & details{std::get<planet_details_t>(it->details)};
          if(details.signals_.size() != event.Signals.size())
            {
            details.signals_ = std::move(event.Signals);
            if(auto res{state.db_.store(state.system.system_address, event.BodyID, details.signals_)}; not res)
              critical_abort("failed to store signals for {}: {}", state.system.system_address, event.BodyID);
            }
          if(details.genuses_.size() != event.Genuses.size())
            {
            details.genuses_ = std::move(event.Genuses);
            if(auto res{state.db_.store(state.system.system_address, event.BodyID, details.genuses_)}; not res)
              critical_abort("failed to store genuses_ for {}: {}", state.system.system_address, event.BodyID);
            }
          }
        else
          {
          spdlog::info("buffering signals for {}: {}", state.system.system_address, event.BodyID);
          state.buffered_signals.emplace_back(event.BodyID, std::move(event.Signals), std::move(event.Genuses));
          }
        }
      else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>)
        {
        spdlog::info("fss scan complete");
        state.system.fss_complete = true;
        if(auto res{state.db_.store_fss_complete(state.system.system_address)}; not res) [[unlikely]]
          critical_abort("failed to update fss scan complete for {}", state.system.system_address);
        }
      else if constexpr(std::same_as<T, events::saa_scan_complete_t>)
        {
        spdlog::info("saa scan complete for {}", event.BodyName);

        if(stralgo::ends_with(event.BodyName, "Ring"sv))
          {
          // we got BodyID for ring, unknown at fss
          std::string_view planet_name{planet_name_from_ring_name(state.system.name, event.BodyName)};
          std::string_view ring_name{stralgo::right(event.BodyName, 6)};
          spdlog::warn("planet[{}] ring[{}]", planet_name, ring_name);
          if(auto it{state.system.body_by_name(planet_name)}; it != state.system.bodies.end())
            {
            events::body_id_t const parent_planet_id{it->body_id};
            if(auto res{
                 state.db_.store_ring_body_id(state.system.system_address, parent_planet_id, ring_name, event.BodyID)
               };
               not res) [[unlikely]]
              critical_abort("failed to update ring body id for {}:{}", state.system.system_address, event.BodyName);

            if(auto itr{std::ranges::find_if(
                 state.system.rings,
                 [&parent_planet_id, &ring_name](ring_t const & ring) noexcept -> bool
                 { return ring.parent_body_id == parent_planet_id and ring_name == ring.name; }
               )};
               itr != state.system.rings.end())
              itr->body_id = event.BodyID;
            else
              critical_abort(
                "failed to update (runtime) ring body id for {}:{}", state.system.system_address, event.BodyName
              );
            }
          else
            spdlog::error(
              "failed to find body for ring {}:{}, system not scanned", state.system.system_address, event.BodyName
            );
          }
        else if(auto it{state.system.body_by_id(event.BodyID)}; it != state.system.bodies.end())
          {
          planet_details_t & details{std::get<planet_details_t>(it->details)};
          details.mapped = true;
          if(auto res{state.db_.store_dss_complete(state.system.system_address, event.BodyID)}; not res) [[unlikely]]
            critical_abort("failed to update dss scan complete for {}:{}", state.system.system_address, event.BodyID);
          }
        }

      else if constexpr(std::same_as<T, events::mission_accepted_t>)
        {
        info::mission_t mission{
          .mission_id = event.MissionID,
          .status = info::mission_status_e::accepted,
          .expiry = event.Expiry,
          .faction = event.Faction,
          .type = event.Name,
          .description = event.LocalisedName,
          .reward = event.Reward,
          .target = event.Target,
          .target_type = event.TargetType_Localised,
          .target_faction = event.TargetFaction,
          .destination_system = event.DestinationSystem,
          .destination_station = event.DestinationStation,
          .destination_settlement = event.DestinationSettlement,
          .count = event.Count,
          .kill_count = event.KillCount,
          .passenger_count = event.PassengerCount
        };
        if(auto res{state.db_.store(mission)}; not res) [[unlikely]]
          critical_abort("failed to store mission details for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::mission_completed_t>)
        {
        if(auto res{state.db_.change_mission_status(event.MissionID, info::mission_status_e::completed)}; not res)
          [[unlikely]]
          critical_abort("failed to change mission status for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::mission_abandoned_t>)
        {
        if(auto res{state.db_.change_mission_status(event.MissionID, info::mission_status_e::abandoned)}; not res)
          [[unlikely]]
          critical_abort("failed to change mission status for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::mission_failed_t>)
        {
        if(auto res{state.db_.change_mission_status(event.MissionID, info::mission_status_e::failed)}; not res)
          [[unlikely]]
          critical_abort("failed to change mission status for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::mission_redirected_t>)
        {
        if(auto res{state.db_.redirect_mission(
             event.MissionID, event.NewDestinationSystem, event.NewDestinationStation, event.NewDestinationSettlement
           )};
           not res) [[unlikely]]
          critical_abort("failed to change mission status for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::missions_t>)
        {
        for(events::mission_failed_t const & mission: event.Failed)
          if(auto res{state.db_.change_mission_status(mission.MissionID, info::mission_status_e::failed)}; not res)
            [[unlikely]]
            spdlog::warn("failed to change mission status for {}", mission.MissionID);
        for(events::mission_completed_t const & mission: event.Complete)
          if(auto res{state.db_.change_mission_status(mission.MissionID, info::mission_status_e::completed)}; not res)
            [[unlikely]]
            spdlog::warn("failed to change mission status for {}", mission.MissionID);
        }
      else if constexpr(std::same_as<T, events::nav_route_t>)
        {
        // ignored in import
        }
      else if constexpr(std::same_as<T, events::nav_route_clear_t>)
        {
        // ignored in import
        }
    },
    e
  );
  }
