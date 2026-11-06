#include <system_index.h>

#include <picture_records.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <format>
#include <fstream>
#include <ranges>
#include <sstream>
#include <system_error>
#include <utility>

namespace system_index
  {
namespace
  {
constexpr std::string_view regions_file{"regions.tsv"};
constexpr std::string_view rows_file{"systems.tsv"};

constexpr std::array<std::string_view, 10> letter_order{"S", "G", "E", "W", "A", "M", "H", "R", "I", "Ri"};

///\brief QT-O in a generated name
[[nodiscard]]
auto is_grid_code(std::string_view word) noexcept -> bool
  {
  auto const upper = [](char c) noexcept { return c >= 'A' and c <= 'Z'; };
  return word.size() == 4u and upper(word[0]) and upper(word[1]) and word[2] == '-' and upper(word[3]);
  }

///\brief d7-43 or d7 in a generated name
[[nodiscard]]
auto is_mass_code(std::string_view word) noexcept -> bool
  {
  if(word.size() < 2u or word[0] < 'a' or word[0] > 'h')
    return false;
  return std::ranges::all_of(
    word.substr(1), [](char c) noexcept { return (c >= '0' and c <= '9') or c == '-'; }
  );
  }

///\brief the value of a key in a journal's line of JSON, whose strings here hold no quotes
[[nodiscard]]
auto value_of(std::string_view line, std::string_view key) -> std::string_view
  {
  std::string const needle{std::format("\"{}\":", key)};
  size_t at{line.find(needle)};
  if(at == std::string_view::npos)
    return {};
  at = line.find('"', at + needle.size());
  if(at == std::string_view::npos)
    return {};
  size_t const end{line.find('"', at + 1u)};
  return end == std::string_view::npos ? std::string_view{} : line.substr(at + 1u, end - at - 1u);
  }

[[nodiscard]]
auto split_tabs(std::string_view line) -> std::vector<std::string_view>
  {
  std::vector<std::string_view> parts;
  for(auto const part: line | std::views::split('\t'))
    parts.emplace_back(part.begin(), part.end());
  return parts;
  }

[[nodiscard]]
auto read_text(std::filesystem::path const & path) -> std::string
  {
  std::ifstream in{path, std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  return std::move(text).str();
  }
  }  // namespace

auto page_name(std::string_view system) -> std::string
  { return std::format("systems/{}.html", pictures::file_safe(system)); }

auto body_letter(std::string_view planet_class) -> std::string_view
  {
  if(planet_class.empty())
    return {};
  if(planet_class.contains("giant"))
    return "G";
  if(planet_class == "Earthlike body")
    return "E";
  if(planet_class == "Water world")
    return "W";
  if(planet_class == "Ammonia world")
    return "A";
  if(planet_class == "Metal rich body")
    return "M";
  if(planet_class == "High metal content body")
    return "H";
  if(planet_class == "Rocky ice body")
    return "Ri";
  if(planet_class == "Rocky body")
    return "R";
  if(planet_class == "Icy body")
    return "I";
  return {};
  }

auto counts_text(body_counts_t const & counts) -> std::string
  {
  std::string text;
  for(std::string_view const letter: letter_order)
    if(auto const it{counts.find(std::string{letter})}; it != counts.end() and it->second != 0u)
      text += std::format("{}{}:{}", text.empty() ? "" : ", ", letter, it->second);
  return text.empty() ? text : "[" + text + "]";
  }

auto sector_of(std::string_view system) -> std::string
  {
  size_t const last{system.rfind(' ')};
  if(last == std::string_view::npos or last == 0u)
    return std::string{named_systems};
  size_t const before{system.rfind(' ', last - 1u)};
  if(before == std::string_view::npos or before == 0u)
    return std::string{named_systems};
  if(not is_mass_code(system.substr(last + 1u)) or not is_grid_code(system.substr(before + 1u, last - before - 1u)))
    return std::string{named_systems};
  return std::string{system.substr(0u, before)};
  }

auto region_in_line(std::string_view line) -> std::optional<region_seen_t>
  {
  if(not line.contains("\"CodexEntry\""))
    return std::nullopt;
  std::string_view const system{value_of(line, "System")};
  std::string_view const region{value_of(line, "Region_Localised")};
  if(system.empty() or region.empty())
    return std::nullopt;
  return region_seen_t{.system = std::string{system}, .region = std::string{region}};
  }

auto regions_in_journals(std::filesystem::path const & journal_dir) -> region_counts_t
  {
  region_counts_t regions;
  std::error_code ec;
  for(std::filesystem::directory_entry const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    {
    std::string const name{entry.path().filename().string()};
    if(not name.starts_with("Journal.") or not name.ends_with(".log"))
      continue;
    std::ifstream in{entry.path()};
    for(std::string line; std::getline(in, line);)
      if(auto seen{region_in_line(line)}; seen)
        ++regions[sector_of(seen->system)][seen->region];
    }
  if(ec)
    spdlog::warn("album: the journals in {} could not be read: {}", journal_dir.string(), ec.message());
  // a name of its own says nothing of where the next one is
  regions.erase(std::string{named_systems});
  return regions;
  }

auto read_regions(std::filesystem::path const & codex_dir) -> region_counts_t
  {
  region_counts_t regions;
  std::ifstream in{codex_dir / regions_file};
  for(std::string line; std::getline(in, line);)
    if(auto const parts{split_tabs(line)}; parts.size() == 3u)
      {
      uint32_t count{};
      std::from_chars(parts[2].data(), parts[2].data() + parts[2].size(), count);
      regions[std::string{parts[0]}][std::string{parts[1]}] = count;
      }
  return regions;
  }

auto write_regions(std::filesystem::path const & codex_dir, region_counts_t const & regions) -> void
  {
  std::string text;
  for(auto const & [sector, named]: regions)
    for(auto const & [region, count]: named)
      text += std::format("{}\t{}\t{}\n", sector, region, count);
  write_if_changed(codex_dir / regions_file, text);
  }

auto merge_regions(region_counts_t & into, region_counts_t const & from) -> void
  {
  for(auto const & [sector, named]: from)
    for(auto const & [region, count]: named)
      into[sector][region] += count;
  }

auto region_of(region_counts_t const & regions, std::string const & sector) -> std::string
  {
  auto const it{regions.find(sector)};
  if(it == regions.end() or it->second.empty())
    return std::string{unknown_region};
  return std::ranges::max_element(it->second, {}, [](auto const & named) { return named.second; })->first;
  }

auto read_rows(std::filesystem::path const & codex_dir) -> std::vector<row_t>
  {
  std::vector<row_t> rows;
  std::ifstream in{codex_dir / rows_file};
  for(std::string line; std::getline(in, line);)
    if(auto const parts{split_tabs(line)}; parts.size() >= 5u)
      {
      row_t row{.name = std::string{parts[1]}, .counts = std::string{parts[2]}};
      std::from_chars(parts[0].data(), parts[0].data() + parts[0].size(), row.address);
      row.picture = parts[3] == "1";
      row.life = parts[4] == "1";
      row.first = parts.size() > 5u and parts[5] == "1";
      row.href = page_name(row.name);
      rows.push_back(std::move(row));
      }
  return rows;
  }

auto write_rows(std::filesystem::path const & codex_dir, std::span<row_t const> rows) -> void
  {
  std::string text;
  for(row_t const & row: rows)
    text += std::format(
      "{}\t{}\t{}\t{}\t{}\t{}\n", row.address, row.name, row.counts, row.picture ? 1 : 0, row.life ? 1 : 0, row.first ? 1 : 0
    );
  write_if_changed(codex_dir / rows_file, text);
  }

auto index_page(std::span<row_t const> rows, region_counts_t const & regions) -> std::string
  {
  using sectors_t = std::map<std::string, std::vector<row_t const *>>;
  std::map<std::string, sectors_t> grouped;
  size_t pictured{};
  size_t alive{};
  size_t firsts{};
  for(row_t const & row: rows)
    {
    std::string sector{sector_of(row.name)};
    std::string region{region_of(regions, sector)};
    grouped[std::move(region)][std::move(sector)].push_back(&row);
    pictured += row.picture ? 1u : 0u;
    alive += row.life ? 1u : 0u;
    firsts += row.first ? 1u : 0u;
    }

  auto const size_of = [](sectors_t const & sectors)
  {
    size_t total{};
    for(auto const & [sector, systems]: sectors)
      total += systems.size();
    return total;
  };
  std::vector<std::pair<std::string, sectors_t const *>> ordered;
  for(auto const & [region, sectors]: grouped)
    ordered.emplace_back(region, &sectors);
  std::ranges::stable_sort(
    ordered,
    [&](auto const & a, auto const & b)
    {
      bool const unknown_a{a.first == unknown_region};
      bool const unknown_b{b.first == unknown_region};
      if(unknown_a != unknown_b)
        return unknown_b;
      return size_of(*a.second) > size_of(*b.second);
    }
  );

  std::string page{std::format(
    "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><title>Sky album</title><style>{}</style></head>"
    "<body>\n<h1>Sky album</h1>\n<p class=\"sub\">{} systems known, {} with pictures, {} with life, {} with a first discovery. "
    "<a href=\"index.html\">Codex of finds</a></p>\n",
    page_style(),
    rows.size(),
    pictured,
    alive,
    firsts
  )};
  for(auto const & [region, sectors]: ordered)
    {
    page += std::format("<h2>{} <span class=\"n\">{} systems</span></h2>\n", escaped(region), size_of(*sectors));
    for(auto const & [sector, systems]: *sectors)
      {
      auto const with = [&](bool row_t::*flag) { return std::ranges::count_if(systems, [&](row_t const * row) { return row->*flag; }); };
      long const shots{with(&row_t::picture)};
      long const life{with(&row_t::life)};
      long const first{with(&row_t::first)};
      page += std::format(
        "<details><summary>{} <span class=\"n\">{} systems{}{}{}</span></summary><div class=\"list\">\n",
        escaped(sector),
        systems.size(),
        shots != 0 ? std::format(", &#128247; {}", shots) : std::string{},
        life != 0 ? std::format(", &#127807; {}", life) : std::string{},
        first != 0 ? std::format(", &#11088; {}", first) : std::string{}
      );
      for(row_t const * row: systems)
        page += std::format(
          "<div><a href=\"{}\">{}</a>{}{}{} <span class=\"n\">{}</span></div>\n",
          escaped(row->href),
          escaped(row->name),
          row->picture ? " &#128247;" : "",
          row->life ? " &#127807;" : "",
          row->first ? " &#11088;" : "",
          escaped(row->counts)
        );
      page += "</div></details>\n";
      }
    }
  page += "</body></html>\n";
  return page;
  }

auto write_if_changed(std::filesystem::path const & path, std::string const & text) -> bool
  {
  std::error_code ec;
  if(std::filesystem::exists(path, ec) and read_text(path) == text)
    return false;
  std::filesystem::path const partial{path.string() + ".part"};
    {
    std::ofstream out{partial, std::ios::binary | std::ios::trunc};
    out << text;
    if(not out)
      {
      spdlog::error("album: {} could not be written", partial.string());
      return false;
      }
    }
  std::filesystem::rename(partial, path, ec);
  if(ec)
    spdlog::error("album: {} could not be put in place: {}", path.string(), ec.message());
  return not ec;
  }

auto escaped(std::string_view text) -> std::string
  {
  std::string out;
  out.reserve(text.size());
  for(char const c: text)
    switch(c)
      {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c;
      }
  return out;
  }

auto page_style() -> std::string_view
  {
  return R"(:root{color-scheme:dark}
body{margin:0;padding:16px 24px;background:#0c0e11;color:#d6dbe1;font:15px/1.45 system-ui,sans-serif}
a{color:#9ad1ff;text-decoration:none}h1{font-size:24px;font-weight:600;margin:0}
h2{font-size:17px;font-weight:600;margin:22px 0 8px;color:#9ad1ff}
.sub,.n{color:#8a94a0;font-size:13px}.n{font-size:12px}
details{margin:2px 0}summary{cursor:pointer}.list{columns:3;column-gap:24px;margin:4px 0 10px 18px;font-size:13.5px}
.list div{break-inside:avoid}
.grid{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:10px}
.card{background:#14181d;border:1px solid #232a32;border-radius:6px;padding:8px;font-size:12.5px;scroll-margin-top:12px}
.card:target{border-color:#9ad1ff}.card b{font-size:14px;color:#fff}.card table{width:100%;border-collapse:collapse;margin-top:4px}
.card td{padding:0 0 1px}.card td:last-child{text-align:right;color:#fff}
.bio{margin-top:6px;border-top:1px solid #232a32;padding-top:4px;color:#7fe0a0}.bio .n{color:#8a94a0}
.thumbs{display:flex;flex-wrap:wrap;gap:3px;margin-top:4px}.thumbs img{height:52px;border-radius:3px;display:block}
.sky{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:8px}.sky img{width:100%;border-radius:4px;display:block}
figure{margin:0}figcaption{font-size:12px;color:#8a94a0}.first{color:#3cb371}
.badges{color:#e0c060;font-size:11.5px}.air{color:#8a94a0;font-size:11.5px;margin-top:3px}
.map{overflow-x:auto;background:#10141a;border-radius:6px;padding:6px}
)";
  }
  }  // namespace system_index
