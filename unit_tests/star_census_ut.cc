#include <star_census.h>

#include <boost/ut.hpp>

#include <array>

using namespace boost::ut;
using namespace std::string_view_literals;

auto main() -> int
  {
  using star_census::star_t;
  using star_census::system_t;
  // a binary of a K arrival star and a far M, a lone G, an M with a T dwarf beside it, all of one region but the last
  std::array const systems{
    system_t{.address = 1u, .stars = {star_t{"K", 7u, "Va", 0.0}, star_t{"M", 3u, "Va", 1200.0}}},
    system_t{.address = 2u, .stars = {star_t{"G", 9u, "Vab", 0.0}}},
    system_t{.address = 3u, .stars = {star_t{"T", 4u, "V", 300.0}, star_t{"M", 3u, "Vab", 0.0}}},
    system_t{.address = 4u, .stars = {star_t{"G", 3u, "Vab", 0.0}}}
  };
  auto const all = [](system_t const &) { return true; };

  "the arrival star counts once for its system, every star under all"_test = [&]
  {
    auto const census{star_census::count(systems, star_census::grouping_e::type, all)};
    expect(census.systems == 4_u);
    expect(fatal(census.rows.size() == 4_ul));
    expect(census.rows[0].key == "G"sv and census.rows[0].arrival == 2_u and census.rows[0].all == 2_u);
    expect(census.rows[1].key == "K"sv and census.rows[1].arrival == 1_u and census.rows[1].all == 1_u);
    expect(census.rows[2].key == "M"sv and census.rows[2].arrival == 1_u and census.rows[2].all == 2_u)
      << "an M arrival star of one system, a companion in another";
    expect(census.rows[3].key == "T"sv and census.rows[3].arrival == 0_u and census.rows[3].all == 1_u)
      << "the brown dwarfs after the main sequence";
  };

  "by subclass and by luminosity"_test = [&]
  {
    auto const by_sub{star_census::count(systems, star_census::grouping_e::sub_class, all)};
    expect(fatal(by_sub.rows.size() == 5_ul));
    expect(by_sub.rows[0].key == "G3"sv and by_sub.rows[1].key == "G9"sv) << "hotter first";
    auto const by_light{star_census::count(systems, star_census::grouping_e::luminosity, all)};
    expect(by_light.rows[0].key == "G Vab"sv and by_light.rows[0].all == 2_u);
    expect(by_light.rows[2].key == "M Va"sv and by_light.rows[3].key == "M Vab"sv);
  };

  "a region counts only its own systems"_test = [&]
  {
    auto const census{
      star_census::count(systems, star_census::grouping_e::type, [](system_t const & s) { return s.address != 4u; })
    };
    expect(census.systems == 3_u);
    expect(census.rows[0].key == "G"sv and census.rows[0].arrival == 1_u);
  };

  "what a star is"_test = []
  {
    expect(star_census::description("N") == "neutron star"sv);
    expect(star_census::description("DA") == "white dwarf"sv);
    expect(star_census::description("nothing").empty());
    expect(star_census::rank("O") < star_census::rank("M") and star_census::rank("M") < star_census::rank("Y"));
  };
  }
