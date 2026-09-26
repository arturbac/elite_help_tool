#include <overlay_feed.h>
#include <qformat.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <functional>
#include <cctype>
#include <format>
#include <map>
#include <set>
#include <ranges>

namespace
  {
constexpr uint32_t colour_heading{0x9ad1ffu};
constexpr uint32_t colour_plain{0xddddddu};
constexpr uint32_t colour_alert{0xd9a34au};
///\brief nieodkryte przez nikogo - to jest ten przypadek, dla ktorego warto sie zatrzymac
constexpr uint32_t colour_first{0x3cb371u};
constexpr uint32_t colour_expiring{0xd9534fu};

///\brief bloki gasna gdy narzedzie zamilknie - lepiej brak napisu niz napis sprzed godziny
constexpr uint32_t block_ttl_ms{10000u};
///\brief niezmieniony obraz i tak trzeba powtarzac, inaczej wygasnie graczowi stojacemu w miejscu
constexpr std::chrono::seconds heartbeat{3};

///\brief ponizej tego progu schodzenie do ciala nie zwraca sie czasowo
constexpr uint32_t minimum_body_value{300000u};
///\brief pas boczny ma swoje granice, dluga lista i tak nie zostanie przeczytana w locie
constexpr size_t listed_bodies{5u};
constexpr size_t listed_factions{5u};
constexpr size_t listed_missions{6u};
constexpr size_t listed_cargo{4u};
constexpr size_t listed_commodities{3u};
///\brief ponizej tej rezerwy czasu misja jest juz problemem, a nie planem
constexpr std::chrono::hours expiry_warning{3};
///\brief mniejszych odchylek od sredniej galaktycznej nie warto pokazywac
constexpr double interesting_deviation{0.25};
///\brief procent bez kwoty klamie - 93% taniej na towarze za 20 Cr to oszczednosc bez znaczenia
constexpr uint32_t interesting_margin{500u};
///\brief influence aktualizuje sie raz na dobe, czesciej pytac nie ma po co
constexpr std::chrono::seconds faction_refresh{60};
///\brief rynek moze pojawic sie w kazdej chwili, gdy gracz go otworzy
constexpr std::chrono::seconds market_refresh{5};
///\brief misje dochodza rzadko, a zapytanie idzie przez obie bazy
constexpr std::chrono::seconds supply_refresh{10};
constexpr size_t listed_sources{2u};
constexpr unsigned listed_trades{3u};

///\brief port w kosmosie ma najwiecej towaru, osada najmniej - taka jest kolejnosc oplacalnosci
[[nodiscard]]
auto station_rank(std::string_view station_type) -> int
  {
  using namespace std::string_view_literals;
  constexpr std::array space{
    "Coriolis"sv,
    "Orbis"sv,
    "Ocellus"sv,
    "Dodec"sv,
    "AsteroidBase"sv,
    "MegaShip"sv,
    "Outpost"sv,
    "SpaceConstructionDepot"sv
  };
  constexpr std::array planetary{
    "CraterPort"sv, "CraterOutpost"sv, "SurfaceStation"sv, "PlanetaryConstructionDepot"sv, "DockablePlanetStation"sv
  };

  if(std::ranges::contains(space, station_type))
    return 0;
  if(std::ranges::contains(planetary, station_type))
    return 1;
  if(station_type == "OnFootSettlement")
    return 2;
  return 3;
  }

///\brief te same barwy co w oknie reputacji - czerwony federacja, niebieski imperium, zielony alians
[[nodiscard]]
auto allegiance_colour(info::allegiance_e allegiance) -> uint32_t
  {
  using enum info::allegiance_e;
  switch(allegiance)
    {
    case federation: return 0xd9534fu;
    case empire:     return 0x4a90d9u;
    case alliance:   return 0x3cb371u;
    default:         return colour_plain;
    }
  }

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

///\brief w nazwie ciala gra powtarza nazwe systemu - na pasie bocznym to sama strata miejsca
[[nodiscard]]
auto short_body_name(std::string const & system_name, std::string const & body_name) -> std::string
  {
  if(body_name.size() > system_name.size() + 1u and body_name.starts_with(system_name))
    return body_name.substr(system_name.size() + 1u);
  return body_name;
  }

///\brief warto zejsc tylko po to, czego jeszcze nie zmapowalismy i co cos daje
[[nodiscard]]
auto worth_mapping(body_t const & body) -> bool
  {
  auto const * const planet{std::get_if<planet_details_t>(&body.details)};
  return planet != nullptr and not planet->mapped and body.value >= minimum_body_value;
  }

[[nodiscard]]
auto describe_exploration(star_system_t const & system) -> std::vector<overlay::line_t>
  {
  std::vector<body_t const *> candidates;
  for(body_t const & body: system.bodies)
    if(worth_mapping(body))
      candidates.push_back(&body);

  if(candidates.empty())
    return {};

  std::ranges::sort(candidates, std::ranges::greater{}, [](body_t const * body) { return body->value; });

  uint64_t total{};
  for(body_t const * body: candidates)
    total += body->value;

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = std::format("worth mapping: {} bodies, {} Cr", candidates.size(), format_credits_value(uint32_t(total))),
      .color = colour_heading
    }
  );

  for(body_t const * body: candidates | std::views::take(listed_bodies))
    {
    auto const * const planet{std::get_if<planet_details_t>(&body->details)};
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{}  {} Cr  {:.0f} ls{}",
          short_body_name(system.name, body->name),
          format_credits_value(body->value),
          body->distance_from_arrival_ls,
          planet != nullptr and planet->landable ? "  landable" : ""
        ),
        // pierwsze odkrycie to premia, ktorej nie da sie odzyskac pozniej
        .color = body->was_discovered ? colour_plain : colour_first
      }
    );
    }

  if(candidates.size() > listed_bodies)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", candidates.size() - listed_bodies), .color = colour_plain}
    );

  return lines;
  }

