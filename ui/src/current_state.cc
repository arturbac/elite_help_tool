#include <eht_settings.h>
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

///\brief the longest window that still says anything about when the tick came
///
/// With a longer gap between readings the span covers half a day; intersecting with it narrows nothing
/// and only litters the table
[[nodiscard]]
static auto max_tick_window() -> std::chrono::hours { return std::chrono::hours{eht::settings()->ticks.max_window_h}; }

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
    {
    spdlog::error("missing faction oid for {}", event_faction.Name);
    return;
    }

  // presence is always noted, even when nothing changed - otherwise a faction that was thrown out of the
  // system stays on the list forever, because its last row speaks only of the last change
  if(auto res{db.store_faction_seen(faction_oid, system_address, timestamp)}; not res) [[unlikely]]
    spdlog::error("failed to record presence of {} in {}", event_faction.Name, system_address);

  auto last{db.last_influence(faction_oid, system_address)};
  if(not last)
    {
    spdlog::error("failed to load influence for {} in {}", event_faction.Name, system_address);
    return;
    }

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
    spdlog::error("failed to store influence for {} in {}", event_faction.Name, system_address);
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
      {
      spdlog::error("failed to load conflict {} vs {}", record.faction1, record.faction2);
      continue;
      }

    if(*last and **last == record)
      continue;

    // days won are recalculated by the war tick, which runs on its own clock - on 4 August 2026 it fell
    // two hours before the influence tick, on the seventh five hours before it
    if(*last and ((*last)->won_days1 != record.won_days1 or (*last)->won_days2 != record.won_days2))
      note_tick(db, info::tick_kind_e::war, system_address, previously_seen, timestamp);

    if(auto res{db.store(record)}; not res)
      spdlog::error("failed to store conflict {} vs {}", record.faction1, record.faction2);
    }
  }

auto process_factions(
  database_storage_t & db,
  std::chrono::sys_seconds timestamp,
  uint64_t system_address,
  std::span<events::faction_info_t> factions,
  bool personal,
  std::optional<std::chrono::sys_seconds> previously_seen,
  bool war_settled
) -> std::vector<info::faction_info_t>
  {
  std::vector<info::faction_info_t> result;
  result.reserve(factions.size());

  for(events::faction_info_t & f: factions)
    {
    // influence is per system and recorded over time, so f must not be consumed
    info::faction_info_t new_faction_data{info::to_native(events::faction_info_t{f})};
    auto res{db.load_faction(new_faction_data.name)};
    if(not res)
      spdlog::error("failed to load faction info for {}", new_faction_data.name);
    else if(not *res)
      {
      // no data add
      spdlog::info("adding faction {}", new_faction_data.name);
      if(auto updres{db.update_faction_info(new_faction_data, personal)}; not updres)
        spdlog::error("failed to add faction data for {}", new_faction_data.name);
      auto oidres{db.faction_oid(new_faction_data.name)};
      if(oidres and *oidres)
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
          spdlog::error("failed to update faction data for {}", new_faction_data.name);
        }
      }
    store_influence(db, timestamp, system_address, new_faction_data.oid, f, previously_seen, war_settled);
    result.emplace_back(std::move(new_faction_data));
    }
  return result;
  }

void current_state_t::route_system_visited(uint64_t system_address)
  {
  auto it{std::ranges::find(
    route_, system_address, [](info::route_item_t const & rt) -> uint64_t { return rt.system_address; }
  )};
  if(it != route_.end())
    {
    it->visited = true;
    // make sure all previous marked as visited
    for(auto itb{route_.begin()}; itb != it; ++itb)
      if(not itb->visited)
        itb->visited = true;
    }
  }

///\brief the events that say where the ship is and what it is flying
///\detail these are replayed even when they are old, because nothing else rebuilds them: the system
/// under us, the ship around us, the place we are standing in. They are a few dozen in a session
/// against the thousands of scans, signals and kills that only ever need writing down once
template<typename event_t>
constexpr bool rebuilds_present_state{
  std::same_as<event_t, events::location_t> or std::same_as<event_t, events::start_jump_t>
  or std::same_as<event_t, events::fsd_jump_t> or std::same_as<event_t, events::fsd_target_t>
  or std::same_as<event_t, events::loadout_t> or std::same_as<event_t, events::docked_t>
  or std::same_as<event_t, events::undocked_t> or std::same_as<event_t, events::supercruise_entry_t>
  or std::same_as<event_t, events::approach_settlement_t> or std::same_as<event_t, events::disembark_t>
  or std::same_as<event_t, events::cargo_t> or std::same_as<event_t, events::missions_t>
  or std::same_as<event_t, events::commander_t> or std::same_as<event_t, events::nav_route_t>
  or std::same_as<event_t, events::nav_route_clear_t>
  // a sample half taken when the tool was closed is still half taken - and the handler only repeats the
  // species already written, in the same order
  or std::same_as<event_t, events::scan_organic_t>
  // where the commander stands on foot - they only move the tracker, write nothing
  or std::same_as<event_t, events::book_dropship_t> or std::same_as<event_t, events::dropship_deploy_t>
  or std::same_as<event_t, events::embark_t> or std::same_as<event_t, events::died_t>
  // the fleet: each of them sets whole rows, so a repeat writes the same again - and a database older than
  // the fleet table gets it from the newest journal
  or fleet::changes_fleet<event_t>
};

///\brief how often the mark is moved on disk while playing
[[nodiscard]]
static auto progress_write_interval() -> std::chrono::seconds
  { return std::chrono::seconds{eht::settings()->journal.progress_write_s}; }

void current_state_t::remember_progress()
  {
  if(last_event_ == std::chrono::sys_seconds{})
    return;

  progress_written_ = std::chrono::steady_clock::now();
  if(auto res{db_.store_journal_progress(last_event_)}; not res)
    spdlog::error("failed to record how far the journal was read");
  }

void current_state_t::forget_live_combat()
  {
  target = {};
  last_bounty = {};
  last_bounty_at = {};
  fighter = fighter_e::stowed;
  fighter_crewed = false;
  }

