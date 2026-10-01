#include <boost/ut.hpp>
#include <string_view>
#include <cmath>
#include <algorithm>
#include <ranges>
#include <exploration_value.h>

auto main() -> int
  {
  using namespace boost::ut;
  // every expected value below is what the game paid in a real sale of a single system (BaseValue)
  static constexpr auto info = [](std::string_view planet_class) -> planet_value_info_t const &
  { return *std::ranges::find(exploration_values, planet_class, &planet_value_info_t::planet_class); };

  "planet_value"_test = []
  {
    "a small icy body is worth the floor of 500 and the Odyssey bonus of 500"_test = []
    {
      expect(exploration::scanned_value(info("Icy body"), 0.642746, false, false) == 1'000_u);
      expect(exploration::scanned_value(info("Rocky ice body"), 4.986541, false, false) == 1'034_u);
    };

    "the whole sale of Hegai QK-F d11-4, a terraformable mapped by its first mapper"_test = []
    {
      uint64_t const sum{
        exploration::star_value("F", 1.492188)
        + exploration::calculate_value(info("High metal content body"), 0.56849, true, false, true, true)
        + exploration::scanned_value(info("High metal content body"), 1.082288, false, false)
        + exploration::scanned_value(info("High metal content body"), 0.388736, false, false)
        + exploration::scanned_value(info("High metal content body"), 2.6096, false, false)
        + exploration::scanned_value(info("High metal content body"), 2.076126, false, false)
        + exploration::scanned_value(info("Rocky ice body"), 4.986541, false, false)
        + exploration::scanned_value(info("High metal content body"), 2.50014, false, false)
      };
      // paid 2'346'435 - within a hundredth of a percent
      expect(sum >= 2'346'200_ull and sum <= 2'346'700_ull) << "estimate:" << sum;
    };

    "a terraformable body is worth far more than a plain one"_test = []
    {
      auto const plain{
        exploration::calculate_value(info("High metal content body"), 0.070008, false, true, true, true)
      };
      auto const terraformable{
        exploration::calculate_value(info("High metal content body"), 0.070008, true, true, true, true)
      };
      expect(terraformable > plain * 5u) << "plain:" << plain << "terraformable:" << terraformable;
    };
  };

  "aprox_value"_test = []
  {
    body_t b{
      .details = planet_details_t{
        .terraform_state = ::events::terraform_state_e::Terraformable,
        .planet_class = "High metal content body",
        .mass_em = 0.070008,
        .was_mapped = false
      },
      .was_discovered = false
    };
    auto const value {exploration::aprox_value(b)};
    expect(value > 1'000'000);
  };

  "star_value"_test = []
  {
    "a supergiant uses the same base as an ordinary star, not the old 33.0 bug"_test = []
    {
      auto const supergiant{exploration::star_value("K_OrangeSuperGiant", 5.0)};
      auto const ordinary{exploration::star_value("K", 5.0)};
      expect(supergiant == ordinary) << "supergiant:" << supergiant << "ordinary:" << ordinary;
      expect(supergiant > 1'000_u) << "supergiant value too low:" << supergiant;
    };

    "the arrival star, discovered first - Gludgae DZ-N c20-0"_test
      = [] { expect(exploration::star_value("K", 0.597656, true) == 3'148_u); };

    "companion stars pay a third more - Hyades Sector YY-R b4-4"_test = []
    {
      auto const sum{
        exploration::star_value("L", 0.203125) + exploration::star_value("T", 0.070313, false, false)
        + exploration::star_value("L", 0.160156, false, false)
      };
      expect(sum == 4'409_u) << "estimate:" << sum;
    };

    "a scanned star body gets a non-zero value, unlike the pre-fix always-0 bug"_test = []
    {
      body_t b{
        .details = star_details_t{.system_address = 1, .star_type = "K", .luminosity = "V", .stellar_mass = 1.0},
        .was_discovered = false
      };
      auto const value{exploration::aprox_value(b)};
      expect(value > 0_u) << "star value must not be 0";
    };
  };

  "organic_values"_test = []
  {
    "a species hits the price list directly"_test = []
    {
      auto const value{organic_value_range("Cactoida Cortexum")};
      expect(value.has_value());
      expect(value->first == 3'667'600_u and value->second == 3'667'600_u);
    };

    "a genus gives the span of the whole family"_test = []
    {
      auto const value{organic_value_range("Bacterium")};
      expect(value.has_value());
      expect(value->first == 1'000'000_u and value->second == 8'418'000_u);
    };

    "the plural of a genus out of the journal"_test = []
    {
      // the journal gives "Brain Trees", the price list knows "Brain Tree"
      auto const value{organic_value_range("Brain Trees")};
      expect(value.has_value());
      expect(value->first == 1'593'700_u and value->second == 1'593'700_u);
    };

    "a genus with a variant prefix"_test = []
    {
      // the journal gives "Luteolum Anemone", the price list knows "Anemone" alone
      auto const value{organic_value_range("Luteolum Anemone")};
      expect(value.has_value());
      expect(value->first == 1'499'900_u);
    };

    "a name outside the price list returns nothing"_test = []
    { expect(not organic_value_range("Nieistniejacy Gatunek").has_value()); };
  };
  }