///\brief nazwa wewnetrzna jest czytelna, ale brzydka - wielka litera wystarczy gdy brak tlumaczenia
[[nodiscard]]
auto readable_name(events::cargo_item_t const & item) -> std::string
  {
  if(not item.Name_Localised.empty())
    return item.Name_Localised;

  std::string name{item.Name};
  if(not name.empty())
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
  return name;
  }

///\brief ladunek zostawiony na pokladzie blokuje wezwanie statku na lądowisko osady
[[nodiscard]]
auto describe_cargo(events::cargo_file_t const & cargo) -> std::vector<overlay::line_t>
  {
  if(cargo.timestamp == std::chrono::sys_seconds{})
    return {};

  std::vector<overlay::line_t> lines;

  if(cargo.Count == 0u)
    {
    lines.push_back(overlay::line_t{.text = "cargo: empty", .color = colour_plain});
    return lines;
    }

  lines.push_back(overlay::line_t{.text = std::format("cargo: {} t", cargo.Count), .color = colour_alert});

  auto sorted{cargo.Inventory};
  std::ranges::sort(sorted, std::ranges::greater{}, &events::cargo_item_t::Count);

  for(events::cargo_item_t const & item: sorted | std::views::take(listed_cargo))
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "  {}  {} t{}",
          readable_name(item),
          item.Count,
          item.Stolen != 0u ? std::format("  {} stolen", item.Stolen) : ""
        ),
        .color = item.Stolen != 0u ? colour_expiring : colour_plain
      }
    );

  if(sorted.size() > listed_cargo)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", sorted.size() - listed_cargo), .color = colour_plain}
    );

  return lines;
  }

///\brief zostalo mniej niz godzina to inny rodzaj wiadomosci niz zostalo pare dni
[[nodiscard]]
auto format_remaining(std::chrono::seconds left) -> std::string
  {
  if(left <= std::chrono::seconds::zero())
    return "expired";

  auto const days{std::chrono::duration_cast<std::chrono::days>(left)};
  auto const hours{std::chrono::duration_cast<std::chrono::hours>(left - days)};
  if(days.count() != 0)
    return std::format("{}d {}h", days.count(), hours.count());

  auto const minutes{std::chrono::duration_cast<std::chrono::minutes>(left - hours)};
  if(hours.count() != 0)
    return std::format("{}h {}m", hours.count(), minutes.count());

  return std::format("{}m", minutes.count());
  }

