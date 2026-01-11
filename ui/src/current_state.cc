#include "logic.h"
#include <main_window.h>
#include <spdlog/spdlog.h>
#include <stralgo/stralgo.h>
using namespace std::string_view_literals;

static auto new_system_def(uint64_t system_address, std::string_view name, std::string_view star_type)
  {
  return star_system_t{
    .system_address = system_address,
    .name = std::string(name),
    .star_type = std::string(star_type),
    .bary_centre = {},
    .bodies = {},
    .fss_complete = {}
  };
  }

auto process_factions(database_storage_t & db, std::span<events::faction_info_t> factions)
  -> std::vector<info::faction_info_t>
  {
  std::vector<info::faction_info_t> result;
  result.reserve(factions.size());

  for(events::faction_info_t & f: factions)
    {
    info::faction_info_t new_faction_data{info::to_native(std::move(f))};
    auto res{db.load_faction(new_faction_data.name)};
    if(not res)
      spdlog::error("failed to load faction info for {}", new_faction_data.name);
    else if(not *res)
      {
      // no data add
      spdlog::info("adding faction {}", new_faction_data.name);
      if(auto updres{db.update_faction_info(new_faction_data)}; not updres)
        spdlog::error("failed to add faction data for {}", new_faction_data.name);
      auto oidres{db.faction_oid(new_faction_data.name)};
      if(oidres and *oidres)
        new_faction_data.oid = int32_t(**oidres);
      }
    else
      {
      info::faction_info_t old_faction_data{std::move(**res)};
      new_faction_data.oid = old_faction_data.oid;
      if(old_faction_data != new_faction_data)
        {
        spdlog::info("updating faction {}", new_faction_data.name);
        if(auto updres{db.update_faction_info(new_faction_data)}; not updres)
          spdlog::error("failed to update faction data for {}", new_faction_data.name);
        }
      }
    result.emplace_back(std::move(new_faction_data));
    }
  return result;
  }
  
