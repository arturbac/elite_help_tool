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

///\brief najdluzsze okno, ktore jeszcze cos mowi o porze ticku
///
/// Przy dluzszej przerwie miedzy odczytami przedzial obejmuje pol doby i przeciecie z nim niczego
/// nie zawezi, a zasmieca tabele
constexpr std::chrono::hours max_tick_window{24};

///\brief zapisuje slad po ticku, gdy sledzona wartosc zmienila sie miedzy dwoma odczytami systemu
void note_tick(
  database_storage_t & db,
  info::tick_kind_e kind,
  uint64_t system_address,
  std::optional<std::chrono::sys_seconds> previously_seen,
  std::chrono::sys_seconds timestamp
)
  {
  // bez poprzedniego odczytu nie ma czym ograniczyc okna - pierwsze spojrzenie na system nic nie mowi
  if(not previously_seen or *previously_seen >= timestamp or timestamp - *previously_seen > max_tick_window)
    return;

  if(
    auto res{db.store(info::tick_observation_t{
      .kind = kind, .system_address = system_address, .window_begin = *previously_seen, .window_end = timestamp
    })};
    not res
  )
    spdlog::error("failed to store tick observation for {}", system_address);
  }

///\brief czy ktoras z wojen w tym systemie wlasnie sie rozstrzygnela
///
/// Koniec wojny rozdziela udzialy pokonanej frakcji od razu, poza dobowym przeliczeniem, wiec
/// zmiana wplywow widziana w tym samym odczycie nie jest sladem ticku i nie wolno jej tak liczyc
[[nodiscard]]
auto war_settled_now(
  database_storage_t & db, uint64_t system_address, std::span<events::conflict_t const> conflicts
) -> bool
  {
  for(events::conflict_t const & conflict: conflicts)
    {
    auto const record{info::to_conflict(system_address, {}, conflict)};

    // pusty status znaczy "juz po wojnie"; interesuje nas tylko przejscie w ten stan
    if(not record.status.empty())
      continue;

    auto last{db.last_conflict(system_address, record.faction1, record.faction2)};
    if(last and *last and not (*last)->status.empty())
      return true;
    }

  return false;
  }

///\brief rejestruje influence frakcji w systemie, tylko gdy zmienila sie wzgledem ostatniego wpisu
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

  // obecnosc notujemy zawsze, nawet gdy nic sie nie zmienilo - inaczej frakcja ktora wyleciala
  // z systemu zostaje na liscie na zawsze, bo jej ostatni wpis mowi tylko o ostatniej zmianie
  if(auto res{db.store_faction_seen(faction_oid, system_address, timestamp)}; not res) [[unlikely]]
    spdlog::error("failed to record presence of {} in {}", event_faction.Name, system_address);

  auto last{db.last_influence(faction_oid, system_address)};
  if(not last)
    critical_abort("failed to load influence for {} in {}", event_faction.Name, system_address);

  auto record{info::to_influence(faction_oid, system_address, timestamp, event_faction)};

  // sam wplyw, bez stanow - stany potrafia sie zmienic poza tickiem, a wplyw przelicza sie wylacznie
  // przy nim, wiec tylko on wyznacza okno
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

///\brief rejestruje konflikty w systemie, tylko gdy ich stan sie zmienil
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

    // dni wygrane przelicza tick wojen, ktory chodzi wlasnym zegarem - 4 sierpnia 2026 wypadl
    // dwie godziny przed tickiem wplywow, siodmego piec godzin przed nim
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
    // influence jest per system i rejestrowane w czasie, wiec f nie moze byc skonsumowane
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
  }  // namespace