///\brief misja przekierowana jest zrobiona i czeka tylko na oddanie - to inna kategoria niz reszta
[[nodiscard]]
auto describe_missions(std::vector<info::mission_t> const & missions) -> std::vector<overlay::line_t>
  {
  std::vector<info::mission_t const *> open;
  for(info::mission_t const & mission: missions)
    if(mission.status == info::mission_status_e::accepted or mission.status == info::mission_status_e::redirected)
      open.push_back(&mission);

  if(open.empty())
    return {};

  std::ranges::sort(open, {}, [](info::mission_t const * mission) { return mission->expiry; });

  auto const ready{std::ranges::count_if(
    open, [](info::mission_t const * mission) { return mission->status == info::mission_status_e::redirected; }
  )};

  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = ready == 0 ? std::format("missions: {} open", open.size())
                         : std::format("missions: {} open, {} to hand in", open.size(), ready),
      .color = colour_heading
    }
  );

  for(info::mission_t const * mission: open | std::views::take(listed_missions))
    {
    auto const left{std::chrono::duration_cast<std::chrono::seconds>(mission->expiry - now)};
    bool const done{mission->status == info::mission_status_e::redirected};

    auto const where{
      done ? (mission->redirected_station.empty() ? mission->redirected_system : mission->redirected_station)
           : (mission->destination_station.empty() ? mission->destination_system : mission->destination_station)
    };

    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{}{}  {}{}{}",
          done ? "> " : "  ",
          mission->faction,
          where.empty() ? std::string{"-"} : where,
          "  ",
          format_remaining(left)
        ),
        // zielone jest do oddania, czerwone zaraz przepadnie
        .color = left < expiry_warning ? colour_expiring : (done ? colour_first : colour_plain)
      }
    );
    }

  if(open.size() > listed_missions)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", open.size() - listed_missions), .color = colour_plain}
    );

  return lines;
  }

[[nodiscard]]
auto describe_system(star_system_t const & system, bool with_controlling) -> std::vector<overlay::line_t>
  {
  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = system.name, .color = colour_heading});

  // gdy mamy influence, frakcja kontrolujaca jest tam oznaczona gwiazdka i nie ma po co jej powtarzac
  if(with_controlling and not system.controlling_faction.empty())
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

overlay_feed_t::overlay_feed_t(std::string socket_path, std::string db_path) :
    server_{std::make_unique<overlay::server_t>(std::move(socket_path))},
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("overlay feed: failed to open {}", db_path);

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

