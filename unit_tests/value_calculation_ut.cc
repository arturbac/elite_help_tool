#include <boost/ut.hpp>
#include <string_view>
#include <cmath>
#include <algorithm>
#include <elite_events.h>

auto main() -> int
  {
  using namespace boost::ut;
  static constexpr planet_value_info_t hmc_info{
    .planet_class = "High metal content body", .base_value = 9'693.0, .terraform_bonus = 93'328.0
  };
  "elite_dangerous_valuation"_test = []
  {
    "A 1: high metal content (non terraformable)"_test = []
    {
      // MassEM: 0.090840, TerraformState: "", First Disc/Map: true
      auto const val = exploration::calculate_value(hmc_info, 0.090840, false, true, true, true);

      // expected value: about 114k
      expect(val >= 110'000_u && val <= 120'000_u) << "Actual value:" << val;
    };

    "A 5: high metal content (terraformable)"_test = []
    {
      // MassEM: 0.070008, TerraformState: "Terraformable", First Disc/Map: true
      auto const val = exploration::calculate_value(hmc_info, 0.070008, true, true, true, true);

      // expected value: > 1.1 million CR
      // (Base 103k * MassQ 0.587) * (1 + 3.33 * 1.25) * 3.695
      expect(val > 1'100'000_u) << "Value too low for terraformable! Actual:" << val;
    };

    "A 6: high metal content (terraformable)"_test = []
    {
      // MassEM: 0.076945, TerraformState: "Terraformable", First Disc/Map: true
      auto const val = exploration::calculate_value(hmc_info, 0.076945, true, true, true, true);

      // expected value: > 1.15 million CR
      expect(val > 1'150'000_u) << "Value too low for terraformable! Actual:" << val;
    };

    "logic error: a terraformable treated as an ordinary body"_test = []
    {
      // a simulation of the bug mentioned (it returns 100k)
      auto const val_error = exploration::calculate_value(hmc_info, 0.070008, false, true, true, true);

      expect(val_error < 115'000_u) << "Value matches the '100k error' mentioned by user";
    };
  };
  
  "elite_dangerous_valuation"_test = []
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
