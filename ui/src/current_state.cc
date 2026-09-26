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
    {
    spdlog::error("missing faction oid for {}", event_faction.Name);
    return;
    }

  // obecnosc notujemy zawsze, nawet gdy nic sie nie zmienilo - inaczej frakcja ktora wyleciala
  // z systemu zostaje na liscie na zawsze, bo jej ostatni wpis mowi tylko o ostatniej zmianie
  if(auto res{db.store_faction_seen(faction_oid, system_address, timestamp)}; not res) [[unlikely]]
    spdlog::error("failed to record presence of {} in {}", event_faction.Name, system_address);

  auto last{db.last_influence(faction_oid, system_address)};
  if(not last)
    {
    spdlog::error("failed to load influence for {} in {}", event_faction.Name, system_address);
    return;
    }

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
    spdlog::error("failed to store influence for {} in {}", event_faction.Name, system_address);
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
      {
      spdlog::error("failed to load conflict {} vs {}", record.faction1, record.faction2);
      continue;
      }

    if(*last and **last == record)
      continue;

    // dni wygrane przelicza tick wojen, ktory chodzi wlasnym zegarem - 4 sierpnia 2026 wypadl
    // dwie godziny przed tickiem wplywow, siodmego piec godzin przed nim
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
    // influence jest per system i rejestrowane w czasie, wiec f nie moze byc skonsumowane
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

