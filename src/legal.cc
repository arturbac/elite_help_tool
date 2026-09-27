#include <legal.h>

#include <glaze/glaze.hpp>

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
  }  // namespace legal_detail

auto bounty_holders(
  std::filesystem::path const & journal_dir, std::string_view commander_fid, std::chrono::sys_seconds since
) -> std::vector<bounty_holder_t>
  {
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  std::ranges::sort(journals, std::ranges::greater{});

  constexpr auto opts{glz::opts{.error_on_unknown_keys = false}};
  std::map<std::string, bounty_holder_t> holders;
  // the factions whose bounties were paid later than the line being read
  std::set<std::string> paid;
  bool all_paid{};

  for(std::filesystem::path const & path: journals | std::views::take(100))
    {
    std::ifstream in{path, std::ios::binary};
    std::vector<std::string> lines;
    bool ours{commander_fid.empty()};
    bool recent{};
    for(std::string line; std::getline(in, line);)
      {
      if(line.contains("\"event\":\"Commander\""))
        {
        legal_detail::commander_line_t commander{};
        if(not glz::read<opts>(commander, line))
          ours = commander_fid.empty() or commander.FID == commander_fid;
        continue;
        }
      if(ours and (line.contains("\"event\":\"CommitCrime\"") or line.contains("\"event\":\"PayBounties\"")))
        lines.push_back(std::move(line));
      }

    for(std::string const & line: lines | std::views::reverse)
      {
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
      if(crime.timestamp < since)
        continue;
      recent = true;
      if(all_paid or paid.contains(crime.Faction))
        continue;
      auto & holder{holders[crime.Faction]};
      holder.faction = crime.Faction;
      holder.last_crime = std::max(holder.last_crime, crime.timestamp);
      }
    // the journals go newest first; one with nothing inside the window and older than it ends the reading
    if(not recent and not lines.empty())
      {
      legal_detail::crime_line_t first{};
      if(not glz::read<opts>(first, lines.front()) and first.timestamp < since)
        break;
      }
    }

  std::vector<bounty_holder_t> result;
  for(auto & [faction, holder]: holders)
    result.push_back(std::move(holder));
  std::ranges::sort(result, std::ranges::greater{}, &bounty_holder_t::last_crime);
  return result;
  }