auto overlay_feed_t::refresh_factions(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  bool const same_system{state.current_system_address_ == factions_system_};

  // influence rusza sie raz na dobe, wiec odpytywanie bazy co ramke byloby marnotrawstwem
  if(same_system and now - factions_loaded_ < faction_refresh)
    return;

  factions_system_ = state.current_system_address_;
  factions_loaded_ = now;
  faction_lines_.clear();
  conflict_lines_.clear();

  if(factions_system_ == 0u)
    return;

  if(auto conflicts{db_.load_conflicts(factions_system_)}; conflicts)
    {
    // baza trzyma cala historie wpisow, a na ekranie ma byc obecny stan kazdej pary frakcji
    std::map<std::pair<std::string, std::string>, info::conflict_t const *> latest_conflict;
    for(info::conflict_t const & conflict: *conflicts)
      {
      auto & slot{latest_conflict[{conflict.faction1, conflict.faction2}]};
      if(slot == nullptr or slot->timestamp < conflict.timestamp)
        slot = &conflict;
      }

    for(auto const & [pair, entry]: latest_conflict)
      {
      info::conflict_t const & conflict{*entry};
      // zakonczone i dopiero zapowiedziane nie zmieniaja tego, co mam robic teraz
      if(conflict.status != "active")
        continue;

      conflict_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "{}: {} {} - {} {}",
            conflict.war_type,
            conflict.faction1,
            conflict.won_days1,
            conflict.won_days2,
            conflict.faction2
          ),
          .color = colour_alert
        }
      );

      if(not conflict.stake1.empty() or not conflict.stake2.empty())
        conflict_lines_.push_back(
          overlay::line_t{
            .text = std::format(
              "  stake: {} / {}",
              conflict.stake1.empty() ? "-" : conflict.stake1,
              conflict.stake2.empty() ? "-" : conflict.stake2
            ),
            .color = colour_plain
          }
        );
      }
    }

  auto history{db_.load_influence_history(factions_system_)};
  if(not history)
    {
    spdlog::error("overlay feed: failed to load influence for {}", factions_system_);
    return;
    }

  // frakcja, ktora wyleciala z systemu, przestaje pojawiac sie w odczytach, ale jej ostatni wpis
  // influence zostaje - dlatego liste zawezamy do tych widzianych przy najswiezszym odczycie
  std::set<int64_t> present;
  if(auto refs{db_.load_present_factions(factions_system_)}; refs)
    for(info::faction_ref_t const & ref: *refs)
      present.insert(ref.faction_oid);

  // ostatni wpis kazdej frakcji to jej obecny stan w systemie
  std::map<int64_t, info::faction_influence_t const *> latest;
  for(info::faction_influence_t const & entry: *history)
    {
    // pusty zbior znaczy ze dla tego systemu nie mamy jeszcze sladu obecnosci - wtedy pokazujemy wszystko
    if(not present.empty() and not present.contains(entry.faction_oid))
      continue;
    latest[entry.faction_oid] = &entry;
    }

  struct presence_t
    {
    std::string name;
    info::allegiance_e allegiance;
    std::string active;
    double influence;
    };

  std::vector<presence_t> presence;
  presence.reserve(latest.size());
  for(auto const & [oid, entry]: latest)
    {
    presence_t item{
      .name = {},
      .allegiance = info::allegiance_e::unknown,
      .active = not entry->active_states.empty() ? entry->active_states
                : entry->faction_state == "None" ? std::string{}
                                                 : entry->faction_state,
      .influence = entry->influence
    };

    if(
      auto it{std::ranges::find(state.known_factions, oid, &info::faction_info_t::oid)};
      it != state.known_factions.end()
    )
      {
      item.name = it->name;
      item.allegiance = it->allegiance;
      }

    if(not item.name.empty())
      presence.emplace_back(std::move(item));
    }

  std::ranges::sort(presence, std::ranges::greater{}, &presence_t::influence);

  for(presence_t const & item: presence | std::views::take(listed_factions))
    faction_lines_.push_back(
      overlay::line_t{
        // gwiazdka wyroznia frakcje kontrolujaca, bo to ona decyduje o obliczu systemu
        .text = std::format(
          "{}{}  {:.1f}%{}{}",
          item.name == state.system.controlling_faction ? "* " : "  ",
          item.name,
          item.influence * 100.0,
          item.active.empty() ? "" : "  ",
          item.active
        ),
        .color = allegiance_colour(item.allegiance)
      }
    );
  }

