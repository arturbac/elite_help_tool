#include <buy_search.h>

#include <json_glaze.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <ranges>

namespace buy_search
  {
namespace detail
  {
  struct docked_line_t
    {
    uint64_t MarketID{};
    std::optional<events::landing_pads_t> LandingPads;
    };

  [[nodiscard]]
  auto distance(std::array<double, 3> const & a, std::array<double, 3> const & b) -> double
    { return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]); }
  }  // namespace detail

auto place_of(std::string_view station_type) -> place_e
  {
  using enum place_e;
  static constexpr std::array<std::pair<std::string_view, place_e>, 11> kinds{
    {{"Coriolis", space_station},
     {"Orbis", space_station},
     {"Ocellus", space_station},
     {"Dodec", space_station},
     {"Bernal", space_station},
     {"AsteroidBase", space_station},
     {"Outpost", outpost},
     {"CraterPort", surface_port},
     {"CraterOutpost", surface_port},
     {"SurfaceStation", surface_port},
     {"OnFootSettlement", settlement}}
  };
  if(station_type == "FleetCarrier")
    return carrier;
  auto const it{std::ranges::find(kinds, station_type, &std::pair<std::string_view, place_e>::first)};
  return it != kinds.end() ? it->second : other;
  }

auto largest_pad(std::optional<uint16_t> small, std::optional<uint16_t> medium, std::optional<uint16_t> large)
  -> std::optional<pad_e>
  {
  if(not small or not medium or not large)
    return std::nullopt;
  if(*large != 0u)
    return pad_e::large;
  if(*medium != 0u)
    return pad_e::medium;
  if(*small != 0u)
    return pad_e::small;
  return pad_e::none;
  }

auto places_of(std::vector<info::buy_offer_t> const & rows, std::optional<std::array<double, 3>> const & here)
  -> std::vector<place_t>
  {
  std::vector<place_t> places;
  std::map<uint64_t, std::size_t> index;
  for(info::buy_offer_t const & row: rows)
    {
    auto [it, fresh]{index.try_emplace(row.market_id, places.size())};
    if(fresh)
      {
      place_t place{
        .market_id = row.market_id,
        .system = row.system,
        .station = row.station,
        .station_type = row.station_type,
        .pad = largest_pad(row.pads_small, row.pads_medium, row.pads_large),
        .dist_ls = row.dist_from_star_ls,
        .position = {},
        .ly = {},
        .updated = row.updated,
        .offers = {},
        .source = row.source
      };
      if(row.loc_x and row.loc_y and row.loc_z)
        place.position = std::array{*row.loc_x, *row.loc_y, *row.loc_z};
      if(place.position and here)
        place.ly = detail::distance(*place.position, *here);
      places.push_back(std::move(place));
      }
    places[it->second].offers.push_back(
      offer_t{.commodity = row.commodity, .buy_price = row.buy_price, .stock = row.stock, .mean_price = row.mean_price}
    );
    }
  return places;
  }

auto covers(offer_t const & offer, wanted_t const & wanted) -> bool
  {
  // the dictionary and the missions write the readable name, but not always in the same case
  bool const same{std::ranges::equal(
    offer.commodity, wanted.commodity, [](char a, char b) { return std::tolower(uint8_t(a)) == std::tolower(uint8_t(b)); }
  )};
  return same and offer.buy_price > 0 and offer.stock > 0
         and offer.stock >= int64_t(wanted.tonnes);
  }

auto hits(place_t const & place, std::vector<wanted_t> const & wanted) -> unsigned
  {
  return unsigned(std::ranges::count_if(
    wanted,
    [&](wanted_t const & w) { return std::ranges::any_of(place.offers, [&](offer_t const & o) { return covers(o, w); }); }
  ));
  }

auto cost(place_t const & place, std::vector<wanted_t> const & wanted) -> std::optional<int64_t>
  {
  int64_t sum{};
  for(wanted_t const & w: wanted)
    {
    if(w.tonnes == 0u)
      return std::nullopt;
    auto const it{std::ranges::find_if(place.offers, [&](offer_t const & o) { return covers(o, w); })};
    if(it == place.offers.end())
      return std::nullopt;
    sum += it->buy_price * int64_t(w.tonnes);
    }
  return wanted.empty() ? std::nullopt : std::optional<int64_t>{sum};
  }

auto passes(place_t const & place, filter_t const & filter, std::chrono::sys_seconds now) -> bool
  {
  place_e const kind{place_of(place.station_type)};
  if(kind == place_e::other or not filter.places.contains(kind))
    return false;
  if(now - place.updated > filter.max_age)
    return false;
  if(place.source == "eddn" and not filter.eddn)
    return false;
  if(filter.max_ly and (not place.ly or *place.ly > *filter.max_ly))
    return false;
  if(not place.pad)
    return filter.unknown_pads;
  return *place.pad >= filter.min_pad;
  }