void current_state_t::route_system_visited(uint64_t system_address)
{
  auto it{std::ranges::find( route_, system_address, [](info::route_item_t const & rt ) -> uint64_t{
    return rt.system_address;
  })};
  if( it != route_.end())
  {
    it->visited = true;
    // make sure all previous marked as visited
    for( auto itb{route_.begin()}; itb != it; ++itb)
      if(not itb->visited)
        itb->visited = true;
  }
}
void current_state_t::handle(std::chrono::sys_seconds timestamp, events::event_holder_t && payload)
  {
  if(nullptr != parent->jlw_)
    {
    bool update_system{};
    bool update_ship{};
    bool update_mission_info{};
    bool route_changed{};
    std::visit(
      [&](auto && event)
      {
        auto f_route_progress = [&](uint64_t system_address)
        {
          route_changed = true;
          current_system_address_ = system_address;
          route_system_visited(system_address);
        };
        
        using T = std::decay_t<decltype(event)>;
        if constexpr(std::same_as<T, events::start_jump_t>)
          {
          if(event.JumpType == events::jump_type_e::Hyperspace)
            {
            auto res{db_.load_system(*event.SystemAddress)};
            if(not res) [[unlikely]]
              {
              spdlog::error("error loading system {} {}", *event.SystemAddress, *event.StarSystem);
              system = new_system_def(*event.SystemAddress, *event.StarSystem, *event.StarClass);
              }
            else if(std::optional loaded{std::move(*res)}; loaded)
              system = std::move(*loaded);
            else
              {
              system = new_system_def(*event.SystemAddress, *event.StarSystem, *event.StarClass);
              if(auto res2{db_.store(system)}; not res2) [[unlikely]]
                spdlog::error("error string system {} {}", system.system_address, system.star_type);
              }
            system_factions.clear();
            update_system = true;
            f_route_progress(*event.SystemAddress);
            }
          }
        else if constexpr(std::same_as<T, events::location_t>)
          {
          // after reloading game start at this system
          buffered_signals.clear();

          if(auto res{db_.load_system(event.SystemAddress)}; not res) [[unlikely]]
            {
            spdlog::error("error loading system {} {}", event.SystemAddress, event.StarSystem);
            system = new_system_def(event.SystemAddress, event.StarSystem, {});
            }
          else if(std::optional loaded{std::move(*res)}; loaded)
            {
            system = std::move(*loaded);
            if(system.system_location != event.StarPos)
              {
              system.system_location = event.StarPos;
              if(auto res{db_.store_system_location(system.system_address, system.system_location)}; not res)
                spdlog::error("failed to store system location {}", system.system_address);
              }
            }
          else
            {
            system = new_system_def(event.SystemAddress, event.StarSystem, {});
            system.system_location = event.StarPos;
            if(auto res2{db_.store(system)}; not res2) [[unlikely]]
              spdlog::error("error string system {} {}", event.SystemAddress, event.StarSystem);
            }
          // add/update factions database
          if(not event.Factions.empty())
            system_factions = process_factions(db_, event.Factions);
          f_route_progress(event.SystemAddress);
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fsd_jump_t>)
          {
          if(system.system_address != event.SystemAddress)
            spdlog::error("jump without start jump {}", system.system_address, event.SystemAddress);
          else if(system.system_location != event.StarPos)
            {
            system.system_location = event.StarPos;
            if(auto res{db_.store_system_location(system.system_address, system.system_location)}; not res)
              spdlog::error("failed to store system location {}", system.system_address);
            }
          jump_info = event;
          ship_loadout.FuelLevel = event.FuelLevel;

          if(not event.Factions.empty())
            system_factions = process_factions(db_, event.Factions);

          f_route_progress(event.SystemAddress);
          update_system = true;
          update_ship = true;
          }
        else if constexpr(std::same_as<T, events::fsd_target_t>)
          {
          next_target = event;
          update_system = true;
          }

        else if constexpr(std::same_as<T, events::fss_body_signals_t>)
          {
          auto it{system.body_by_id(event.BodyID)};
          if(it != system.bodies.end())
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            details.signals_ = std::move(event.Signals);
            if(auto res{db_.store(system.system_address, event.BodyID, details.signals_)}; not res)
              spdlog::error("failed to store signal for {}: {}", system.system_address, event.BodyID);
            }
          else
            {
            buffered_signals.emplace_back(event.BodyID, std::move(event.Signals));
            }
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::dss_body_signals_t>)
          {
          if(stralgo::ends_with(event.BodyName, "Ring"sv))
            {
            if(auto it{system.ring_by_id(event.BodyID)}; it != system.rings.end())
              {
              ring_t & ring{*it};
              ring.signals_ = std::move(event.Signals);
              if(auto res{db_.store(system.system_address, event.BodyID, ring.signals_)}; not res)
                spdlog::error("failed to store signals for ring {}: {}", system.system_address, event.BodyID);
              }
            else
              spdlog::error("ring was not found for {}: {} {}", system.system_address, event.BodyID, event.BodyName);
            }
          else if(auto it{system.body_by_id(event.BodyID)}; it != system.bodies.end())
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            if(details.signals_.size() != event.Signals.size())
              {
              details.signals_ = std::move(event.Signals);
              if(auto res{db_.store(system.system_address, event.BodyID, details.signals_)}; not res)
                spdlog::error("failed to store signal for {}: {}", system.system_address, event.BodyID);
              }
            if(details.genuses_.size() != event.Genuses.size())
              {
              details.genuses_ = std::move(event.Genuses);
              if(auto res{db_.store(system.system_address, event.BodyID, details.genuses_)}; not res)
                spdlog::error("failed to store genuses_ for {}: {}", system.system_address, event.BodyID);
              }
            }
          else
            buffered_signals.emplace_back(event.BodyID, std::move(event.Signals), std::move(event.Genuses));

          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>)
          {
          system.fss_complete = true;
          if(auto res{db_.store_fss_complete(system.system_address)}; not res) [[unlikely]]
            spdlog::error("failed to update fss scan complete for {}", system.system_address);
          }
        else if constexpr(std::same_as<T, events::scan_bary_centre_t>)
          {
          system.bary_centre.emplace_back(
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
          }
        else if constexpr(std::same_as<T, events::scan_detailed_scan_t>)
          {
          if(system.fss_complete)
            return;

          if(auto it{system.body_by_id(event.BodyID)}; it == system.bodies.end())
            {
            system.bodies.emplace_back(to_body(std::move(event)));
            body_t & body{system.bodies.back()};
            std::visit(
              [&]<typename U>(U & details)
              {
                if constexpr(std::same_as<U, planet_details_t>)
                  {
                  if(auto it{
                       std::ranges::find(buffered_signals, body.body_id, [](auto const & bs) { return bs.body_id; })
                     };
                     buffered_signals.end() != it)
                    {
                    details.signals_ = std::move(it->signals_);
                    details.genuses_ = std::move(it->genuses_);
                    buffered_signals.erase(it);
                    }
                  }
              },
              body.details
            );
            if(auto res{db_.store(system.system_address, body)}; not res)
              spdlog::error("failed to store body {}: {}", system.system_address, body.name);

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
              if(auto res{db_.store(system.system_address, rings)}; not res)
                spdlog::error("failed to store rings for {}: {}", system.system_address, body.name);
              else
                system.rings.insert(system.rings.end(), rings.begin(), rings.end());
              }
            }

          update_system = true;
          }
        else if constexpr(std::same_as<T, events::saa_scan_complete_t>)
          {
          if(stralgo::ends_with(event.BodyName, "Ring"sv))
            {
            // we got BodyID for ring, unknown at fss
            std::string_view planet_name{planet_name_from_ring_name(system.name, event.BodyName)};
            std::string_view ring_name{stralgo::right(event.BodyName, 6)};
            if(auto it{system.body_by_name(planet_name)}; it != system.bodies.end())
              {
              events::body_id_t const parent_planet_id{it->body_id};
              if(auto res{db_.store_ring_body_id(system.system_address, parent_planet_id, ring_name, event.BodyID)};
                 not res) [[unlikely]]
                spdlog::error("failed to update ring body id for {}:{}", system.system_address, event.BodyName);

              if(auto itr{std::ranges::find_if(
                   system.rings,
                   [&parent_planet_id, &ring_name](ring_t const & ring) noexcept -> bool
                   { return ring.parent_body_id == parent_planet_id and ring_name == ring.name; }
                 )};
                 itr != system.rings.end())
                itr->body_id = event.BodyID;
              else
                spdlog::error(
                  "failed to update (runtime) ring body id for {}:{}", system.system_address, event.BodyName
                );
              }
            else
              spdlog::error(
                "failed to find body for ring {}:{}, system not scanned", system.system_address, event.BodyName
              );
            }
          else if(auto it{system.body_by_id(event.BodyID)}; it != system.bodies.end())
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            details.mapped = true;
            if(auto res{db_.store_dss_complete(system.system_address, event.BodyID)}; not res) [[unlikely]]
              spdlog::error("failed to update dss scan complete for {}:{}", system.system_address, event.BodyID);
            }
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fuel_scoop_t>)
          {
          ship_loadout.FuelLevel = event.Total;
          update_ship = true;
          }
        else if constexpr(std::same_as<T, events::loadout_t>)
          {
          ship_loadout = ship_loadout_t{
            .Ship = std::move(event.Ship),
            .ShipID = event.ShipID,
            .ShipName = std::move(event.ShipName),
            .ShipIdent = std::move(event.ShipIdent),
            .HullHealth = event.HullHealth,
            .CargoCapacity = event.CargoCapacity,
            .FuelCapacity = event.FuelCapacity,
            .Modules = std::move(event.Modules)
          };
          std::ranges::sort(
            ship_loadout.Modules, std::less{}, [](events::module_t const & mod) -> uint8_t { return mod.Priority; }
          );
          update_ship = true;
          }
        else if constexpr(std::same_as<T, events::cargo_t>)
          {
          ship_loadout.CargoUsed = event.Count;
          update_ship = true;
          }
        else if constexpr(std::same_as<T, events::mission_accepted_t>)
          {
          // in case restarted multiple times with same log prevent adding same missions
          if(auto res{db_.mission_exists(event.MissionID)}; res and not *res)
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
            if(auto res{db_.store(mission)}; not res) [[unlikely]]
              spdlog::error("failed to store mission details for {}", event.MissionID);
            }
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::mission_completed_t>)
          {
          if(auto res{db_.change_mission_status(event.MissionID, info::mission_status_e::completed)}; not res)
            [[unlikely]]
            [[unlikely]] spdlog::error("failed to change mission status for {}", event.MissionID);
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::mission_abandoned_t>)
          {
          if(auto res{db_.change_mission_status(event.MissionID, info::mission_status_e::abandoned)}; not res)
            [[unlikely]]
            [[unlikely]] spdlog::error("failed to change mission status for {}", event.MissionID);
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::mission_failed_t>)
          {
          if(auto res{db_.change_mission_status(event.MissionID, info::mission_status_e::failed)}; not res) [[unlikely]]
            [[unlikely]] spdlog::error("failed to change mission status for {}", event.MissionID);
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::mission_redirected_t>)
          {
          if(auto res{db_.redirect_mission(
               event.MissionID, event.NewDestinationSystem, event.NewDestinationStation, event.NewDestinationSettlement
             )};
             not res) [[unlikely]]
            spdlog::error("failed to change mission status for {}", event.MissionID);
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::missions_t>)
          {
          for(events::mission_failed_t const & mission: event.Failed)
            if(auto res{db_.change_mission_status(mission.MissionID, info::mission_status_e::failed)}; not res)
              [[unlikely]]
              spdlog::warn("failed to change mission status for {}", mission.MissionID);
          for(events::mission_completed_t const & mission: event.Complete)
            if(auto res{db_.change_mission_status(mission.MissionID, info::mission_status_e::completed)}; not res)
              [[unlikely]]
              spdlog::warn("failed to change mission status for {}", mission.MissionID);

          // called on startup so we are loading accepted missions
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::nav_route_t>)
          {
          route_.clear();
          if(not event.Route.empty())
            {
          info::space_location_t prev{event.Route.front().StarPos};
          std::ranges::transform(
            event.Route,
            std::back_inserter(route_),
            [&prev](events::nav_route_t::item_t & ri) -> info::route_item_t
            {
              info::route_item_t result{
                .system = std::move(ri.StarSystem),
                .system_address = ri.SystemAddress,
                .star_location = ri.StarPos,
                .star_class = std::move(ri.StarClass),
                .distance = info::distance(ri.StarPos, prev),
                .visited{}
              };
              prev = ri.StarPos;
              return result;
            }
          );}
          route_system_visited(current_system_address_);
          route_changed = true;
          }
        else if constexpr(std::same_as<T, events::nav_route_clear_t>)
          {
          route_.clear();
          route_changed = true;
          }
      },
      payload
    );
    
    if(route_changed)
    {
      QMetaObject::invokeMethod(
        parent,
        [target = parent]() mutable
        {
          if(target->route_view_) [[likely]]
            target->route_view_->refresh_ui();
        },
        Qt::QueuedConnection
      );
    }
      {
      std::lock_guard lock(buffer_mtx_);
      event_buffer_.push_back(std::move(payload));
      }
    QMetaObject::invokeMethod(
      parent->jlw_,
      [this]()
      {
        std::vector<events::event_holder_t> batch;
          {
          std::lock_guard lock(buffer_mtx_);
          batch = std::move(event_buffer_);
          event_buffer_.clear();
          }

        if(not batch.empty() and parent->jlw_)
          parent->jlw_->add_logs_batch(std::move(batch));
      },
      Qt::QueuedConnection
    );

    // parent->system_view_->model_->clear();
    if(update_system)
      QMetaObject::invokeMethod(
        parent,
        [target = parent]() mutable
        {
          if(target->system_view_) [[likely]]
            target->system_view_->refresh_ui();
        },
        Qt::QueuedConnection
      );

    if(update_ship)
      QMetaObject::invokeMethod(
        parent,
        [target = parent, sh = &ship_loadout]() mutable
        {
          if(target->ship_view_)
            target->ship_view_->refresh_ui(*sh);
        },
        Qt::QueuedConnection
      );
    if(update_mission_info)
      QMetaObject::invokeMethod(
        parent,
        [target = parent]() mutable
        {
          if(target->mission_view_)
            target->mission_view_->refresh_ui();
        },
        Qt::QueuedConnection
      );
    }
  }

void current_state_t::load_missions()
  {
  if(auto res{db_.load_missions()}; not res) [[unlikely]]
    spdlog::warn("failed to load missions status");
  else
    active_missions = std::move(*res);
  }
