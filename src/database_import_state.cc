#include <eht_settings.h>
#include <database_import_state.h>
#include <spdlog/spdlog.h>
#include <simple_enum/std_format.hpp>
#include <stralgo/stralgo.h>
#include <span>
using namespace std::string_view_literals;

namespace
  {
template<typename... Args>
void critical_abort(std::format_string<Args...> fmt, Args &&... args)
  {
  spdlog::default_logger_raw()->debug(fmt, std::forward<Args>(args)...);
  std::abort();
  }

///\brief the longest window that still says anything about when the tick came
///
/// With a longer gap between readings the span covers half a day; intersecting with it narrows nothing
/// and only litters the table
[[nodiscard]]
auto max_tick_window() -> std::chrono::hours { return std::chrono::hours{eht::settings()->ticks.max_window_h}; }

///\brief records the trace of a tick when the watched value changed between two readings of the system
void note_tick(
  database_storage_t & db,
  info::tick_kind_e kind,
  uint64_t system_address,
  std::optional<std::chrono::sys_seconds> previously_seen,
  std::chrono::sys_seconds timestamp
)
  {
  // without a previous reading there is nothing to bound the window with - a first look at a system says nothing
  if(not previously_seen or *previously_seen >= timestamp or timestamp - *previously_seen > max_tick_window())
    return;

  if(
    auto res{db.store(info::tick_observation_t{
      .kind = kind, .system_address = system_address, .window_begin = *previously_seen, .window_end = timestamp
    })};
    not res
  )
    spdlog::error("failed to store tick observation for {}", system_address);
  }

///\brief whether one of the wars in this system has just been settled
///
/// The end of a war shares out the beaten faction's holding at once, outside the daily recalculation, so
/// a change of influence seen in the same reading is no trace of a tick and must not be counted as one
[[nodiscard]]
auto war_settled_now(
  database_storage_t & db, uint64_t system_address, std::span<events::conflict_t const> conflicts
) -> bool
  {
  for(events::conflict_t const & conflict: conflicts)
    {
    auto const record{info::to_conflict(system_address, {}, conflict)};

    // an empty status means "the war is over"; only the passage into that state matters here
    if(not record.status.empty())
      continue;

    auto last{db.last_conflict(system_address, record.faction1, record.faction2)};
    if(last and *last and not (*last)->status.empty())
      return true;
    }

  return false;
  }

///\brief records a faction's influence in the system, only when it has changed against the last row
void store_influence(
  database_storage_t & db,
  std::chrono::sys_seconds timestamp,
  uint64_t system_address,
  int64_t faction_oid,
  events::faction_info_t const & event_faction,
  std::optional<std::chrono::sys_seconds> previously_seen,
  bool war_settled
)
  {
  if(faction_oid == -1) [[unlikely]]
    critical_abort("missing faction oid for {}", event_faction.Name);

  // presence is always noted, even when nothing changed - otherwise a faction that was thrown out of the
  // system stays on the list forever, because its last row speaks only of the last change
  if(auto res{db.store_faction_seen(faction_oid, system_address, timestamp)}; not res) [[unlikely]]
    spdlog::error("failed to record presence of {} in {}", event_faction.Name, system_address);

  auto last{db.last_influence(faction_oid, system_address)};
  if(not last)
    critical_abort("failed to load influence for {} in {}", event_faction.Name, system_address);

  auto record{info::to_influence(faction_oid, system_address, timestamp, event_faction)};

  // influence alone, without the states - states can change outside the tick, whereas influence is
  // recalculated at it and nowhere else, so influence alone marks out the window
  if(*last and (*last)->influence != record.influence and not war_settled)
    note_tick(db, info::tick_kind_e::influence, system_address, previously_seen, timestamp);

  if(
    *last and (*last)->influence == record.influence and (*last)->faction_state == record.faction_state
    and (*last)->pending_states == record.pending_states and (*last)->active_states == record.active_states
    and (*last)->recovering_states == record.recovering_states
  )
    return;

  if(auto res{db.store(record)}; not res)
    critical_abort("failed to store influence for {} in {}", event_faction.Name, system_address);
  }

///\brief records the conflicts in the system, only when their state has changed
void process_conflicts(
  database_storage_t & db,
  std::chrono::sys_seconds timestamp,
  uint64_t system_address,
  std::span<events::conflict_t const> conflicts,
  std::optional<std::chrono::sys_seconds> previously_seen
)
  {
  for(events::conflict_t const & conflict: conflicts)
    {
    auto record{info::to_conflict(system_address, timestamp, conflict)};

    auto last{db.last_conflict(system_address, record.faction1, record.faction2)};
    if(not last)
      critical_abort("failed to load conflict {} vs {}", record.faction1, record.faction2);

    if(*last and **last == record)
      continue;

    // days won are recalculated by the war tick, which runs on its own clock - on 4 August 2026 it fell
    // two hours before the influence tick, on the seventh five hours before it
    if(*last and ((*last)->won_days1 != record.won_days1 or (*last)->won_days2 != record.won_days2))
      note_tick(db, info::tick_kind_e::war, system_address, previously_seen, timestamp);

    if(auto res{db.store(record)}; not res)
      critical_abort("failed to store conflict {} vs {}", record.faction1, record.faction2);
    }
  }

void process_factions(
  database_storage_t & db,
  std::chrono::sys_seconds timestamp,
  uint64_t system_address,
  std::span<events::faction_info_t> factions,
  bool personal,
  std::optional<std::chrono::sys_seconds> previously_seen,
  bool war_settled
)
  {
  for(events::faction_info_t & f: factions)
    {
    // influence is per system and recorded over time, so f must not be consumed
    info::faction_info_t new_faction_data{info::to_native(events::faction_info_t{f})};
    auto res{db.load_faction(new_faction_data.name)};
    if(not res)
      critical_abort("failed to load cation info for {}", new_faction_data.name);
    if(not *res)
      {
      // no data add
      spdlog::info("adding faction {}", new_faction_data.name);
      if(auto updres{db.update_faction_info(new_faction_data, personal)}; not updres)
        critical_abort("failed to add faction data for {}", new_faction_data.name);
      auto oidres{db.faction_oid(new_faction_data.name)};
      if(not oidres or not *oidres)
        critical_abort("failed to read faction oid for {}", new_faction_data.name);
      new_faction_data.oid = int64_t(**oidres);
      }
    else
      {
      info::faction_info_t old_faction_data{std::move(**res)};
      new_faction_data.oid = old_faction_data.oid;
      if(old_faction_data != new_faction_data)
        {
        spdlog::info("updating faction {}", new_faction_data.name);
        if(auto updres{db.update_faction_info(new_faction_data, personal)}; not updres)
          critical_abort("failed to update faction data for {}", new_faction_data.name);
        }
      }
    store_influence(db, timestamp, system_address, new_faction_data.oid, f, previously_seen, war_settled);
    }
  }
///\brief whether one docks a ship at this place
///
/// An escape pod on a carrier sends you to the last PORT, not to the last place you stopped at.
/// On-foot settlements are out, because they have no landing pad for a ship. A carrier is out despite its
/// pads - checked on 26.09.2026: after a stop at W1V-NXM at 14:33 the pod sent us to Arkush City,
/// where the stop had been at 14:16
[[nodiscard]]
auto is_port(std::string_view station_type) -> bool
  {
  return station_type != "OnFootSettlement" and station_type != "FleetCarrier" and not station_type.empty();
  }

  }  // namespace

