#include <star_census.h>

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace star_census
  {
namespace
  {
  struct kind_t
    {
    std::string_view type;
    std::string_view what;
    };

  // the order of the rows: the main sequence from the hottest, the brown dwarfs, then the young, the giants and
  // what is left of a star
  constexpr std::array kinds{
    kind_t{"O", "blue-white, the hottest"},
    kind_t{"B", "blue-white"},
    kind_t{"A", "white"},
    kind_t{"F", "yellow-white"},
    kind_t{"G", "yellow, as the Sun"},
    kind_t{"K", "orange dwarf"},
    kind_t{"M", "red dwarf"},
    kind_t{"L", "brown dwarf"},
    kind_t{"T", "brown dwarf"},
    kind_t{"Y", "brown dwarf, the coolest"},
    kind_t{"TTS", "T Tauri, a young star"},
    kind_t{"AeBe", "Herbig Ae/Be, a young star"},
    kind_t{"A_BlueWhiteSuperGiant", "blue-white supergiant"},
    kind_t{"F_WhiteSuperGiant", "white supergiant"},
    kind_t{"B_BlueWhiteSuperGiant", "blue-white supergiant"},
    kind_t{"G_WhiteSuperGiant", "white supergiant"},
    kind_t{"K_OrangeGiant", "orange giant"},
    kind_t{"M_RedGiant", "red giant"},
    kind_t{"M_RedSuperGiant", "red supergiant"},
    kind_t{"W", "Wolf-Rayet"},
    kind_t{"WN", "Wolf-Rayet"},
    kind_t{"WNC", "Wolf-Rayet"},
    kind_t{"WC", "Wolf-Rayet"},
    kind_t{"WO", "Wolf-Rayet"},
    kind_t{"CS", "carbon star"},
    kind_t{"C", "carbon star"},
    kind_t{"CN", "carbon star"},
    kind_t{"CJ", "carbon star"},
    kind_t{"CH", "carbon star"},
    kind_t{"CHd", "carbon star"},
    kind_t{"MS", "S-type star"},
    kind_t{"S", "S-type star"},
    kind_t{"D", "white dwarf"},
    kind_t{"DA", "white dwarf"},
    kind_t{"DAB", "white dwarf"},
    kind_t{"DAO", "white dwarf"},
    kind_t{"DAZ", "white dwarf"},
    kind_t{"DAV", "white dwarf"},
    kind_t{"DB", "white dwarf"},
    kind_t{"DBZ", "white dwarf"},
    kind_t{"DBV", "white dwarf"},
    kind_t{"DO", "white dwarf"},
    kind_t{"DOV", "white dwarf"},
    kind_t{"DQ", "white dwarf"},
    kind_t{"DC", "white dwarf"},
    kind_t{"DCV", "white dwarf"},
    kind_t{"DX", "white dwarf"},
    kind_t{"N", "neutron star"},
    kind_t{"H", "black hole"},
    kind_t{"SupermassiveBlackHole", "supermassive black hole"},
  };
  }  // namespace

auto description(std::string_view type) noexcept -> std::string_view
  {
  auto const it{std::ranges::find(kinds, type, &kind_t::type)};
  return it == kinds.end() ? std::string_view{} : it->what;
  }

auto rank(std::string_view type) noexcept -> int
  {
  auto const it{std::ranges::find(kinds, type, &kind_t::type)};
  return it == kinds.end() ? int(kinds.size()) : int(it - kinds.begin());
  }

auto key_of(star_t const & star, grouping_e grouping) -> std::string
  {
  switch(grouping)
    {
    case grouping_e::sub_class:  return std::format("{}{}", star.type, star.sub_class);
    case grouping_e::luminosity: return star.luminosity.empty() ? star.type : std::format("{} {}", star.type, star.luminosity);
    case grouping_e::type:       break;
    }
  return star.type;
  }

auto sorted(std::vector<row_t> rows) -> std::vector<row_t>
  {
  std::ranges::sort(
    rows,
    [](row_t const & a, row_t const & b)
    {
      int const ra{rank(a.type)};
      int const rb{rank(b.type)};
      if(ra != rb)
        return ra < rb;
      // the same kind: hotter subclasses first, which the key's order gives, and a type not known by its name
      return a.key < b.key;
    }
  );
  return rows;
  }
  }  // namespace star_census
