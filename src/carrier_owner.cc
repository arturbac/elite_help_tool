#include <carrier_owner.h>

#include <json_glaze.h>

#include <algorithm>
#include <fstream>
#include <ranges>
#include <vector>

namespace carrier_owner
  {
namespace detail
  {
  struct commander_line_t
    {
    std::string FID;
    };

  struct stats_line_t
    {
    uint64_t CarrierID;
    std::string CarrierType;
    };
  }  // namespace detail

auto owners_in_journal(std::istream & journal) -> owners_t
  {
  constexpr auto opts{glz::opts{.error_on_unknown_keys = false}};
  owners_t owners;
  std::string fid;
  for(std::string line; std::getline(journal, line);)
    if(line.contains("\"event\":\"Commander\""))
      {
      detail::commander_line_t commander{};
      fid = glz::read<opts>(commander, line) ? std::string{} : std::move(commander.FID);
      }
    else if(line.contains("\"event\":\"CarrierStats\""))
      {
      detail::stats_line_t stats{};
      if(not fid.empty() and not glz::read<opts>(stats, line) and stats.CarrierType != "SquadronCarrier")
        owners[stats.CarrierID] = fid;
      }
  return owners;
  }

auto owners_from_journals(std::filesystem::path const & journal_dir, std::set<uint64_t> wanted) -> owners_t
  {
  owners_t found;
  if(wanted.empty())
    return found;
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  // the names carry the date, so the order of the names is the order of the sessions
  std::ranges::sort(journals, std::ranges::greater{});
  for(std::filesystem::path const & path: journals)
    {
    std::ifstream in{path, std::ios::binary};
    for(auto & [carrier, fid]: owners_in_journal(in))
      if(wanted.erase(carrier) != 0u)
        found.emplace(carrier, std::move(fid));
    if(wanted.empty())
      break;
    }
  return found;
  }
  }  // namespace carrier_owner
