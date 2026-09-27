#include <ground_cz.h>

#include <glaze/glaze.hpp>

#include <algorithm>
#include <fstream>
#include <ranges>
#include <set>
#include <vector>

auto ground_cz_tracker_t::approach(events::approach_settlement_t const & event) -> void
  {
  system_address_ = event.SystemAddress;
  market_id_ = event.MarketID;
  }

auto ground_cz_tracker_t::docked(events::docked_t const & event) -> void
  {
  system_address_ = event.SystemAddress;
  market_id_ = event.MarketID;
  on_foot_ = false;
  }

auto ground_cz_tracker_t::disembark(events::disembark_t const & event) -> void
  {
  on_foot_ = true;
  system_address_ = event.SystemAddress;
  // a disembark at a place names it; one in the open keeps the settlement the ship was brought to
  if(event.MarketID != 0u)
    market_id_ = event.MarketID;
  }

auto ground_cz_tracker_t::book_dropship(events::book_dropship_t const & event) -> void
  {
  // the way back leads out of the zone; the way there names the next one
  if(event.Retreat)
    {
    booked_.clear();
    return;
    }
  booked_ = event.DestinationLocation;
  }

auto ground_cz_tracker_t::dropship_deploy(database_storage_t & db, events::dropship_deploy_t const & event) -> void
  {
  on_foot_ = true;
  system_address_ = event.SystemAddress;
  market_id_ = 0u;
  if(not booked_.empty())
    if(auto found{db.load_station(event.SystemAddress, booked_)}; found and *found)
      market_id_ = (*found)->market_id;
  booked_.clear();
  }

auto ground_cz_tracker_t::location(events::location_t const & event) -> void
  {
  // after a relog: another system is another place, while on foot in the same one the settlement stays
  if(event.SystemAddress != system_address_)
    market_id_ = 0u;
  system_address_ = event.SystemAddress;
  on_foot_ = event.OnFoot;
  if(event.MarketID != 0u)
    market_id_ = event.MarketID;
  }

auto ground_cz_tracker_t::jumped(uint64_t system_address) -> void
  {
  system_address_ = system_address;
  market_id_ = 0u;
  on_foot_ = false;
  booked_.clear();
  }

auto ground_cz_tracker_t::embark() -> void { on_foot_ = false; }

auto ground_cz_tracker_t::died() -> void { on_foot_ = false; }

auto ground_cz_tracker_t::bond(std::chrono::sys_seconds when, events::faction_kill_bond_t const & event) const
  -> std::optional<info::ground_bond_t>
  {
  if(not on_foot_)
    return std::nullopt;
  return info::ground_bond_t{
    .timestamp = when,
    .system_address = system_address_,
    .market_id = market_id_,
    .awarding_faction = event.AwardingFaction,
    .victim_faction = event.VictimFaction,
    .reward = event.Reward,
    .intensity = static_cast<uint8_t>(info::cz_intensity_of(event.Reward))
  };
  }

// named, not anonymous: glaze reflects these, and it needs types with linkage
namespace ground_cz_detail
  {
struct commander_line_t
  { std::string FID; };

struct redeem_bond_line_t
  {
  std::string Type;
  std::string Faction;
  };
  }  // namespace ground_cz_detail

auto unsold_bonds(std::filesystem::path const & journal_dir, std::string_view commander_fid)
  -> std::map<std::string, uint64_t>
  {
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  std::ranges::sort(journals, std::ranges::greater{});

  constexpr auto opts{glz::opts{.error_on_unknown_keys = false}};
  std::map<std::string, uint64_t> result;
  // the factions whose bonds were redeemed later than the line being read
  std::set<std::string> redeemed;

  // bonds are sold after a war or two; the bound keeps a commander who never sold any from reading the
  // whole archive at every refresh
  for(std::filesystem::path const & path: journals | std::views::take(300))
    {
    std::ifstream in{path, std::ios::binary};
    std::vector<std::string> lines;
    bool ours{commander_fid.empty()};
    for(std::string line; std::getline(in, line);)
      {
      if(line.contains("\"event\":\"Commander\""))
        {
        ground_cz_detail::commander_line_t commander{};
        if(not glz::read<opts>(commander, line))
          ours = commander_fid.empty() or commander.FID == commander_fid;
        continue;
        }
      if(ours and (line.contains("\"event\":\"FactionKillBond\"") or line.contains("\"CombatBond\"")))
        lines.push_back(std::move(line));
      }

    for(std::string const & line: lines | std::views::reverse)
      {
      if(line.contains("\"event\":\"RedeemVoucher\""))
        {
        ground_cz_detail::redeem_bond_line_t redeem{};
        if(not glz::read<opts>(redeem, line) and redeem.Type == "CombatBond")
          redeemed.insert(redeem.Faction);
        continue;
        }
      events::faction_kill_bond_t bond{};
      if(glz::read<opts>(bond, line) or redeemed.contains(bond.AwardingFaction))
        continue;
      result[bond.AwardingFaction] += bond.Reward;
      }
    }
  return result;
  }