auto overlay_feed_t::refresh_market(uint64_t market_id, uint32_t cargo_capacity) -> void
  {
  // samo miejsce nie wystarczy jako klucz: w osadzie jestesmy od wejscia, a towary poznajemy
  // dopiero gdy gracz otworzy rynek, wiec pytanie raz przy zmianie miejsca zawsze trafialo w pustke
  auto const now{std::chrono::steady_clock::now()};
  if(market_id == market_id_ and now - market_loaded_ < market_refresh)
    return;

  market_id_ = market_id;
  market_loaded_ = now;
  market_lines_.clear();

  if(market_id == 0u)
    return;

  auto station{db_.load_station(market_id)};
  std::string const name{station and *station ? (*station)->name : std::format("market {}", market_id)};

  auto entries{db_.load_market_entries(market_id)};
  if(not entries or entries->empty())
    {
    // zdarzenie Market powstaje dopiero po otwarciu ekranu towarow - milczenie w tym miejscu
    // wygladaloby jak brak okazji, a znaczy tylko tyle, ze nie mielismy czego zapisac
    market_lines_.push_back(overlay::line_t{.text = std::format("{}: no market data", name), .color = colour_alert});
    market_lines_.push_back(overlay::line_t{.text = "  open the commodity market to record it", .color = colour_plain});
    return;
    }

  // odchylenie od sredniej galaktycznej to jedyna liczba mowiaca czy cena jest okazja
  auto const sell_gain{
    [](info::market_entry_t const & entry) -> double
    {
      if(entry.mean_price == 0u or entry.demand == 0u)
        return 0.0;
      return (double(entry.sell_price) - double(entry.mean_price)) / double(entry.mean_price);
    }
  };
  auto const buy_gain{
    [](info::market_entry_t const & entry) -> double
    {
      if(entry.mean_price == 0u or entry.stock == 0u or entry.buy_price == 0u)
        return 0.0;
      return (double(entry.mean_price) - double(entry.buy_price)) / double(entry.mean_price);
    }
  };

  // rynek odczytany przed dolozeniem flag ma wszedzie zera - wtedy flagom nie mozna wierzyc
  bool const flags_known{
    std::ranges::any_of(*entries, [](info::market_entry_t const & entry) { return entry.producer or entry.consumer; })
  };

  std::vector<info::market_entry_t const *> sells;
  std::vector<info::market_entry_t const *> buys;
  for(info::market_entry_t const & entry: *entries)
    {
    // gra podaje cene takze dla towarow, ktorymi stacja nie handluje - bez tych flag Platinum
    // ze Scott View wygladal jak okazja, choc nie byl ani sprzedawany, ani skupowany.
    // rynki zapisane przed dolozeniem flag maja je zerowe, wiec dla nich wracamy do zapasu i popytu
    bool const buys_it{flags_known ? entry.consumer : entry.demand > 0u};
    bool const sells_it{flags_known ? entry.producer : entry.stock > 0u};

    if(
      buys_it and sell_gain(entry) >= interesting_deviation and entry.sell_price > entry.mean_price + interesting_margin
    )
      sells.push_back(&entry);
    if(
      sells_it and buy_gain(entry) >= interesting_deviation and entry.buy_price + interesting_margin < entry.mean_price
    )
      buys.push_back(&entry);
    }

  // ladownia ma stala pojemnosc, wiec o wyborze decyduje kwota na tonie, nie procent
  std::ranges::sort(
    sells, std::ranges::greater{}, [](info::market_entry_t const * e) { return e->sell_price - e->mean_price; }
  );
  std::ranges::sort(
    buys, std::ranges::greater{}, [](info::market_entry_t const * e) { return e->mean_price - e->buy_price; }
  );

  size_t on_sale{};
  size_t wanted{};
  for(info::market_entry_t const & entry: *entries)
    {
    on_sale += entry.stock > 0u ? 1u : 0u;
    wanted += entry.demand > 0u ? 1u : 0u;
    }

  // port bywa dostawca albo odbiorca - te dwie liczby mowia to od pierwszego spojrzenia
  market_lines_.push_back(
    overlay::line_t{.text = std::format("{}: {} on sale, {} wanted", name, on_sale, wanted), .color = colour_heading}
  );

  // surowca, ktorego nikt nie sprzedaje, handlarz nie przywiezie - stacja moze za niego placic
  // swietnie i to nadal bedzie slepy zaulek, wiec idzie na koniec i pod wlasnym naglowkiem
  auto const mined{[](info::market_entry_t const * entry) { return info::is_mining_only(entry->name); }};
  auto const tradeable{std::ranges::partition(sells, std::not_fn(mined))};
  std::vector<info::market_entry_t const *> const dug_up(tradeable.begin(), tradeable.end());
  sells.erase(tradeable.begin(), tradeable.end());

  if(not sells.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "pays above average:", .color = colour_plain});
    for(info::market_entry_t const * entry: sells | std::views::take(listed_commodities))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} over avg",
            entry->name,
            format_credits_value(entry->sell_price),
            format_credits_value(entry->sell_price - entry->mean_price)
          ),
          .color = colour_first
        }
      );
    }

  if(not dug_up.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "pays well, but mining only:", .color = colour_plain});
    for(info::market_entry_t const * entry: dug_up | std::views::take(2u))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} over avg",
            entry->name,
            format_credits_value(entry->sell_price),
            format_credits_value(entry->sell_price - entry->mean_price)
          ),
          .color = colour_plain
        }
      );
    }

  if(not buys.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "sells below average:", .color = colour_plain});
    for(info::market_entry_t const * entry: buys | std::views::take(listed_commodities))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} under avg",
            entry->name,
            format_credits_value(entry->buy_price),
            format_credits_value(entry->mean_price - entry->buy_price)
          ),
          .color = colour_plain
        }
      );
    }

  // srednia galaktyczna mowi czy cena jest dobra, ale zarobek bierze sie z roznicy miedzy rynkami
  auto const add_trades{
    [this, cargo_capacity](bool bring_here, char const * heading)
    {
      auto trades{db_.load_trade_options(market_id_, listed_trades, bring_here)};
      if(not trades or trades->empty())
        return;

      market_lines_.push_back(overlay::line_t{.text = heading, .color = colour_plain});

      for(info::trade_option_t const & trade: *trades)
        {
        auto const margin{trade.sell_price - trade.buy_price};
        // jeden kurs to tyle ton ile zmiesci ladownia, o ile starczy towaru i popytu
        auto const tonnes{std::min({cargo_capacity != 0u ? cargo_capacity : trade.stock, trade.stock, trade.demand})};
        auto const run{uint64_t{margin} * tonnes};

        market_lines_.push_back(
          overlay::line_t{
            .text = std::format("  {}  {} Cr/t", trade.commodity, format_credits_value(margin)), .color = colour_first
          }
        );
        market_lines_.push_back(
          overlay::line_t{
            .text = std::format(
              "    {} t = {} Cr  {} {}{}{}",
              format_credits_value(tonnes),
              format_credits_value(static_cast<uint32_t>(std::min<uint64_t>(run, UINT32_MAX))),
              bring_here ? "from" : "to",
              trade.station,
              trade.system.empty() ? "" : ", ",
              trade.system
            ),
            .color = colour_plain
          }
        );
        }
    }
  };

  add_trades(true, "bring here, best known:");
  add_trades(false, "take from here, best known:");
  }