void current_state_t::handle(std::chrono::sys_seconds timestamp, events::event_holder_t && payload)
  {
  if(nullptr != parent->jlw_)
    {
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
        };

        using T = std::decay_t<decltype(event)>;
        // kariera nalezy do postaci - z sesji cudzego konta bierzemy sam swiat
      if constexpr(
        std::same_as<T, events::mission_accepted_t> or std::same_as<T, events::mission_completed_t>
        or std::same_as<T, events::mission_abandoned_t> or std::same_as<T, events::mission_failed_t>
        or std::same_as<T, events::mission_redirected_t> or std::same_as<T, events::missions_t>
        or std::same_as<T, events::sell_micro_resources_t> or std::same_as<T, events::backpack_change_t>
      )
        if(not personal_)
          return;

      if constexpr(std::same_as<T, events::commander_t>)
        {
        if(owner_fid_.empty())
          if(auto owner{db_.load_owner()}; owner and *owner)
            owner_fid_ = (*owner)->fid;

        personal_ = owner_fid_.empty() or event.FID == owner_fid_;
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
          // kiedy ostatnio patrzylismy na ten system - musi byc odczytane zanim zapis obecnosci
          // przesunie znacznik do przodu, bo to ono zamyka okno ticku od dolu
          std::optional<std::chrono::sys_seconds> previously_seen;
          if(auto seen{db_.last_system_seen(event.SystemAddress)}; seen)
            previously_seen = *seen;
          else
            spdlog::error("failed to read last visit of {}", event.SystemAddress);

          // add/update factions database
          // rozstrzygniecie wojny trzeba znac zanim policzymy wplywy, bo to ono, a nie tick,
          // tlumaczy zmiane widziana w tym samym odczycie
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
          // opis systemu przychodzi tylko z tych dwoch eventow
          if(apply_system_info(system, event))
            {
            if(auto res{db_.update_system_info(system)}; not res)
              spdlog::error("failed to store system info {}", event.SystemAddress);
            update_factions = true;
            }
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

          // kiedy ostatnio patrzylismy na ten system - musi byc odczytane zanim zapis obecnosci
          // przesunie znacznik do przodu, bo to ono zamyka okno ticku od dolu
          std::optional<std::chrono::sys_seconds> previously_seen;
          if(auto seen{db_.last_system_seen(event.SystemAddress)}; seen)
            previously_seen = *seen;
          else
            spdlog::error("failed to read last visit of {}", event.SystemAddress);

          // rozstrzygniecie wojny trzeba znac zanim policzymy wplywy, bo to ono, a nie tick,
          // tlumaczy zmiane widziana w tym samym odczycie
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
          // opis systemu przychodzi tylko z tych dwoch eventow
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
          // gra konczy pobranie probki typem Analyse, wczesniejsze Log i Sample tylko ja zapowiadaja
          bool const analysed{event.ScanType == events::scan_type_e::Analyse};

          // mapowanie daje tylko rodzaj, probka dopowiada gatunek
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

          if(
            auto res{db_.store_genus_species(
              event.SystemAddress,
              event.Body,
              event.Genus_Localised,
              event.Species_Localised,
              analysed and personal_
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
            // kategoria przychodzi tylko tutaj, id i nazwa czytelna od bartendera
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
        // odlot z ladowiska konczy nasza obecnosc w tym miejscu tak samo jak wejscie w supercruise
        else if constexpr(std::same_as<T, events::undocked_t>)
          settlement_market_id_ = 0;
        else if constexpr(std::same_as<T, events::backpack_change_t>)
          {
          if(not event.Added.empty())
            update_micro_resources = true;
          for(events::backpack_item_t const & item: event.Added)
            {
            auto key{micro_resource_key(item.Name)};
            // typ z plecaka to ta sama kategoria co przy sprzedazy
            if(auto res{db_.store(info::micro_resource_t{
                 .name = key, .id = {}, .localised = item.Name_Localised, .category = item.Type
               })};
               not res)
              spdlog::error("failed to store micro resource {}", item.Name);

            // miejsce zapisujemy jako market_id, wiec ekonomia dojdzie sama gdy ja poznamy
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
        else if constexpr(std::same_as<T, events::docked_t>)
          {
          // tozsamosc stacji odtwarzamy z journali - typ rozroznia flotowiec od stacji
          settlement_market_id_ = event.MarketID;
          info::station_t station{
            .market_id = event.MarketID,
            .system_address = event.SystemAddress,
            .name = event.StationName,
            .station_type = event.StationType,
            .economy = event.StationEconomy_Localised,
            .government = event.StationGovernment_Localised,
            .controlling_faction = event.StationFaction.Name
          };

          if(auto res{db_.store(station)}; not res)
            spdlog::error("failed to store station {}", event.MarketID);
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

          // zawartosc rynku istnieje tylko w Market.json obok journali i tylko na zywo
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
                .mean_price = entry.MeanPrice
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
          // USS wygasa po kilku minutach, w bazie bylby tylko smieciem
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
          // SRV ma wlasna ladownie, a zapomniany ladunek to ten, ktory zostaje na statku
          if(event.Vessel == "SRV")
            return;

          ship_loadout.CargoUsed = event.Count;
          update_ship = true;

          // licznik jest w evencie, ale co konkretnie wiozimy mowi dopiero Cargo.json
          if(auto file{load_cargo(journal_dir_path_)}; file and file->Vessel != "SRV")
            cargo = std::move(*file);
          else
            spdlog::warn("failed to read Cargo.json");
          }
        else if constexpr(std::same_as<T, events::mission_accepted_t>)
          {
          // powtorne odtworzenie journala trafia na te sama misje - wtedy wystarczy ja otworzyc
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

            // misja towarowa mowi czego trzeba - bez tego nie da sie podpowiedziec skad to wziac
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
          // kogo ta misja ruszyla i o ile - plusy ze znakiem, zeby wypychanie obcych frakcji dalo sie
          // policzyc osobno od budowania wlasnych. Gra nie podaje liczby, miara jest dlugosc ciagu
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

            // nagrody ida prosto do lockera, w plecaku sie nie pojawiaja - zadnego dublowania
            for(events::material_reward_t const & reward: event.MaterialsReward)
              {
              // Encoded, Manufactured i Elements to materialy statku, nie mikrozasoby
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
          for(events::mission_failed_t const & mission: event.Failed)
            if(auto res{db_.change_mission_status(mission.MissionID, info::mission_status_e::failed, timestamp)}; not res)
              [[unlikely]]
              spdlog::warn("failed to change mission status for {}", mission.MissionID);
          for(events::mission_completed_t const & mission: event.Complete)
            if(auto res{db_.change_mission_status(mission.MissionID, info::mission_status_e::completed, timestamp)}; not res)
              [[unlikely]]
              spdlog::warn("failed to change mission status for {}", mission.MissionID);

          // to jedyny moment gdy gra mowi wprost co jeszcze wisi - wszystko poza ta lista juz sie zamknelo
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
          route_changed = true;
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

            // znacznik wlasnego flotowca ustawia uzytkownik, odczyt cen nie ma prawa go zdjac
            bool tracked{};
            if(auto known{db_.load_carrier(fcmat.CarrierID)}; known and *known)
              tracked = (*known)->tracked;

            info::carrier_t carrier{
              .oid = carrier_oid.value_or(-1),
              .market_id = fcmat.MarketID,
              .carrier_name = fcmat.CarrierName,
              .carrier_id = fcmat.CarrierID,
              .tracked = tracked
            };
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
                  // nazwy powtarzaja sie w kazdym odczycie, wiec ida do slownika a nie do wierszy
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
    // overlay dostaje obraz po kazdej paczce zdarzen, a sam decyduje czy cokolwiek sie zmienilo
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

    // podsumowanie zdobyczy to przebieg przez cala historie - warto je liczyc tylko gdy przybylo zdobyczy,
    // a nie przy kazdej zmianie wplywow frakcji
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
///\brief odstep w godzinach z jednym miejscem po przecinku - minuty przy tickach sa nieczytelne
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
  // dwa tygodnie wystarcza zeby zlapac typowa przerwe, a nie ciagna calej historii przy kazdym skoku
  constexpr uint32_t window_days{14};

  tick_view_t view{.here = "brak obserwacji", .galaxy = {}, .awaiting = false};

  auto waves{db.load_recent_ticks(kind, window_days)};
  auto mine{db.load_system_ticks(system_address, kind, window_days)};
  auto stats{db.load_tick_stats(kind, window_days)};
  if(not waves or not mine or not stats)
    {
    spdlog::error("failed to read tick history for {}", system_address);
    return view;
    }

  if(not mine->empty())
    {
    auto const & last{mine->front()};
    view.here = std::format(
      "{:%d.%m %H:%M}-{:%H:%M}, {} temu", last.window_begin, last.window_end, hours_ago(last.window_end, now)
    );
    }

  if(not waves->empty())
    {
    auto const & wave{waves->front()};

    // system przelicza sie wlasnym zegarem, wiec brak go w najswiezszej fali znaczy tylko tyle,
    // ze jeszcze do niego nie doszla albo ze jeszcze tam nie zagladalismy
    view.awaiting = mine->empty() or mine->front().window_end < wave.start_begin;

    std::string regularity{"za malo fal na wzorzec"};
    if(stats->waves > 2u)
      regularity = std::format(
        "typowo co {:.1f}h, najdluzej {:.1f}h",
        double(stats->typical_gap.count()) / 60.0,
        double(stats->longest_gap.count()) / 60.0
      );

    view.galaxy = std::format(
      "galaktyka {:%d.%m %H:%M}-{:%H:%M} ({} sys), {}",
      wave.start_begin,
      wave.end_end,
      wave.systems,
      regularity
    );
    }

  return view;
  }