auto rank(
  std::vector<place_t> places, std::vector<wanted_t> const & wanted, filter_t const & filter, std::chrono::sys_seconds now
) -> std::vector<place_t>
  {
  std::erase_if(places, [&](place_t const & p) { return not passes(p, filter, now) or hits(p, wanted) == 0u; });
  // a place of unknown distance goes after every known one with as many hits
  auto const key = [&](place_t const & p)
  { return std::tuple{-int(hits(p, wanted)), p.ly.has_value() ? 0 : 1, p.ly.value_or(0.0)}; };
  std::ranges::stable_sort(places, std::less{}, key);
  return places;
  }

auto covering_pairs(std::vector<place_t> const & ranked, std::vector<wanted_t> const & wanted, std::size_t most)
  -> std::vector<pair_t>
  {
  std::vector<pair_t> pairs;
  if(wanted.size() < 2u or ranked.empty() or hits(ranked.front(), wanted) == wanted.size())
    return pairs;

  std::vector<std::vector<bool>> has(ranked.size());
  for(std::size_t i{}; i != ranked.size(); ++i)
    for(wanted_t const & w: wanted)
      has[i].push_back(std::ranges::any_of(ranked[i].offers, [&](offer_t const & o) { return covers(o, w); }));

  for(std::size_t i{}; i != ranked.size(); ++i)
    for(std::size_t j{i + 1u}; j != ranked.size(); ++j)
      {
      place_t const & a{ranked[i]};
      place_t const & b{ranked[j]};
      if(not a.ly or not b.ly or not a.position or not b.position)
        continue;
      bool all{true};
      for(std::size_t k{}; k != wanted.size() and all; ++k)
        all = has[i][k] or has[j][k];
      if(not all)
        continue;
      bool const a_nearer{*a.ly <= *b.ly};
      pairs.push_back(
        pair_t{
          .first = a_nearer ? i : j,
          .second = a_nearer ? j : i,
          .ly = std::min(*a.ly, *b.ly) + detail::distance(*a.position, *b.position)
        }
      );
      }
  std::ranges::sort(pairs, std::less{}, &pair_t::ly);
  if(pairs.size() > most)
    pairs.resize(most);
  return pairs;
  }

auto jumps(double ly, float range) -> std::optional<unsigned>
  {
  if(range <= 0.f or ly < 0.0)
    return std::nullopt;
  return unsigned(std::ceil(ly / double(range)));
  }

auto market_state(info::market_to_update_t const & place) -> market_state_e
  {
  if(not place.updated)
    return market_state_e::never_read;
  // the owner at the reading, when it was kept, says it at once
  if(not place.read_owner.empty() and not place.owner.empty() and place.read_owner != place.owner)
    return market_state_e::owner_changed;
  // otherwise the history of owners: a takeover after the reading. With one owner only the first sighting could
  // fall after a reading taken before the owners were kept, and that is no change
  if(place.owners > 1u and place.owner_since and *place.owner_since > *place.updated)
    return market_state_e::owner_changed;
  return market_state_e::read;
  }

auto worth_reading(info::market_to_update_t const & place) -> bool
  {
  place_e const kind{place_of(place.station_type)};
  return kind != place_e::other and kind != place_e::carrier and not place.station.empty()
         and not place.station.starts_with('$') and not place.station.contains("Construction Site");
  }

auto pads_in_journal(std::istream & journal) -> std::map<uint64_t, events::landing_pads_t>
  {
  constexpr auto opts{glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = false}};
  std::map<uint64_t, events::landing_pads_t> pads;
  for(std::string line; std::getline(journal, line);)
    if(line.contains("\"LandingPads\"") and line.contains("\"event\":\"Docked\""))
      {
      detail::docked_line_t docked{};
      if(not glz::read<opts>(docked, line) and docked.MarketID != 0u and docked.LandingPads)
        pads[docked.MarketID] = *docked.LandingPads;
      }
  return pads;
  }

auto pads_from_journals(std::filesystem::path const & journal_dir, std::set<uint64_t> wanted, std::string_view after)
  -> journal_pads_t
  {
  journal_pads_t result;
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()};
       name.starts_with("Journal.") and name.ends_with(".log") and name > after)
      journals.push_back(entry.path());
  // the names carry the date, so the newest reading of a place comes first
  std::ranges::sort(journals, std::ranges::greater{});
  if(not journals.empty())
    result.newest_journal = journals.front().filename().string();
  for(std::filesystem::path const & path: journals)
    {
    if(wanted.empty())
      break;
    std::ifstream in{path, std::ios::binary};
    for(auto const & [market_id, pads]: pads_in_journal(in))
      if(wanted.erase(market_id) != 0u)
        result.found.emplace(market_id, pads);
    }
  return result;
  }
  }  // namespace buy_search
