#include <legal.h>

#include <json_glaze.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <ranges>
#include <set>

// named, not anonymous: glaze reflects these, and it needs types with linkage
namespace legal_detail
  {
struct commander_line_t
  { std::string FID; };

struct crime_line_t
  {
  std::chrono::sys_seconds timestamp;
  std::string Faction;
  uint64_t Bounty{};
  };

struct payment_line_t
  {
  std::string Faction;
  bool AllFines{};
  };

struct crime_stats_t
  { uint32_t Notoriety{}; };

struct statistics_line_t
  {
  std::chrono::sys_seconds timestamp;
  crime_stats_t Crime;
  };
  }  // namespace legal_detail

auto legal_standing(
  std::filesystem::path const & journal_dir,
  std::string_view commander_fid,
  std::chrono::sys_seconds since,
  std::chrono::sys_seconds now
) -> legal_standing_t
  {
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  std::ranges::sort(journals, std::ranges::greater{});

  constexpr auto opts{glz::opts{.error_on_unknown_keys = false}};
  legal_standing_t standing;
  bool notoriety_known{};
  std::map<std::string, bounty_holder_t> holders;
  // the factions whose bounties were paid later than the line being read
  std::set<std::string> paid;
  bool all_paid{};

  // a bounty grows over weeks of small crimes - the reading goes back to the payment, or this far
  for(std::filesystem::path const & path: journals | std::views::take(300))
    {
    std::ifstream in{path, std::ios::binary};
    std::vector<std::string> lines;
    bool ours{commander_fid.empty()};
    for(std::string line; std::getline(in, line);)
      {
      if(line.contains("\"event\":\"Commander\""))
        {
        legal_detail::commander_line_t commander{};
        if(not glz::read<opts>(commander, line))
          ours = commander_fid.empty() or commander.FID == commander_fid;
        continue;
        }
      if(
        ours
        and (line.contains("\"event\":\"CommitCrime\"") or line.contains("\"event\":\"PayBounties\"") or line.contains("\"event\":\"Statistics\""))
      )
        lines.push_back(std::move(line));
      }

    for(std::string const & line: lines | std::views::reverse)
      {
      if(line.contains("\"event\":\"Statistics\""))
        {
        // the newest login's count is the one that matters
        legal_detail::statistics_line_t s{};
        if(not notoriety_known and not glz::read<opts>(s, line))
          {
          notoriety_known = true;
          standing.notoriety = s.Crime.Notoriety;
          standing.notoriety_at = s.timestamp;
          }
        continue;
        }
      if(line.contains("\"event\":\"PayBounties\""))
        {
        legal_detail::payment_line_t payment{};
        if(glz::read<opts>(payment, line))
          continue;
        if(payment.AllFines or payment.Faction.empty())
          all_paid = true;
        else
          paid.insert(payment.Faction);
        continue;
        }
      legal_detail::crime_line_t crime{};
      if(glz::read<opts>(crime, line) or crime.Bounty == 0u or crime.Faction.empty())
        continue;
      if(all_paid or paid.contains(crime.Faction))
        continue;
      auto & holder{holders[crime.Faction]};
      holder.faction = crime.Faction;
      holder.last_crime = std::max(holder.last_crime, crime.timestamp);
      holder.crimes_bounty += crime.Bounty;
      }
    }

  // one step of the squadron's decay takes 100k within two hours - a few murders are gone by then
  constexpr uint64_t decay_step{100'000u};
  constexpr std::chrono::hours decay_every{2};
  for(auto & [faction, holder]: holders)
    if(holder.last_crime >= since and (holder.crimes_bounty >= decay_step or now - holder.last_crime < decay_every))
      standing.holders.push_back(std::move(holder));
  std::ranges::sort(standing.holders, std::ranges::greater{}, &bounty_holder_t::last_crime);
  return standing;
  }
