#include "logic.h"
#include <main_window.h>

void current_state_t::handle(events::event_holder_t && payload)
  {
  if(nullptr != parent->jlw_)
    {
    bool update_system{};
    bool update_ship{};
    std::visit(
      [&](auto && event)
      {
        using T = std::decay_t<decltype(event)>;
        if constexpr(std::same_as<T, events::fsd_jump_t>)
          {
          jump_info = event;
          ship_loadout.FuelLevel = event.FuelLevel;
          update_system = true;
          update_ship = true;
          }
        else if constexpr(std::same_as<T, events::fsd_target_t>)
          {
          next_target = event;
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::start_jump_t>)
          {
          if(event.JumpType == events::jump_type_e::Hyperspace)
            {
            system = star_system_t{
              .system_address = *event.SystemAddress,
              .name = *event.StarSystem,
              .star_type = *event.StarClass,
              .bary_centre = {},
              .bodies = {},
              .sub_class = {},
              .fss_complete = {}
            };
            update_system = true;
            }
          }
        else if constexpr(std::same_as<T, events::fss_body_signals_t>)
          {
          // info(" {}", event.BodyName);
          auto it{system.body_by_id(event.BodyID)};
          if(it != system.bodies.end())
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            details.signals_ = std::move(event.Signals);
            }
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>)
          {
          system.fss_complete = true;
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
          auto it{system.body_by_id(event.BodyID)};
          if(it != system.bodies.end()) {}  // *it = to_body(std::move(event));
          else
            system.bodies.emplace_back(to_body(std::move(event)));

          update_system = true;
          }
        else if constexpr(std::same_as<T, events::saa_scan_complete_t>)
          {
          // info("saa scan complete for {}", event.BodyName);
          auto it{system.body_by_id(event.BodyID)};
          if(it != system.bodies.end())
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            details.mapped = true;
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
      },
      payload
    );
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
        [target = parent]() mutable
        {
          if(target->ship_view_)
            target->ship_view_->refresh_ui();
        },
        Qt::QueuedConnection
      );
    }
  }
