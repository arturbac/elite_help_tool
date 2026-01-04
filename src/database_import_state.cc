#include <database_import_state.h>
#include <spdlog/spdlog.h>
#include <simple_enum/std_format.hpp>

        
void database_import_state_t::handle(std::chrono::sys_seconds timestamp, events::event_holder_t && e)
  {
  state_t & state{*this->state};
  std::visit([&state]<typename T>(T & event) {} , e );
  
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
            {
            spdlog::critical("error loading system {} {}", *event.SystemAddress, *event.StarSystem);
            std::abort();
            }
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
              {
              spdlog::critical("error string system {} {}", *event.SystemAddress, *event.StarSystem);
              std::abort();
              }
            }
          }
        }
      else if constexpr(std::same_as<T, events::location_t>)
        {
        // after reloading game start at this system
        spdlog::info("location {}: {}", event.SystemAddress, event.StarSystem);
        auto res{state.db_.load_system(event.SystemAddress)};
        if(not res) [[unlikely]]
          {
          spdlog::critical("error loading system {} {}", event.SystemAddress, event.StarSystem);
          std::abort();
          }
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
            {
            spdlog::critical("error string system {} {}", event.SystemAddress, event.StarSystem);
            std::abort();
            }
          }
        }
      else if constexpr(std::same_as<T, events::fsd_jump_t>)
        {
        if(state.system.system_address != event.SystemAddress)
          {
          spdlog::critical("jump without start jump {}", state.system.system_address, event.SystemAddress);
          std::abort();
          }
        else if(state.system.system_location != event.StarPos)
          {
          state.system.system_location = event.StarPos;
          if(auto res{state.db_.store_system_location(state.system.system_address, state.system.system_location)};
             not res)
            {
            spdlog::critical("failed to store system location {}", state.system.system_address);
            std::abort();
            }
          }
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
          {
          spdlog::critical("failed to store body {}: {}", state.system.system_address, body.name);
          std::abort();
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
          {
          spdlog::critical("failed to store bary_centre {}: {}", state.system.system_address, bc.body_id);
          std::abort();
          }
        }
      else if constexpr(std::same_as<T, events::fss_body_signals_t>)
        {
        spdlog::info(" {}", event.BodyName);
        for(events::signal_t const & signal: event.Signals)
          spdlog::info("   {}: {}", signal.Type_Localised, signal.Count);

        auto it{state.system.body_by_id(event.BodyID)};
        if(it != state.system.bodies.end())
          {
          planet_details_t & details{std::get<planet_details_t>(it->details)};
          details.signals_ = std::move(event.Signals);

          if(auto res{state.db_.store(state.system.system_address, event.BodyID, details.signals_)}; not res)
            {
            spdlog::critical("failed to store signal for {}: {}", state.system.system_address, event.BodyID);
            std::abort();
            }
          }
        else
          {
          spdlog::info("buffering signals for {}: {}", state.system.system_address, event.BodyID);
          state.buffered_signals.emplace_back(event.BodyID, std::move(event.Signals));
          }
        }
      else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>)
        {
        spdlog::info("fss scan complete");
        state.system.fss_complete = true;
        if(auto res{state.db_.store_fss_complete(state.system.system_address)}; not res) [[unlikely]]
          {
          spdlog::critical("failed to update fss scan complete for {}", state.system.system_address);
          std::abort();
          }
        }
      else if constexpr(std::same_as<T, events::saa_scan_complete_t>)
        {
        spdlog::info("saa scan complete for {}", event.BodyName);
        auto it{state.system.body_by_id(event.BodyID)};
        if(it != state.system.bodies.end())
          {
          planet_details_t & details{std::get<planet_details_t>(it->details)};
          details.mapped = true;
          if(auto res{state.db_.store_dss_complete(state.system.system_address, event.BodyID)}; not res) [[unlikely]]
            {
            spdlog::critical("failed to update dss scan complete for {}:{}", state.system.system_address, event.BodyID);
            std::abort();
            }
          }
        }
    },
    e
  );
  }