void database_import_state_t::handle(std::chrono::sys_seconds timestamp, events::event_holder_t && e)
  {
  state_t & state{*this->state};

  std::visit(
    [&state, timestamp]<typename T>(T & event)
    {
      // a career belongs to a character - from another account's journal we take only the world, and these
      // events speak of nothing but what the player did, so in a stranger's database they describe nothing
      if constexpr(
        std::same_as<T, events::mission_accepted_t> or std::same_as<T, events::mission_completed_t>
        or std::same_as<T, events::mission_abandoned_t> or std::same_as<T, events::mission_failed_t>
        or std::same_as<T, events::mission_redirected_t> or std::same_as<T, events::missions_t>
        or std::same_as<T, events::sell_micro_resources_t> or std::same_as<T, events::backpack_change_t>
      )
        if(not state.personal)
          return;

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
            .fss_complete = {}
          };
          if(auto res2{state.db_.store(state.system)}; not res2) [[unlikely]]
            critical_abort("error string system {} {}", event.SystemAddress, event.StarSystem);
          }
        // the system description comes from these two events only
        if(apply_system_info(state.system, event))
          if(auto res{state.db_.update_system_info(state.system)}; not res)
            critical_abort("failed to store system info {}", event.SystemAddress);

        // when we last looked at this system - it has to be read before recording presence moves the
        // marker forward, because it is what closes the tick window from below
        auto previously_seen{state.db_.last_system_seen(event.SystemAddress)};
        if(not previously_seen) [[unlikely]]
          critical_abort("failed to read last visit of {}", event.SystemAddress);

        // the settling of a war has to be known before influence is counted, because it, and not the tick,
        // explains the change seen in the same reading
        bool const war_settled{war_settled_now(state.db_, event.SystemAddress, event.Conflicts)};

        // add/update factions database
        if(not event.Factions.empty())
          process_factions(
            state.db_, timestamp, event.SystemAddress, event.Factions, state.personal, *previously_seen, war_settled
          );
        if(not event.Conflicts.empty())
          process_conflicts(state.db_, timestamp, event.SystemAddress, event.Conflicts, *previously_seen);
        }
      else if constexpr(std::same_as<T, events::fsd_jump_t>)
        {
        // the import stops rather than guesses: unlike the live state, which can rebuild the arrival
        // from this event, a rebuild reading journals in order has no business finding a gap here
        if(state.system.system_address != event.SystemAddress)
          critical_abort(
            "jump to {} without start jump, the state was still at {}",
            event.SystemAddress,
            state.system.system_address
          );
        else if(state.system.system_location != event.StarPos)
          {
          state.system.system_location = event.StarPos;
          if(
            auto res{state.db_.store_system_location(state.system.system_address, state.system.system_location)};
            not res
          )
            critical_abort("failed to store system location {}", state.system.system_address);
          }
        // the system description comes from these two events only
        if(apply_system_info(state.system, event))
          if(auto res{state.db_.update_system_info(state.system)}; not res)
            critical_abort("failed to store system info {}", event.SystemAddress);

        // when we last looked at this system - it has to be read before recording presence moves the
        // marker forward, because it is what closes the tick window from below
        auto previously_seen{state.db_.last_system_seen(event.SystemAddress)};
        if(not previously_seen) [[unlikely]]
          critical_abort("failed to read last visit of {}", event.SystemAddress);

        // the settling of a war has to be known before influence is counted, because it, and not the tick,
        // explains the change seen in the same reading
        bool const war_settled{war_settled_now(state.db_, event.SystemAddress, event.Conflicts)};

        // add/update factions database
        if(not event.Factions.empty())
          process_factions(
            state.db_, timestamp, event.SystemAddress, event.Factions, state.personal, *previously_seen, war_settled
          );
        if(not event.Conflicts.empty())
          process_conflicts(state.db_, timestamp, event.SystemAddress, event.Conflicts, *previously_seen);
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

        if(
          auto it{std::ranges::find(state.system.bodies, event.BodyID, body_body_id_proj)};
          it != state.system.bodies.end()
        )
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
              if(
                auto it{
                  std::ranges::find(state.buffered_signals, body.body_id, [](auto const & bs) { return bs.body_id; })
                };
                state.buffered_signals.end() != it
              )
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
        auto const & bc{state.system.put_bary_centre(
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
        )};
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
          if(std::holds_alternative<planet_details_t>(it->details))
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            details.signals_ = std::move(event.Signals);

            if(auto res{state.db_.store(state.system.system_address, event.BodyID, details.signals_)}; not res)
              critical_abort("failed to store signals for {}: {}", state.system.system_address, event.BodyID);
            }
          else
            spdlog::error("body {}:{} does not hold planet details ...", event.BodyID, it->name);
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
            spdlog::error(
              "ring was not found for {}: {} {}, skipping", state.system.system_address, event.BodyID, event.BodyName
            );
          }
        else if(auto it{state.system.body_by_id(event.BodyID)}; it != state.system.bodies.end())
          {
          if(std::holds_alternative<planet_details_t>(it->details))
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
            spdlog::error("body {}:{} does not hold planet details ...", event.BodyID, it->name);
          }
        else
          {
          spdlog::info("buffering signals for {}: {}", state.system.system_address, event.BodyID);
          state.buffered_signals.emplace_back(event.BodyID, std::move(event.Signals), std::move(event.Genuses));
          }
        }
      else if constexpr(std::same_as<T, events::scan_organic_t>)
        {
        // the game ends the taking of a sample with the Analyse type; the earlier Log and Sample only announce it
        bool const analysed{event.ScanType == events::scan_type_e::Analyse};

        // mapping gives the genus alone, the sample fills in the species
        if(auto it{state.system.body_by_id(event.Body)}; it != state.system.bodies.end())
          if(std::holds_alternative<planet_details_t>(it->details))
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            auto genus{std::ranges::find(details.genuses_, event.Genus_Localised, &events::genus_t::Genus_Localised)};
            if(genus != details.genuses_.end())
              {
              genus->Species_Localised = event.Species_Localised;
              genus->Sampled = genus->Sampled or analysed;
              }
            }

        if(
          auto res{state.db_.store_genus_species(
            event.SystemAddress,
            event.Body,
            event.Genus_Localised,
            event.Species_Localised,
            analysed and state.personal
          )};
          not res
        ) [[unlikely]]
          critical_abort("failed to store species for {}:{}", event.SystemAddress, event.Body);
        }
      else if constexpr(std::same_as<T, events::sell_micro_resources_t>)
        {
        info::micro_sale_t sale{
          .timestamp = timestamp,
          .market_id = event.MarketID,
          .price = event.Price,
          .total_count = event.TotalCount
        };

        std::vector<info::micro_sale_item_t> items;
        items.reserve(event.MicroResources.size());
        for(events::sold_micro_resource_t const & sold: event.MicroResources)
          {
          auto key{micro_resource_key(sold.Name)};
          // the category comes only from here, the id and the readable name from the bartender
          if(auto res{state.db_.store(info::micro_resource_t{
               .name = key, .id = {}, .localised = sold.Name_Localised, .category = sold.Category
             })};
             not res)
            spdlog::error("failed to store micro resource {}", sold.Name);

          items.emplace_back(info::micro_sale_item_t{.name = std::move(key), .count = sold.Count});
          }

        if(auto res{state.db_.store(sale, items)}; not res)
          critical_abort("failed to store micro resource sale at {}", event.MarketID);
        }
        else if constexpr(std::same_as<T, events::approach_settlement_t>)
        {
        state.settlement_market_id = event.MarketID;
        if(auto res{state.db_.store(info::station_t{
             .market_id = event.MarketID,
             .system_address = event.SystemAddress,
             .name = event.Name,
             .station_type = {},
             .economy = event.StationEconomy_Localised,
             .government = event.StationGovernment_Localised,
             .controlling_faction = event.StationFaction.Name
           })};
           not res)
          spdlog::error("failed to store settlement {}", event.MarketID);
        }
      else if constexpr(std::same_as<T, events::disembark_t>)
        {
        // arriving by taxi is sometimes the only trace that we are at this settlement
        if(event.MarketID != 0)
          {
          state.settlement_market_id = event.MarketID;
          if(auto res{state.db_.store(info::station_t{
               .market_id = event.MarketID,
               .system_address = event.SystemAddress,
               .name = event.StationName,
               .station_type = event.StationType,
               .economy = {},
               .government = {}
             })};
             not res)
            spdlog::error("failed to store station {}", event.MarketID);
          }
        }
      else if constexpr(std::same_as<T, events::supercruise_entry_t>)
        state.settlement_market_id = 0;
      // leaving the pad ends our presence at the place just as supercruise does
      else if constexpr(std::same_as<T, events::undocked_t>)
        state.settlement_market_id = 0;
      else if constexpr(std::same_as<T, events::backpack_change_t>)
        {
        for(events::backpack_item_t const & item: event.Added)
          {
          auto key{micro_resource_key(item.Name)};
          // the type out of the backpack is the same category as the one used when selling
          if(auto res{state.db_.store(info::micro_resource_t{
               .name = key, .id = {}, .localised = item.Name_Localised, .category = item.Type
             })};
             not res)
            spdlog::error("failed to store micro resource {}", item.Name);

          // the place is stored as a market_id, so the economy follows on its own once we learn it
          if(auto res{state.db_.store(info::micro_acquisition_t{
               .timestamp = timestamp, .market_id = state.settlement_market_id, .name = std::move(key),
               .count = item.Count,
               .source = info::acquisition_source_e::collected
             })};
             not res)
            spdlog::error("failed to store acquisition {}", item.Name);
          }
        }
      else if constexpr(std::same_as<T, events::shipyard_transfer_t>)
        {
        // the game gives the delivery time once and never mentions it again, and announces the arrival not at all
        if(auto res{state.db_.store(info::ship_transfer_t{
             .ship_id = event.ShipID,
             .ship_type = event.ShipType_Localised.empty() ? event.ShipType : event.ShipType_Localised,
             .from_system = event.System,
             .to_market_id = event.MarketID,
             .distance = event.Distance,
             .price = event.TransferPrice,
             .ordered = timestamp,
             .arrives = timestamp + std::chrono::seconds{event.TransferTime}
           })};
           not res)
          spdlog::error("failed to store ship transfer {}", event.ShipID);
        }
      else if constexpr(std::same_as<T, events::docked_t>)
        {
        if(is_port(event.StationType))
          if(auto res{state.db_.store(info::port_visit_t{
               .market_id = event.MarketID,
               .name = event.StationName,
               .system = event.StarSystem,
               .station_type = event.StationType,
               .visited = timestamp
             })};
             not res)
            spdlog::error("failed to store port visit {}", event.MarketID);

        // a station's identity is rebuilt from journals - the type tells a carrier from a station
        state.settlement_market_id = event.MarketID;
        info::station_t station{
          .market_id = event.MarketID,
          .system_address = event.SystemAddress,
          .name = event.StationName,
          .station_type = event.StationType,
          .economy = event.StationEconomy_Localised,
          .government = event.StationGovernment_Localised,
          .controlling_faction = event.StationFaction.Name,
          .dist_from_star_ls = event.DistFromStarLS
        };

        if(auto res{state.db_.store(station)}; not res) [[unlikely]]
          critical_abort("failed to store station {}", event.MarketID);
        }
      else if constexpr(std::same_as<T, events::market_t>)
        {
        // a market's contents exist only in Market.json and only live; the import knows the station alone
        info::station_t station{
          .market_id = event.MarketID,
          .system_address = state.system.system_address,
          .name = event.StationName,
          .station_type = event.StationType,
          .economy = {},
          .government = {}
        };

        if(auto res{state.db_.store(station)}; not res) [[unlikely]]
          critical_abort("failed to store station {}", event.MarketID);
        }
      else if constexpr(std::same_as<T, events::fss_signal_discovered_t>)
        {
        // a USS expires after a few minutes, in the database it would be nothing but litter
        if(event.TimeRemaining)
          return;

        if(auto res{state.db_.store(to_system_signal(event, timestamp))}; not res) [[unlikely]]
          critical_abort("failed to store signal for {}", event.SystemAddress);
        }
      else if constexpr(std::same_as<T, events::commander_t>)
        {
        // from this moment to the end of the file it is known whose the entries are
        state.personal = state.owner_fid.empty() or event.FID == state.owner_fid;
        if(not state.personal)
          spdlog::info("journal of {} - taking the world from it, not the career", event.Name);
        }
      else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>)
        {
        spdlog::info("fss scan complete");
        state.system.fss_complete = true;
        if(state.personal)
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
            if(
              auto res{
                state.db_.store_ring_body_id(state.system.system_address, parent_planet_id, ring_name, event.BodyID)
              };
              not res
            ) [[unlikely]]
              critical_abort("failed to update ring body id for {}:{}", state.system.system_address, event.BodyName);

            if(
              auto itr{std::ranges::find_if(
                state.system.rings,
                [&parent_planet_id, &ring_name](ring_t const & ring) noexcept -> bool
                { return ring.parent_body_id == parent_planet_id and ring_name == ring.name; }
              )};
              itr != state.system.rings.end()
            )
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
          if(std::holds_alternative<planet_details_t>(it->details))
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            details.mapped = true;
            if(state.personal)
              if(auto res{state.db_.store_dss_complete(state.system.system_address, event.BodyID)};
                 not res) [[unlikely]]
                critical_abort(
                  "failed to update dss scan complete for {}:{}", state.system.system_address, event.BodyID
                );
            }
          else
            spdlog::error("body {}:{} does not hold planet details ...", event.BodyID, it->name);
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
          .market_id = state.settlement_market_id,
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

        // a cargo mission says what is needed - without that there is no telling where to get it
        if(not event.Commodity_Localised.empty() and event.Count != 0u)
          if(
            auto res{state.db_.store(
              info::mission_cargo_t{
                .mission_id = event.MissionID, .commodity = event.Commodity_Localised, .count = event.Count
              }
            )};
            not res
          )
            spdlog::error("failed to store mission cargo for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::mission_completed_t>)
        {
        if(auto res{state.db_.complete_mission(event.MissionID, timestamp, event.Reward)}; not res) [[unlikely]]
          critical_abort("failed to change mission status for {}", event.MissionID);
        // whom this mission moved and by how much - pluses with a sign, so that pushing strangers out can be
        // counted apart from building one's own up. The game gives no number, the measure is the string's length
        for(events::faction_effect_t const & effect: event.FactionEffects)
          for(events::influence_effect_t const & influence: effect.Influence)
            {
            if(influence.Influence.empty())
              continue;

            auto const magnitude{int32_t(influence.Influence.size())};
            if(auto res{state.db_.store(info::mission_influence_t{
                 .mission_id = event.MissionID,
                 .timestamp = timestamp,
                 .faction = effect.Faction,
                 .system_address = influence.SystemAddress,
                 .pluses = influence.Trend == "DownBad" ? -magnitude : magnitude
               })};
               not res)
              spdlog::error("failed to store mission influence for {}", event.MissionID);
            }

          // rewards go straight to the locker and never appear in the backpack - nothing is counted twice
          for(events::material_reward_t const & reward: event.MaterialsReward)
            {
            // Encoded, Manufactured and Elements are ship materials, not micro resources
            if(reward.Category_Localised != "Data" and reward.Category_Localised != "Item"
               and reward.Category_Localised != "Component" and reward.Category_Localised != "Consumable")
              continue;
      
            auto key{micro_resource_key(reward.Name)};
            if(auto res{state.db_.store(info::micro_resource_t{
                 .name = key, .id = {}, .localised = {}, .category = reward.Category_Localised
               })};
               not res)
              spdlog::error("failed to store micro resource {}", reward.Name);
      
            if(auto res{state.db_.store(info::micro_acquisition_t{
                 .timestamp = timestamp,
                 .market_id = state.settlement_market_id,
                 .name = std::move(key),
                 .count = reward.Count,
                 .source = info::acquisition_source_e::mission_reward
               })};
               not res)
              spdlog::error("failed to store mission reward {}", reward.Name);
            }
        }
      else if constexpr(std::same_as<T, events::mission_abandoned_t>)
        {
        if(auto res{state.db_.change_mission_status(event.MissionID, info::mission_status_e::abandoned, timestamp)}; not res)
          [[unlikely]]
          critical_abort("failed to change mission status for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::mission_failed_t>)
        {
        if(auto res{state.db_.change_mission_status(event.MissionID, info::mission_status_e::failed, timestamp)}; not res)
          [[unlikely]]
          critical_abort("failed to change mission status for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::mission_redirected_t>)
        {
        if(
          auto res{state.db_.redirect_mission(
            event.MissionID, event.NewDestinationSystem, event.NewDestinationStation, event.NewDestinationSettlement
          )};
          not res
        ) [[unlikely]]
          critical_abort("failed to change mission status for {}", event.MissionID);
        }
      else if constexpr(std::same_as<T, events::missions_t>)
        {
        for(events::mission_failed_t const & mission: event.Failed)
          if(auto res{state.db_.change_mission_status(mission.MissionID, info::mission_status_e::failed, timestamp)}; not res)
            [[unlikely]]
            spdlog::warn("failed to change mission status for {}", mission.MissionID);
        for(events::mission_completed_t const & mission: event.Complete)
          if(auto res{state.db_.change_mission_status(mission.MissionID, info::mission_status_e::completed, timestamp)}; not res)
            [[unlikely]]
            spdlog::warn("failed to change mission status for {}", mission.MissionID);

        // this is the only moment the game says outright what is still open - everything outside that list has closed
        std::vector<uint64_t> active;
        active.reserve(event.Active.size());
        for(events::mission_active_t const & mission: event.Active)
          active.push_back(mission.MissionID);

        if(auto res{state.db_.expire_missions_outside(active, timestamp)}; not res) [[unlikely]]
          spdlog::warn("failed to expire stale missions");
        }
      else if constexpr(std::same_as<T, events::nav_route_t>)
        {
        // ignored in import
        }
      else if constexpr(std::same_as<T, events::nav_route_clear_t>)
        {
        // ignored in import
        }
      else if constexpr(std::same_as<T, events::carrier_stats_t>)
        {
        // A carrier's state goes into live.sqlite, which a rebuild does not touch - so writing from here
        // would rebuild nothing, it would only overwrite the current state with what once was.
        // A backfill from history is possible (journals run in order, so the last one would win),
        // but it would drag in every stranger's carrier we ever stood at as well
        }
      else if constexpr(std::same_as<T, events::fcmaterials_t>)
        {
        // ignored in import
        }
    },
    e
  );
  }