void current_state_t::handle(std::chrono::sys_seconds timestamp, events::event_holder_t && payload)
  {
  if(nullptr != parent->jlw_)
    {
    // Already written down, and only on the way back to the present - after that everything is
    // taken as it comes, because a game clock that stepped backwards must not silence live events
    bool const already_written{catching_up_ and timestamp <= resume_from_};
    if(timestamp > last_event_)
      last_event_ = timestamp;

    bool update_system{};
    bool update_ship{};
    bool update_mission_info{};
    bool update_factions{};
    bool update_micro_resources{};
    bool route_changed{};
    std::visit(
      [&](auto && event)
      {
        auto f_route_progress = [&](uint64_t system_address)
        {
          route_changed = true;
          current_system_address_ = system_address;
          route_system_visited(system_address);
          // reaching the target ends it, whether or not a NavRouteClear comes to say so
          if(next_target.SystemAddress == system_address)
            next_target = {};
        };

        using T = std::decay_t<decltype(event)>;

        if(already_written and not rebuilds_present_state<T>)
          {
          ++events_walked_past_;
          return;
          }
        ++events_handled_;

        // a career belongs to a character - from another account's session we take only the world
      if constexpr(
        std::same_as<T, events::mission_accepted_t> or std::same_as<T, events::mission_completed_t>
        or std::same_as<T, events::mission_abandoned_t> or std::same_as<T, events::mission_failed_t>
        or std::same_as<T, events::mission_redirected_t> or std::same_as<T, events::missions_t>
        or std::same_as<T, events::sell_micro_resources_t> or std::same_as<T, events::backpack_change_t>
        or std::same_as<T, events::shipyard_transfer_t> or fleet::changes_fleet<T>
      )
        if(not personal_)
          return;

      // the fleet - the same work in the import, so it lives in one place
      if constexpr(fleet::changes_fleet<T>)
        {
        fleet::record(db_, timestamp, event, system.name, settlement_market_id_);
        ++fleet_changes_;
        }

      if constexpr(std::same_as<T, events::commander_t>)
        {
        if(owner_fid_.empty())
          if(auto owner{db_.load_owner()}; owner and *owner)
            owner_fid_ = (*owner)->fid;

        personal_ = owner_fid_.empty() or event.FID == owner_fid_;
        commander_name_ = event.Name;
        if(not personal_)
          spdlog::warn("session of {} - its career will not be written to this database", event.Name);
        }
      else if constexpr(std::same_as<T, events::start_jump_t>)
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
            update_factions = true;
            f_route_progress(*event.SystemAddress);
            }
          }
        else if constexpr(std::same_as<T, events::location_t>)
          {
          // after reloading game start at this system
          buffered_signals.clear();
          ground_cz_.location(event);

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
          // when we last looked at this system - it has to be read before recording presence moves the
          // marker forward, because it is what closes the tick window from below
          std::optional<std::chrono::sys_seconds> previously_seen;
          if(auto seen{db_.last_system_seen(event.SystemAddress)}; seen)
            previously_seen = *seen;
          else
            spdlog::error("failed to read last visit of {}", event.SystemAddress);

          // add/update factions database
          // the settling of a war has to be known before influence is counted, because it, and not the
          // tick, explains the change seen in the same reading
          bool const war_settled{war_settled_now(db_, event.SystemAddress, event.Conflicts)};

          if(not event.Factions.empty())
            {
            system_factions = process_factions(
              db_, timestamp, event.SystemAddress, event.Factions, personal_, previously_seen, war_settled
            );
            update_factions = true;
            }
          if(not event.Conflicts.empty())
            {
            process_conflicts(db_, timestamp, event.SystemAddress, event.Conflicts, previously_seen);
            update_factions = true;
            }
          // the system description comes from these two events only
          if(apply_system_info(system, event))
            {
            if(auto res{db_.update_system_info(system)}; not res)
              spdlog::error("failed to store system info {}", event.SystemAddress);
            update_factions = true;
            }
          // Where we are, for the place's market and missions: docked, the port is named. Started on foot
          // in a concourse it is not - only Body, with BodyType Station - so the port is found by its name
          if(event.MarketID != 0u)
            settlement_market_id_ = event.MarketID;
          else if(event.OnFoot and event.BodyType == "Station" and not event.Body.empty())
            {
            auto found{db_.load_station(event.SystemAddress, event.Body)};
            settlement_market_id_ = found and *found ? (*found)->market_id : 0u;
            }
          else
            settlement_market_id_ = 0u;
          f_route_progress(event.SystemAddress);
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fsd_jump_t>)
          {
          ground_cz_.jumped(event.SystemAddress);
          if(system.system_address != event.SystemAddress)
            {
            // StartJump normally moves us here while the drive is still charging. Without it the state
            // holds the system we left, and carrying on does more harm than the missing event ever
            // would: apply_system_info below copies THIS system's economy and government onto the
            // previous system's object and stores them under its address, while the overlay shows one
            // system's name above another's factions. So the arrival is rebuilt from the event.
            spdlog::error(
              "jump to {} without start jump, the state was still at {}", event.SystemAddress, system.system_address
            );

            if(auto res{db_.load_system(event.SystemAddress)}; not res) [[unlikely]]
              {
              spdlog::error("error loading system {} {}", event.SystemAddress, event.StarSystem);
              system = new_system_def(event.SystemAddress, event.StarSystem, {});
              }
            else if(std::optional loaded{std::move(*res)}; loaded)
              system = std::move(*loaded);
            else
              {
              system = new_system_def(event.SystemAddress, event.StarSystem, {});
              if(auto res2{db_.store(system)}; not res2) [[unlikely]]
                spdlog::error("error storing system {} {}", event.SystemAddress, event.StarSystem);
              }

            system.system_location = event.StarPos;
            if(auto res{db_.store_system_location(system.system_address, system.system_location)}; not res)
              spdlog::error("failed to store system location {}", system.system_address);

            // both belong to the system we were in, and neither has anything to say about this one
            buffered_signals.clear();
            system_factions.clear();
            update_factions = true;
            }
          else if(system.system_location != event.StarPos)
            {
            system.system_location = event.StarPos;
            if(auto res{db_.store_system_location(system.system_address, system.system_location)}; not res)
              spdlog::error("failed to store system location {}", system.system_address);
            }
          jump_info = event;
          ship_loadout.FuelLevel = event.FuelLevel;

          // when we last looked at this system - it has to be read before recording presence moves the
          // marker forward, because it is what closes the tick window from below
          std::optional<std::chrono::sys_seconds> previously_seen;
          if(auto seen{db_.last_system_seen(event.SystemAddress)}; seen)
            previously_seen = *seen;
          else
            spdlog::error("failed to read last visit of {}", event.SystemAddress);

          // the settling of a war has to be known before influence is counted, because it, and not the
          // tick, explains the change seen in the same reading
          bool const war_settled{war_settled_now(db_, event.SystemAddress, event.Conflicts)};

          if(not event.Factions.empty())
            {
            system_factions = process_factions(
              db_, timestamp, event.SystemAddress, event.Factions, personal_, previously_seen, war_settled
            );
            update_factions = true;
            }
          if(not event.Conflicts.empty())
            {
            process_conflicts(db_, timestamp, event.SystemAddress, event.Conflicts, previously_seen);
            update_factions = true;
            }
          // the system description comes from these two events only
          if(apply_system_info(system, event))
            {
            if(auto res{db_.update_system_info(system)}; not res)
              spdlog::error("failed to store system info {}", event.SystemAddress);
            update_factions = true;
            }

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
          // the scanner speaks of the body in front of it both when the mapping ends and when it opens again
          scanner_body_ = event.BodyName;
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
        else if constexpr(std::same_as<T, events::scan_organic_t>)
          {
          // the game ends the taking of a sample with the Analyse type; the earlier Log and Sample only announce it
          bool const analysed{event.ScanType == events::scan_type_e::Analyse};

          // mapping gives the genus alone, the sample fills in the species
          if(auto it{system.body_by_id(event.Body)}; it != system.bodies.end())
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

          // the one in progress - a Log of anything begins anew, as the game drops the unfinished one
          if(
            event.ScanType == events::scan_type_e::Log or sampling.species != event.Species_Localised
            or sampling.body != event.Body or sampling.system_address != event.SystemAddress
          )
            sampling = organic_sampling_t{
              .system_address = event.SystemAddress,
              .body = event.Body,
              .genus = event.Genus_Localised,
              .species = event.Species_Localised,
              .variant = event.Variant_Localised,
              .samples = 0u,
              .analysed = false,
              .was_logged = event.WasLogged,
              .points = {}
            };
          if(analysed)
            sampling.analysed = true;
          else
            sampling.samples = std::min(3u, sampling.samples + 1u);

          // only now is the commander standing where the sample was taken - a replayed scan has no place
          if(not catching_up_ and not analysed)
            {
            std::optional<bio::surface_point_t> point;
            std::string body_name;
            if(auto status{load_status(journal_dir_path_)}; status and status->Latitude and status->Longitude)
              {
              point = bio::surface_point_t{*status->Latitude, *status->Longitude};
              body_name = status->BodyName;
              sampling.points.push_back(*point);
              }
            last_organic_scan_ = organic_scan_seen_t{
              .scan = event,
              .timestamp = timestamp,
              .system_name = system.name,
              .body_name = std::move(body_name),
              .point = point,
              .sample = sampling.samples
            };
            ++organic_scans_seen_;
            }

          if(
            auto res{db_.store_genus_species(
              event.SystemAddress, event.Body, event.Genus_Localised, event.Species_Localised, personal_, analysed
            )};
            not res
          )
            spdlog::error("failed to store species for {}:{}", event.SystemAddress, event.Body);

          update_system = true;
          }
      else if constexpr(std::same_as<T, events::sell_micro_resources_t>)
          {
          update_micro_resources = true;
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
            if(auto res{db_.store(info::micro_resource_t{
                 .name = key, .id = {}, .localised = sold.Name_Localised, .category = sold.Category
               })};
               not res)
              spdlog::error("failed to store micro resource {}", sold.Name);

            items.emplace_back(info::micro_sale_item_t{.name = std::move(key), .count = sold.Count});
            }

          if(auto res{db_.store(sale, items)}; not res)
            spdlog::error("failed to store micro resource sale at {}", event.MarketID);
          }
          else if constexpr(std::same_as<T, events::approach_settlement_t>)
          {
          settlement_market_id_ = event.MarketID;
          if(auto res{db_.store(info::station_t{
               .market_id = event.MarketID,
               .system_address = event.SystemAddress,
               .name = event.Name,
               .station_type = {},
               .economy = event.StationEconomy_Localised,
               .government = event.StationGovernment_Localised,
               .controlling_faction = event.StationFaction.Name,
               .body_id = event.BodyID
             })};
             not res)
            spdlog::error("failed to store settlement {}", event.MarketID);
          ground_cz_.approach(event);
          if(auto res{db_.note_settlement_owner(event.MarketID, event.SystemAddress, event.StationFaction.Name, timestamp)};
             not res)
            spdlog::error("failed to note the owner of {}", event.MarketID);
          }
        else if constexpr(std::same_as<T, events::disembark_t>)
          {
          ground_cz_.disembark(event);
          // arriving by taxi is sometimes the only trace that we are at this settlement
          if(event.MarketID != 0)
            {
            settlement_market_id_ = event.MarketID;
            if(auto res{db_.store(info::station_t{
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
          settlement_market_id_ = 0;
        // leaving the pad ends our presence at the place just as entering supercruise does
        else if constexpr(std::same_as<T, events::undocked_t>)
          {
          if(personal_)
            {
            fleet::record(db_, timestamp, event, system.name, settlement_market_id_);
            ++fleet_changes_;
            }
          settlement_market_id_ = 0;
          // a name given while docked - a construction site's, chosen from the game's rolls - shows first here,
          // and a construction finished while docked leaves as what it became: a settlement, an outpost
          if(event.MarketID != 0u and not event.StationName.empty())
            {
            if(auto res{db_.store(info::station_t{
                 .market_id = event.MarketID,
                 .system_address = 0u,
                 .name = event.StationName,
                 .station_type = event.StationType
               })};
               not res)
              spdlog::error("failed to store the name of {}", event.MarketID);
            ++construction_changes_;
            }
          if(not catching_up_)
            close_carrier_visit(timestamp, false, "docking");
          }
        else if constexpr(std::same_as<T, events::backpack_change_t>)
          {
          if(not event.Added.empty())
            update_micro_resources = true;
          for(events::backpack_item_t const & item: event.Added)
            {
            auto key{micro_resource_key(item.Name)};
            // the type out of the backpack is the same category as the one used when selling
            if(auto res{db_.store(info::micro_resource_t{
                 .name = key, .id = {}, .localised = item.Name_Localised, .category = item.Type
               })};
               not res)
              spdlog::error("failed to store micro resource {}", item.Name);

            // the place is stored as a market_id, so the economy follows on its own once we learn it
            if(auto res{db_.store(info::micro_acquisition_t{
                 .timestamp = timestamp,
                 .market_id = settlement_market_id_,
                 .name = std::move(key),
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
          if(auto res{db_.store(info::ship_transfer_t{
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
          // the escape pod sends the character home, and another account's ports mean nothing to it
          if(personal_ and info::is_escape_pod_port(event.StationType, event.StationName))
            if(auto res{db_.store(info::port_visit_t{
                 .market_id = event.MarketID,
                 .name = event.StationName,
                 .system = event.StarSystem,
                 .station_type = event.StationType,
                 .visited = timestamp
               })};
               not res)
              spdlog::error("failed to store port visit {}", event.MarketID);

          // a station's identity is rebuilt from journals - the type tells a carrier from a station
          settlement_market_id_ = event.MarketID;
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

          if(auto res{db_.store(station)}; not res)
            spdlog::error("failed to store station {}", event.MarketID);
          ground_cz_.docked(event);
          if(auto res{db_.note_settlement_owner(event.MarketID, event.SystemAddress, event.StationFaction.Name, timestamp)};
             not res)
            spdlog::error("failed to note the owner of {}", event.MarketID);

          // docked at one of our carriers: the hold now is what leaving is measured against
          if(not catching_up_ and event.StationType == "FleetCarrier")
            if(auto carriers{db_.load_carriers()}; carriers)
              if(std::ranges::any_of(*carriers, [&](info::carrier_t const & c) { return c.market_id == event.MarketID; }))
                {
                carrier_visit_t visit{.carrier_id = event.MarketID, .hold = {}};
                for(events::cargo_item_t const & item: cargo.Inventory)
                  {
                  auto & slot{visit.hold[info::commodity_key(item.Name)]};
                  slot.first = item.Name_Localised.empty() ? item.Name : item.Name_Localised;
                  slot.second += item.Count;
                  }
                carrier_visit_ = std::move(visit);
                }
          }
        // conflict zones on foot: where the commander stands, and the kills that tell a zone's intensity
        else if constexpr(std::same_as<T, events::faction_kill_bond_t>)
          {
          if(auto bond{ground_cz_.bond(timestamp, event)}; bond)
            if(auto res{db_.store(*bond)}; not res)
              spdlog::error("failed to store a kill on foot at {}", bond->market_id);
          }
        else if constexpr(std::same_as<T, events::book_dropship_t>)
          ground_cz_.book_dropship(event);
        else if constexpr(std::same_as<T, events::dropship_deploy_t>)
          ground_cz_.dropship_deploy(db_, event);
        else if constexpr(std::same_as<T, events::embark_t>)
          ground_cz_.embark();
        else if constexpr(std::same_as<T, events::died_t>)
          ground_cz_.died();
        else if constexpr(std::same_as<T, events::shipyard_swap_t>)
          {
          // A smaller ship taken at the carrier, so as not to run far across the pad: the big one stays there
          // with its whole hold. The ship taken brings its own hold off the carrier - what it carries on
          // leaving is taken off, as if it had docked empty
          if(not catching_up_ and carrier_visit_ and carrier_visit_->carrier_id == event.MarketID)
            {
            uint64_t const carrier{carrier_visit_->carrier_id};
            close_carrier_visit(timestamp, true, "ship left on carrier");
            carrier_visit_ = carrier_visit_t{.carrier_id = carrier, .hold = {}};
            }
          }
        else if constexpr(std::same_as<T, events::resurrect_t>)
          {
          // leaving a carrier by escape pod leaves the load on it - the journal writes no death for it
          if(not catching_up_)
            close_carrier_visit(timestamp, true, "escape pod");
          }
        // colonisation: whose systems, what a site needs, what came to it
        else if constexpr(std::same_as<T, events::colonisation_system_claim_t>)
        {
        if(auto res{db_.store(info::colony_claim_t{
             .system_address = event.SystemAddress, .system = event.StarSystem, .commander = commander_name_,
             .claimed = timestamp, .released = false
           })};
           not res)
          spdlog::error("failed to store the claim of {}", event.StarSystem);
        }
        else if constexpr(std::same_as<T, events::colonisation_system_claim_release_t>)
        {
        if(auto res{db_.store(info::colony_claim_t{
             .system_address = event.SystemAddress, .system = event.StarSystem, .commander = commander_name_,
             .claimed = timestamp, .released = true
           })};
           not res)
          spdlog::error("failed to store the release of {}", event.StarSystem);
        }
        else if constexpr(std::same_as<T, events::colonisation_construction_depot_t>)
        {
        std::vector<info::construction_need_t> needs;
        for(events::construction_resource_t const & r: event.ResourcesRequired)
          needs.push_back(info::construction_need_t{
            .market_id = event.MarketID, .key = info::commodity_key(r.Name),
            .commodity = r.Name_Localised.empty() ? r.Name : r.Name_Localised, .required = r.RequiredAmount,
            .provided = r.ProvidedAmount, .payment = r.Payment
          });
        // docked at the site - the system we are in is the site's
        if(auto res{db_.store_construction(
             info::construction_depot_t{
               .market_id = event.MarketID, .system_address = system.system_address,
               .progress = event.ConstructionProgress, .complete = event.ConstructionComplete,
               .failed = event.ConstructionFailed, .updated = timestamp
             },
             needs
           )};
           not res)
          spdlog::error("failed to store construction site {}", event.MarketID);
        ++construction_changes_;
        }
        else if constexpr(std::same_as<T, events::colonisation_contribution_t>)
        {
        for(events::construction_contribution_t const & c: event.Contributions)
          if(auto res{db_.store_delivery(info::construction_delivery_t{
               .timestamp = timestamp, .market_id = event.MarketID, .key = info::commodity_key(c.Name),
               .amount = c.Amount, .commander = commander_name_
             })};
             not res)
            spdlog::error("failed to store a delivery to {}", event.MarketID);
        ++construction_changes_;
        }
        // carriers: every move, so that where each is and where it goes can be read back
        else if constexpr(std::same_as<T, events::carrier_jump_request_t>)
        {
        if(auto res{db_.store(info::carrier_movement_t{
             .timestamp = timestamp, .carrier_id = event.CarrierID, .carrier_type = event.CarrierType, .kind = "request",
             .system = event.SystemName, .system_address = event.SystemAddress, .body = event.Body,
             .departure = event.DepartureTime
           })};
           not res)
          spdlog::error("failed to store the jump of carrier {}", event.CarrierID);
        ++carrier_changes_;
        }
        else if constexpr(std::same_as<T, events::carrier_location_t>)
        {
        if(auto res{db_.store(info::carrier_movement_t{
             .timestamp = timestamp, .carrier_id = event.CarrierID, .carrier_type = event.CarrierType, .kind = "location",
             .system = event.StarSystem, .system_address = event.SystemAddress, .body = {}, .departure = {}
           })};
           not res)
          spdlog::error("failed to store the position of carrier {}", event.CarrierID);
        ++carrier_changes_;
        }
        else if constexpr(std::same_as<T, events::carrier_jump_cancelled_t>)
        {
        if(auto res{db_.store(info::carrier_movement_t{
             .timestamp = timestamp, .carrier_id = event.CarrierID, .carrier_type = event.CarrierType, .kind = "cancel",
             .system = {}, .system_address = 0u, .body = {}, .departure = {}
           })};
           not res)
          spdlog::error("failed to store the cancelled jump of carrier {}", event.CarrierID);
        ++carrier_changes_;
        }
        else if constexpr(std::same_as<T, events::carrier_jump_t>)
        {
        // aboard a carrier as it jumps - the carrier is the station, by its market
        if(event.StationType == "FleetCarrier" and event.MarketID != 0u)
          if(auto res{db_.store(info::carrier_movement_t{
               .timestamp = timestamp, .carrier_id = event.MarketID, .carrier_type = {}, .kind = "jump",
               .system = event.StarSystem, .system_address = event.SystemAddress, .body = event.Body, .departure = {}
             })};
             not res)
            spdlog::error("failed to store the jump of carrier {}", event.MarketID);
        ++carrier_changes_;
        }
        else if constexpr(std::same_as<T, events::market_t>)
          {
          info::station_t station{
            .market_id = event.MarketID,
            .system_address = system.system_address,
            .name = event.StationName,
            .station_type = event.StationType,
            .economy = {},
            .government = {}
          };

          // a market's contents exist only in the Market.json beside the journals, and only live
          auto market{load_market(journal_dir_path_)};
          if(not market or market->MarketID != event.MarketID)
            {
            if(auto res{db_.store(station)}; not res)
              spdlog::error("failed to store station {}", event.MarketID);
            return;
            }

          if(auto res{db_.store(station)}; not res)
            {
            spdlog::error("failed to store station {}", event.MarketID);
            return;
            }

          std::vector<info::commodity_t> commodities;
          std::vector<info::market_item_t> items;
          commodities.reserve(market->Items.size());
          items.reserve(market->Items.size());

          for(events::market_commodity_t const & entry: market->Items)
            {
            commodities.emplace_back(
              info::commodity_t{
                .id = entry.id,
                .name = entry.Name_Localised,
                .category = entry.Category_Localised,
                .mean_price = entry.MeanPrice,
                .key = info::commodity_key(entry.Name)
              }
            );
            items.emplace_back(
              info::market_item_t{
                .market_id = event.MarketID,
                .commodity_id = entry.id,
                .buy_price = entry.BuyPrice,
                .sell_price = entry.SellPrice,
                .stock = entry.Stock,
                .demand = entry.Demand,
                .producer = entry.Producer,
                .consumer = entry.Consumer
              }
            );
            }

          if(auto res{db_.replace_market(event.MarketID, market->timestamp, commodities, items)}; not res)
            spdlog::error("failed to store market {}", event.MarketID);
          else
            spdlog::info("market {} at {}: {} items", event.MarketID, event.StationName, items.size());
          }
        else if constexpr(std::same_as<T, events::fss_signal_discovered_t>)
          {
          // a USS expires after a few minutes, in the database it would be nothing but litter
          if(event.TimeRemaining)
            return;

          auto signal{to_system_signal(event, timestamp)};
          if(auto res{db_.store(signal)}; not res)
            spdlog::error("failed to store signal for {}", event.SystemAddress);

          if(auto it{std::ranges::find(system.system_signals, signal.name, &system_signal_t::name)};
             it != system.system_signals.end())
            it->last_seen = signal.last_seen;
          else
            system.system_signals.emplace_back(std::move(signal));

          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fss_discovery_scan_t>)
          {
          // the count is what the scans are measured against - without it "7 bodies" says nothing
          if(system.system_address == event.SystemAddress and system.body_count != event.BodyCount)
            {
            system.body_count = event.BodyCount;
            if(auto res{db_.store_body_count(event.SystemAddress, event.BodyCount)}; not res) [[unlikely]]
              spdlog::error("failed to store body count of {}", event.SystemName);
            }
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>)
          {
          system.fss_complete = true;
          if(not personal_)
            {}
          else if(auto res{db_.store_fss_complete(system.system_address)}; not res) [[unlikely]]
            spdlog::error("failed to update fss scan complete for {}", system.system_address);
          }
        else if constexpr(std::same_as<T, events::scan_bary_centre_t>)
          {
          system.put_bary_centre(
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
                  if(
                    auto it{
                      std::ranges::find(buffered_signals, body.body_id, [](auto const & bs) { return bs.body_id; })
                    };
                    buffered_signals.end() != it
                  )
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
          scanner_body_ = event.BodyName;
          if(stralgo::ends_with(event.BodyName, "Ring"sv))
            {
            // we got BodyID for ring, unknown at fss
            std::string_view planet_name{planet_name_from_ring_name(system.name, event.BodyName)};
            std::string_view ring_name{stralgo::right(event.BodyName, 6)};
            if(auto it{system.body_by_name(planet_name)}; it != system.bodies.end())
              {
              events::body_id_t const parent_planet_id{it->body_id};
              if(
                auto res{db_.store_ring_body_id(system.system_address, parent_planet_id, ring_name, event.BodyID)};
                not res
              ) [[unlikely]]
                spdlog::error("failed to update ring body id for {}:{}", system.system_address, event.BodyName);

              if(
                auto itr{std::ranges::find_if(
                  system.rings,
                  [&parent_planet_id, &ring_name](ring_t const & ring) noexcept -> bool
                  { return ring.parent_body_id == parent_planet_id and ring_name == ring.name; }
                )};
                itr != system.rings.end()
              )
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
            if(not personal_)
              {}
            else if(auto res{db_.store_dss_complete(system.system_address, event.BodyID)}; not res) [[unlikely]]
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
          // an SRV has a hold of its own, and forgotten cargo is what stays on the ship
          if(event.Vessel == "SRV")
            return;

          ship_loadout.CargoUsed = event.Count;
          update_ship = true;

          // the count is in the event, but what exactly is being carried only Cargo.json says
          if(auto file{load_cargo(journal_dir_path_)}; file and file->Vessel != "SRV")
            cargo = std::move(*file);
          else
            spdlog::warn("failed to read Cargo.json");
          }
        else if constexpr(std::same_as<T, events::mission_accepted_t>)
          {
          // replaying the journal meets the same mission again - then it is enough to open it
          if(auto known{db_.mission_exists(event.MissionID)}; known and *known)
            {
            if(auto res{db_.reopen_mission(event.MissionID, event.Expiry)}; not res) [[unlikely]]
              spdlog::error("failed to reopen mission {}", event.MissionID);
            }
          else
            {
            info::mission_t mission{
              .mission_id = event.MissionID,
              .status = info::mission_status_e::accepted,
              .expiry = event.Expiry,
              .faction = event.Faction,
              .type = event.Name,
              .description = event.LocalisedName,
              .reward = event.Reward,
              .market_id = settlement_market_id_,

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

            // a cargo mission says what is needed - without that there is no telling where to get it
            if(not event.Commodity_Localised.empty() and event.Count != 0u)
              if(
                auto res{db_.store(
                  info::mission_cargo_t{
                    .mission_id = event.MissionID, .commodity = event.Commodity_Localised, .count = event.Count
                  }
                )};
                not res
              )
                spdlog::error("failed to store mission cargo for {}", event.MissionID);
            }
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::mission_completed_t>)
          {
          if(auto res{db_.complete_mission(event.MissionID, timestamp, event.Reward)}; not res) [[unlikely]]
            spdlog::error("failed to change mission status for {}", event.MissionID);
          // whom this mission moved and by how much - pluses with a sign, so that pushing strangers out
          // can be counted apart from building one's own up. The game gives no number, the measure is the
          // string's length
          for(events::faction_effect_t const & effect: event.FactionEffects)
            for(events::influence_effect_t const & influence: effect.Influence)
              {
              if(influence.Influence.empty())
                continue;

              auto const magnitude{int32_t(influence.Influence.size())};
              if(auto res{db_.store(info::mission_influence_t{
                   .mission_id = event.MissionID,
                   .timestamp = timestamp,
                   .faction = effect.Faction,
                   .system_address = influence.SystemAddress,
                   .pluses = influence.Trend == "DownBad" ? -magnitude : magnitude
                 })};
                 not res)
                spdlog::error("failed to store mission influence for {}", event.MissionID);
              }
          load_missions();
          update_mission_info = true;

            // rewards go straight to the locker and never appear in the backpack - nothing is counted twice
            for(events::material_reward_t const & reward: event.MaterialsReward)
              {
              // Encoded, Manufactured and Elements are ship materials, not micro resources
              if(reward.Category_Localised != "Data" and reward.Category_Localised != "Item"
                 and reward.Category_Localised != "Component" and reward.Category_Localised != "Consumable")
                continue;
        
              auto key{micro_resource_key(reward.Name)};
              if(auto res{db_.store(info::micro_resource_t{
                   .name = key, .id = {}, .localised = {}, .category = reward.Category_Localised
                 })};
                 not res)
                spdlog::error("failed to store micro resource {}", reward.Name);
        
              update_micro_resources = true;
              if(auto res{db_.store(info::micro_acquisition_t{
                   .timestamp = timestamp,
                   .market_id = settlement_market_id_,
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
          if(auto res{db_.change_mission_status(event.MissionID, info::mission_status_e::abandoned, timestamp)}; not res)
            [[unlikely]]
            [[unlikely]] spdlog::error("failed to change mission status for {}", event.MissionID);
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::mission_failed_t>)
          {
          if(auto res{db_.change_mission_status(event.MissionID, info::mission_status_e::failed, timestamp)}; not res) [[unlikely]]
            [[unlikely]] spdlog::error("failed to change mission status for {}", event.MissionID);
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::mission_redirected_t>)
          {
          if(
            auto res{db_.redirect_mission(
              event.MissionID, event.NewDestinationSystem, event.NewDestinationStation, event.NewDestinationSettlement
            )};
            not res
          ) [[unlikely]]
            spdlog::error("failed to change mission status for {}", event.MissionID);
          load_missions();
          update_mission_info = true;
          }
        else if constexpr(std::same_as<T, events::missions_t>)
          {
          // Replayed after a restart, the snapshot is older than missions taken since - and the
          // MissionAccepted that would open them again is walked past as already written. Its verdict
          // is in the database from the first time round, so all it is needed for now is the list
          if(already_written)
            {
            load_missions();
            update_mission_info = true;
            return;
            }

          for(events::mission_failed_t const & mission: event.Failed)
            if(auto res{db_.change_mission_status(mission.MissionID, info::mission_status_e::failed, timestamp)}; not res)
              [[unlikely]]
              spdlog::warn("failed to change mission status for {}", mission.MissionID);
          for(events::mission_completed_t const & mission: event.Complete)
            if(auto res{db_.change_mission_status(mission.MissionID, info::mission_status_e::completed, timestamp)}; not res)
              [[unlikely]]
              spdlog::warn("failed to change mission status for {}", mission.MissionID);

          // this is the only moment the game says outright what is still open - everything outside that list has closed
          std::vector<uint64_t> active;
          active.reserve(event.Active.size());
          for(events::mission_active_t const & mission: event.Active)
            active.push_back(mission.MissionID);

          if(auto res{db_.expire_missions_outside(active, timestamp)}; not res) [[unlikely]]
            spdlog::warn("failed to expire stale missions");

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
            );
            }
          route_system_visited(current_system_address_);
          route_changed = true;
          }
        else if constexpr(std::same_as<T, events::nav_route_clear_t>)
          {
          route_.clear();
          // the target goes with the route: the game clears both on arrival, and a taxi flight writes no
          // FSDTarget of its own, so a kept one would go on naming a system long since reached
          next_target = {};
          route_changed = true;
          update_system = true;
          }
        // The crosshairs and the fighter, both live only. None of this is written down: a target is
      // gone the moment it is let go, and seventy thousand of them a year would tell a database
      // nothing a screen does not tell better while it still matters.
      else if constexpr(std::same_as<T, events::ship_targeted_t>)
        {
        // every stage repeats what the earlier ones said, so the newest event is the whole truth
        target = event.TargetLocked ? event : events::ship_targeted_t{};
        }
      else if constexpr(std::same_as<T, events::bounty_t>)
        {
        last_bounty = event;
        last_bounty_at = std::chrono::steady_clock::now();
        }
      else if constexpr(std::same_as<T, events::launch_fighter_t>)
        {
        fighter = fighter_e::deployed;
        fighter_crewed = not event.PlayerControlled;
        }
      else if constexpr(std::same_as<T, events::dock_fighter_t>)
        fighter = fighter_e::stowed;
      else if constexpr(std::same_as<T, events::fighter_destroyed_t>)
        fighter = fighter_e::destroyed;
      else if constexpr(std::same_as<T, events::fighter_rebuilt_t>)
        fighter = fighter_e::stowed;
      else if constexpr(std::same_as<T, events::crew_assign_t>)
        {
        // one of the hired crew is on duty at a time, and only that one flies the fighter
        if(event.Role == "Active")
          crew_name = event.Name;
        }
      else if constexpr(std::same_as<T, events::npc_crew_rank_t>)
        {
        if(event.NpcCrewName == crew_name)
          crew_combat_rank = event.RankCombat;
        }
      else if constexpr(std::same_as<T, events::carrier_stats_t>)
          {
          update_micro_resources = true;

          // Joined by the callsign, not by CarrierID - that field means a different thing in each of the
          // two sources: here it is a number (equal to MarketID), in FCMaterials.json it is the callsign
          info::carrier_t carrier{};
          if(auto known{db_.load_carrier(event.Callsign)}; known and *known)
            carrier = std::move(**known);
          else if(auto oid{db_.carrier_oid(event.Callsign)}; oid and *oid)
            carrier.oid = int64_t(**oid);
          else
            carrier.oid = -1;

          // the mark of one's own carrier belongs to the user - a reading of the state has no business
          // taking it off, so it stays as it came from the database
          carrier.market_id = event.CarrierID;
          carrier.carrier_name = event.Name;
          carrier.carrier_id = event.Callsign;
          carrier.carrier_type = event.CarrierType;
          carrier.docking_access = event.DockingAccess;
          carrier.fuel_level = event.FuelLevel;
          carrier.jump_range_curr = event.JumpRangeCurr;
          carrier.jump_range_max = event.JumpRangeMax;
          carrier.total_capacity = event.SpaceUsage.TotalCapacity;
          carrier.free_space = event.SpaceUsage.FreeSpace;
          carrier.cargo = event.SpaceUsage.Cargo;
          carrier.balance = event.Finance.CarrierBalance;
          carrier.available_balance = event.Finance.AvailableBalance;
          carrier.stats_seen = timestamp;

          if(auto res{db_.update_carrier(carrier)}; not res)
            spdlog::error("failed to store carrier stats for {}", event.Callsign);
          else
            spdlog::info(
              "Carrier {} [{}]: fuel {} t, free {} t, balance {}",
              event.Name,
              event.Callsign,
              event.FuelLevel,
              event.SpaceUsage.FreeSpace,
              event.Finance.CarrierBalance
            );
          }
        else if constexpr(std::same_as<T, events::fcmaterials_t>)
          {
          update_micro_resources = true;
          events::fcmaterials_t fcmat{std::move(event)};
          spdlog::info("Carrier: {} mats: {}", fcmat.CarrierID, fcmat.Items.size());
          // carrier_oid( std::string_view name ) -> expected_ec<std::optional<uint64_t>
          auto res{db_.carrier_oid(fcmat.CarrierID)};
          if(not res)
            spdlog::error("failed to retrieve carrier oid for {}", fcmat.CarrierID);
          else
            {
            std::optional<int64_t> carrier_oid{*res};

            // The row is taken whole from the database and only what the shelf reading carries is
            // overwritten. update_carrier writes every column, so building it from scratch would erase the
            // state from CarrierStats - and the mark of one's own carrier, which belongs to the user
            info::carrier_t carrier{};
            if(auto known{db_.load_carrier(fcmat.CarrierID)}; known and *known)
              carrier = std::move(**known);
            else
              carrier.oid = carrier_oid.value_or(-1);

            carrier.market_id = fcmat.MarketID;
            carrier.carrier_name = fcmat.CarrierName;
            carrier.carrier_id = fcmat.CarrierID;
            if(auto res{db_.update_carrier(carrier)}; not res)
              spdlog::error("failed to update carrier info for {}", fcmat.CarrierID);
            else
              {
              if(not carrier_oid)
                {
                auto res{db_.carrier_oid(fcmat.CarrierID)};
                if(not res)
                  spdlog::error("failed to retrieve carrier oid for {}", fcmat.CarrierID);
                else
                  carrier_oid = *res;
                }

              if(carrier_oid)
                {
                for(events::fcmaterial_t const & fmat: fcmat.Items)
                  {
                  // the names repeat at every reading, so they go to the dictionary rather than to the rows
                  if(auto res{db_.store(
                       info::micro_resource_t{
                         .name = micro_resource_key(fmat.Name), .id = fmat.id, .localised = fmat.Name_Localised
                       }
                     )};
                     not res)
                    spdlog::error("failed to store micro resource {}", fmat.id);

                  info::fcmaterial_t mat{
                    .carrier_id = *carrier_oid,
                    .timestamp = fcmat.timestamp.time_since_epoch().count(),
                    .material_id = fmat.id,
                    .price = fmat.Price,
                    .stock = fmat.Stock,
                    .demand = fmat.Demand
                  };
                  if(auto res{db_.store(mat)}; not res)
                    spdlog::error("failed to store material id {} for {}", fmat.id, fcmat.CarrierID);
                  }
                }
              }
            }
          }
      },
      payload
    );

    if(route_changed)
      QMetaObject::invokeMethod(
        parent,
        [target = parent]() mutable
        {
          if(target->route_view_) [[likely]]
            target->route_view_->refresh_ui();
        },
        Qt::QueuedConnection
      );
      // --------------------------
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
    // the mark is moved on from time to time rather than at every line; losing a minute of it only
    // means a minute of journal walked again
    if(not catching_up_ and std::chrono::steady_clock::now() - progress_written_ > progress_write_interval())
      remember_progress();

    // the overlay is given a picture after every batch of events and decides for itself whether anything changed
    QMetaObject::invokeMethod(parent, [target = parent]() mutable { target->publish_overlay(); }, Qt::QueuedConnection);

    if(update_factions)
      {
      load_factions();
      QMetaObject::invokeMethod(
        parent,
        [target = parent]() mutable
        {
          if(target->faction_view_)
            target->faction_view_->refresh_ui();
          if(target->faction_state_view_)
            target->faction_state_view_->refresh_ui();
        },
        Qt::QueuedConnection
      );
      }

    // the summary of finds is a pass over the whole history - worth counting only when finds have been
    // added, not at every change of a faction's influence
    if(update_micro_resources)
      QMetaObject::invokeMethod(
        parent,
        [target = parent]() mutable
        {
          if(target->micro_resource_view_)
            target->micro_resource_view_->refresh_ui();
        },
        Qt::QueuedConnection
      );
    }
  }

void current_state_t::load_factions()
  {
  if(auto res{db_.load_factions()}; not res) [[unlikely]]
    spdlog::warn("failed to load factions");
  else
    known_factions = std::move(*res);
  }

void current_state_t::load_missions()
  {
  if(auto res{db_.load_missions()}; not res) [[unlikely]]
    spdlog::warn("failed to load missions status");
  else
    active_missions = std::move(*res);
  }

namespace
  {
///\brief the gap in hours to one decimal place - minutes are unreadable where ticks are concerned
[[nodiscard]]
auto hours_ago(std::chrono::sys_seconds from, std::chrono::sys_seconds to) -> std::string
  {
  auto const span{std::chrono::duration_cast<std::chrono::minutes>(to - from).count() / 60.0};
  return std::format("{:.1f}h", span);
  }
  }  // namespace

auto describe_tick(
  database_storage_t & db, uint64_t system_address, info::tick_kind_e kind, std::chrono::sys_seconds now
) -> tick_view_t
  {
  // a fortnight is enough to catch the usual gap without dragging the whole history along at every jump
  constexpr uint32_t window_days{14};

  tick_view_t view{.here = "no observations", .galaxy = {}, .awaiting = false};

  auto waves{db.load_recent_ticks(kind, window_days)};
  auto mine{db.last_local_tick(system_address, kind)};
  auto seen{db.last_seen(system_address)};
  auto stats{db.load_tick_stats(kind, window_days)};
  if(not waves or not mine or not stats or not seen)
    {
    spdlog::error("failed to read tick history for {}", system_address);
    return view;
    }

  // The date of the last change, not a span - what gets recalculated moves at the tick and nowhere else,
  // so the change alone is proof that the tick was here. We see it delayed by our own visit, which is why
  // it reads "no earlier than" rather than "exactly then"
  if(*mine)
    // the zone with every hour - the journal and the game run on UTC, the clock on the bar does not
    view.here = std::format("changed {:%d.%m %H:%M} UTC, {} ago", **mine, hours_ago(**mine, now));

  if(not waves->empty())
    {
    auto const & wave{waves->front()};

    // A system recalculates on a clock of its own, so no change since the start of the newest wave means
    // either that it has not reached the system yet or that we have not looked in there since the tick
    view.awaiting = not *mine or **mine < wave.start_begin;
    // a quiet system - a fresh colony above all - can go through a tick with its influence unmoved
    view.seen_since = view.awaiting and *seen and **seen >= wave.start_begin;

    std::string regularity{"too few waves"};
    if(stats->waves > 2u)
      regularity = std::format(
        "every ~{:.0f}h, max {:.0f}h",
        double(stats->typical_gap.count()) / 60.0,
        double(stats->longest_gap.count()) / 60.0
      );

    // no word "galaxy" - the row's caption says it already, and every character here costs width
    view.galaxy = std::format(
      "{:%d.%m %H:%M}-{:%H:%M} UTC ({} sys) - {}", wave.start_begin, wave.end_end, wave.systems, regularity
    );
    }

  return view;
  }

auto current_state_t::close_carrier_visit(std::chrono::sys_seconds when, bool escaped, std::string_view source) -> void
  {
  if(not carrier_visit_)
    return;
  carrier_visit_t const visit{std::move(*carrier_visit_)};
  carrier_visit_.reset();

  // what the hold carries now, by the same key
  std::map<std::string, std::pair<std::string, int64_t>> now;
  if(not escaped)
    for(events::cargo_item_t const & item: cargo.Inventory)
      {
      auto & slot{now[info::commodity_key(item.Name)]};
      slot.first = item.Name_Localised.empty() ? item.Name : item.Name_Localised;
      slot.second += item.Count;
      }

  // on the carrier grows what the hold has less of than at docking, and shrinks what it has more of
  std::set<std::string> keys;
  for(auto const & [key, v]: visit.hold)
    keys.insert(key);
  for(auto const & [key, v]: now)
    keys.insert(key);
  for(std::string const & key: keys)
    {
    auto const before{visit.hold.find(key)};
    auto const after{now.find(key)};
    int64_t const had{before == visit.hold.end() ? 0 : before->second.second};
    int64_t const has{after == now.end() ? 0 : after->second.second};
    std::string const & name{before != visit.hold.end() ? before->second.first : after->second.first};
    if(auto res{db_.change_carrier_cargo(
         info::carrier_cargo_change_t{
           .timestamp = when, .carrier_id = visit.carrier_id, .key = key, .commodity = name, .delta = had - has,
           .source = std::string{source}
         }
       )};
       not res)
      spdlog::error("failed to change the cargo of carrier {}", visit.carrier_id);
    }
  ++carrier_changes_;
  }
