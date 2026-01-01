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
    "A 1: High Metal Content (Nie-terraformowalna)"_test = []
    {
      // MassEM: 0.090840, TerraformState: "", First Disc/Map: true
      auto const val = exploration::calculate_value(hmc_info, 0.090840, false, true, true, true);

      // Oczekiwana wartość: ok. 114k
      expect(val >= 110'000_u && val <= 120'000_u) << "Actual value:" << val;
    };

    "A 5: High Metal Content (Terraformowalna)"_test = []
    {
      // MassEM: 0.070008, TerraformState: "Terraformable", First Disc/Map: true
      auto const val = exploration::calculate_value(hmc_info, 0.070008, true, true, true, true);

      // Oczekiwana wartość: > 1.1 mln CR
      // (Base 103k * MassQ 0.587) * (1 + 3.33 * 1.25) * 3.695
      expect(val > 1'100'000_u) << "Value too low for terraformable! Actual:" << val;
    };

    "A 6: High Metal Content (Terraformowalna)"_test = []
    {
      // MassEM: 0.076945, TerraformState: "Terraformable", First Disc/Map: true
      auto const val = exploration::calculate_value(hmc_info, 0.076945, true, true, true, true);

      // Oczekiwana wartość: > 1.15 mln CR
      expect(val > 1'150'000_u) << "Value too low for terraformable! Actual:" << val;
    };

    "Błąd logiki: Terraformable traktowana jako zwykła"_test = []
    {
      // Symulacja błędu, o którym wspominasz (zwraca 100k)
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
    auto const value {exploration::aprox_value(b).value};
    expect(value > 1'000'000);
  };
  }
