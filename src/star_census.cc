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
  // the galaxy map's star filter, in its own order; the classes not a letter take the types below
  constexpr std::array map_classes{
    kind_t{"Proto", "T Tauri and Herbig Ae/Be, young stars"},
    kind_t{"Carbon", "carbon and S-type stars"},
    kind_t{"Wolf-Rayet", "Wolf-Rayet stars"},
    kind_t{"White dwarf", "white dwarfs"},
    kind_t{"Non sequence", "neutron stars and black holes"},
  };

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
  if(auto const it{std::ranges::find(kinds, type, &kind_t::type)}; it != kinds.end())
    return it->what;
  auto const it{std::ranges::find(map_classes, type, &kind_t::type)};
  return it == map_classes.end() ? std::string_view{} : it->what;
  }

auto rank(std::string_view type) noexcept -> int
  {
  // the letters O to Y are the first ten kinds, as on the map; the map's other classes after them
  constexpr int letters{10};
  if(auto const it{std::ranges::find(map_classes, type, &kind_t::type)}; it != map_classes.end())
    return letters + int(it - map_classes.begin());
  auto const it{std::ranges::find(kinds, type, &kind_t::type)};
  int const at{it == kinds.end() ? int(kinds.size()) : int(it - kinds.begin())};
  return at < letters ? at : at + int(map_classes.size());
  }

auto map_class(std::string_view type) noexcept -> std::string_view
  {
  if(type == "TTS" or type == "AeBe")
    return "Proto";
  if(type == "N" or type == "H" or type == "SupermassiveBlackHole")
    return "Non sequence";
  if(type.starts_with('W'))
    return "Wolf-Rayet";
  if(type.starts_with('D'))
    return "White dwarf";
  if(type.starts_with('C') or type == "S" or type == "MS")
    return "Carbon";
  // a giant or a supergiant, K_OrangeGiant, under the letter of its type
  if(type.size() > 2u and type[1] == '_')
    return type.substr(0u, 1u);
  return type;
  }

auto key_of(star_t const & star, grouping_e grouping) -> std::string
  {
  switch(grouping)
    {
    case grouping_e::sub_class:  return std::format("{}{}", star.type, star.sub_class);
    case grouping_e::luminosity: return star.luminosity.empty() ? star.type : std::format("{} {}", star.type, star.luminosity);
    case grouping_e::map_filter: return std::string{map_class(star.type)};
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