auto overlay_feed_t::refresh_supply() -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(now - supply_loaded_ < supply_refresh)
    return;

  supply_loaded_ = now;
  needs_.clear();
  options_.clear();
  producers_.clear();

  if(auto needs{db_.load_cargo_needs()}; needs)
    needs_ = std::move(*needs);

  if(needs_.empty())
    return;

  if(auto options{db_.load_supply_options()}; options)
    options_ = std::move(*options);

  if(auto producers{db_.load_producers()}; producers)
    producers_ = std::move(*producers);
  }

auto overlay_feed_t::build_supply_lines(events::cargo_file_t const & cargo) const -> std::vector<overlay::line_t>
  {
  if(needs_.empty())
    return {};

  // nazwy w ladowni sa wewnetrzne, w misjach czytelne - porownujemy po samych literach i cyfrach
  auto const key{[](std::string_view text)
                 {
                   std::string out;
                   for(char const c: text)
                     if(std::isalnum(static_cast<unsigned char>(c)))
                       out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                   return out;
                 }};

  std::map<std::string, uint32_t> aboard;
  for(events::cargo_item_t const & item: cargo.Inventory)
    {
    auto const internal{key(item.Name)};
    aboard[internal] += item.Count;

    // obie nazwy potrafia sprowadzic sie do tego samego klucza i wtedy ilosc liczylaby sie dwa razy
    if(auto const localised{key(item.Name_Localised)}; not localised.empty() and localised != internal)
      aboard[localised] += item.Count;
    }

  auto const held{
    [&](std::string const & commodity) -> uint32_t
    {
      auto const it{aboard.find(key(commodity))};
      return it != aboard.end() ? it->second : 0u;
    }
  };

  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = "mission cargo:", .color = colour_heading});

  std::set<std::string> missing;
  for(info::cargo_need_t const & need: needs_)
    {
    auto const have{held(need.commodity)};
    bool const known{std::ranges::any_of(
      options_, [&need](info::supply_option_t const & option) { return option.commodity == need.commodity; }
    )};

    // rynek moze tym handlowac i byc akurat pusty - to zupelnie inna wiadomosc niz brak zrodla
    auto const seller{std::ranges::find_if(
      producers_, [&need](info::supply_option_t const & option) { return option.commodity == need.commodity; }
    )};
    bool const sold_somewhere{seller != producers_.end()};

    if(have < need.count)
      missing.insert(need.commodity);

    // liczba w nawiasie to stan ladowni - dzieki niej brakujaca pozycja rzuca sie w oczy
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "  {} x{} ({}){}",
          need.commodity,
          need.count,
          have,
          have >= need.count or known ? std::string{}
          : sold_somewhere            ? std::format("   {} sells it, out of stock", seller->station)
                                      : std::string{"   no source known"}
        ),
        .color = have >= need.count ? colour_first : (known ? colour_plain : colour_alert)
      }
    );
    }

  if(missing.empty())
    {
    lines.push_back(overlay::line_t{.text = "all aboard", .color = colour_first});
    return lines;
    }

  // jedno miejsce na kilka brakujacych towarow oszczedza caly kurs, dopiero potem liczy sie rodzaj portu
  struct place_t
    {
    std::string station;
    std::string system;
    int rank{};
    std::vector<info::supply_option_t const *> items;
    };

  std::map<uint64_t, place_t> places;
  for(info::supply_option_t const & option: options_)
    {
    if(not missing.contains(option.commodity))
      continue;

    place_t & place{places[option.market_id]};
    if(place.items.empty())
      {
      place.station = option.station.empty() ? std::format("market {}", option.market_id) : option.station;
      place.system = option.system;
      place.rank = station_rank(option.station_type);
      }
    place.items.push_back(&option);
    }

  std::vector<place_t const *> ranked;
  ranked.reserve(places.size());
  for(auto const & [market_id, place]: places)
    ranked.push_back(&place);

  std::ranges::sort(
    ranked,
    [](place_t const * left, place_t const * right)
    {
      if(left->items.size() != right->items.size())
        return left->items.size() > right->items.size();
      return left->rank < right->rank;
    }
  );

  for(place_t const * place: ranked | std::views::take(listed_sources))
    {
    lines.push_back(
      overlay::line_t{
        .text = std::format("{}{}{}", place->station, place->system.empty() ? "" : "  ", place->system),
        .color = colour_first
      }
    );

    for(info::supply_option_t const * item: place->items | std::views::take(listed_commodities))
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} in stock  {} Cr",
            item->commodity,
            format_credits_value(item->stock),
            format_credits_value(item->buy_price)
          ),
          .color = colour_plain
        }
      );
    }

  return lines;
  }