void database_import_state_t::handle(std::chrono::sys_seconds timestamp, events::event_holder_t && e)
  {
  state_t & state{*this->state};

  std::visit(
    [&state, timestamp]<typename T>(T & event)
    {
      // kariera nalezy do postaci - z journala cudzego konta bierzemy sam swiat, a te zdarzenia
      // mowia wylacznie o tym, co robil gracz, wiec w cudzej bazie nie maja czego opisywac
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
        // opis systemu przychodzi tylko z tych dwoch eventow
        if(apply_system_info(state.system, event))
          if(auto res{state.db_.update_system_info(state.system)}; not res)
            critical_abort("failed to store system info {}", event.SystemAddress);

        // kiedy ostatnio patrzylismy na ten system - musi byc odczytane zanim zapis obecnosci
        // przesunie znacznik do przodu, bo to ono zamyka okno ticku od dolu
        auto previously_seen{state.db_.last_system_seen(event.SystemAddress)};
        if(not previously_seen) [[unlikely]]
          critical_abort("failed to read last visit of {}", event.SystemAddress);

        // rozstrzygniecie wojny trzeba znac zanim policzymy wplywy, bo to ono, a nie tick,
        // tlumaczy zmiane widziana w tym samym odczycie
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
        if(state.system.system_address != event.SystemAddress)
          critical_abort("jump without start jump {}", state.system.system_address, event.SystemAddress);
        else if(state.system.system_location != event.StarPos)
          {
          state.system.system_location = event.StarPos;
          if(
            auto res{state.db_.store_system_location(state.system.system_address, state.system.system_location)};
            not res
          )
            critical_abort("failed to store system location {}", state.system.system_address);
          }
        // opis systemu przychodzi tylko z tych dwoch eventow
        if(apply_system_info(state.system, event))
          if(auto res{state.db_.update_system_info(state.system)}; not res)
            critical_abort("failed to store system info {}", event.SystemAddress);

        // kiedy ostatnio patrzylismy na ten system - musi byc odczytane zanim zapis obecnosci
        // przesunie znacznik do przodu, bo to ono zamyka okno ticku od dolu
        auto previously_seen{state.db_.last_system_seen(event.SystemAddress)};
        if(not previously_seen) [[unlikely]]
          critical_abort("failed to read last visit of {}", event.SystemAddress);

        // rozstrzygniecie wojny trzeba znac zanim policzymy wplywy, bo to ono, a nie tick,
        // tlumaczy zmiane widziana w tym samym odczycie
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
        // gra konczy pobranie probki typem Analyse, wczesniejsze Log i Sample tylko ja zapowiadaja
        bool const analysed{event.ScanType == events::scan_type_e::Analyse};

        // mapowanie daje tylko rodzaj, probka dopowiada gatunek
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
          // kategoria przychodzi tylko tutaj, id i nazwa czytelna od bartendera
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
        // przylot taksowka bywa jedynym sladem, ze jestesmy w tej osadzie
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
      // odlot z ladowiska konczy nasza obecnosc w tym miejscu tak samo jak supercruise
      else if constexpr(std::same_as<T, events::undocked_t>)
        state.settlement_market_id = 0;
      else if constexpr(std::same_as<T, events::backpack_change_t>)
        {
        for(events::backpack_item_t const & item: event.Added)
          {
          auto key{micro_resource_key(item.Name)};
          // typ z plecaka to ta sama kategoria co przy sprzedazy
          if(auto res{state.db_.store(info::micro_resource_t{
               .name = key, .id = {}, .localised = item.Name_Localised, .category = item.Type
             })};
             not res)
            spdlog::error("failed to store micro resource {}", item.Name);

          // miejsce zapisujemy jako market_id, wiec ekonomia dojdzie sama gdy ja poznamy
          if(auto res{state.db_.store(info::micro_acquisition_t{
               .timestamp = timestamp, .market_id = state.settlement_market_id, .name = std::move(key),
               .count = item.Count,
               .source = info::acquisition_source_e::collected
             })};
             not res)
            spdlog::error("failed to store acquisition {}", item.Name);
          }
        }
      else if constexpr(std::same_as<T, events::docked_t>)
        {
        // tozsamosc stacji odtwarzamy z journali - typ rozroznia flotowiec od stacji
        state.settlement_market_id = event.MarketID;
        info::station_t station{
          .market_id = event.MarketID,
          .system_address = event.SystemAddress,
          .name = event.StationName,
          .station_type = event.StationType,
          .economy = event.StationEconomy_Localised,
          .government = event.StationGovernment_Localised,
          .controlling_faction = event.StationFaction.Name
        };

        if(auto res{state.db_.store(station)}; not res) [[unlikely]]
          critical_abort("failed to store station {}", event.MarketID);
        }
      else if constexpr(std::same_as<T, events::market_t>)
        {
        // zawartosc rynku istnieje tylko w Market.json i tylko na zywo, import zna sama stacje
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
        // USS wygasa po kilku minutach, w bazie bylby tylko smieciem
        if(event.TimeRemaining)
          return;

        if(auto res{state.db_.store(to_system_signal(event, timestamp))}; not res) [[unlikely]]
          critical_abort("failed to store signal for {}", event.SystemAddress);
        }
      else if constexpr(std::same_as<T, events::commander_t>)
        {
        // od tej chwili az do konca pliku wiadomo, czyje sa wpisy
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

        // misja towarowa mowi czego trzeba - bez tego nie da sie podpowiedziec skad to wziac
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
        // kogo ta misja ruszyla i o ile - plusy ze znakiem, zeby wypychanie obcych frakcji dalo sie
        // policzyc osobno od budowania wlasnych. Gra nie podaje liczby, miara jest dlugosc ciagu
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

          // nagrody ida prosto do lockera, w plecaku sie nie pojawiaja - zadnego dublowania
          for(events::material_reward_t const & reward: event.MaterialsReward)
            {
            // Encoded, Manufactured i Elements to materialy statku, nie mikrozasoby
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

        // to jedyny moment gdy gra mowi wprost co jeszcze wisi - wszystko poza ta lista juz sie zamknelo
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
        // ignored in import
        }
      else if constexpr(std::same_as<T, events::fcmaterials_t>)
        {
        // ignored in import
        }
    },
    e
  );
  }