auto overlay_feed_t::publish(current_state_t const & state) -> void
  {
  if(not server_->listening())
    return;

  refresh_factions(state);
  refresh_market(state.settlement_market_id_, state.ship_loadout.CargoCapacity);
  refresh_supply();

  overlay::frame_t frame{};

  if(not state.system.name.empty())
    {
    auto lines{describe_system(state.system, faction_lines_.empty())};
    lines.insert(lines.end(), faction_lines_.begin(), faction_lines_.end());
    lines.insert(lines.end(), conflict_lines_.begin(), conflict_lines_.end());
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_left, .ttl_ms = block_ttl_ms, .lines = std::move(lines)}
    );
    }

  if(auto exploration{describe_exploration(state.system)}; not exploration.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_right, .ttl_ms = block_ttl_ms, .lines = std::move(exploration)
      }
    );

  if(auto supply{build_supply_lines(state.cargo)}; not supply.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms, .lines = std::move(supply)}
    );

  // rynek pod spodem, bo jest dluzszy i mniej pilny niz to, czego brakuje do misji
  if(not market_lines_.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms, .lines = market_lines_}
    );

  if(auto cargo{describe_cargo(state.cargo)}; not cargo.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms, .lines = std::move(cargo)}
    );

  if(auto missions{describe_missions(state.active_missions)}; not missions.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms, .lines = std::move(missions)}
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
