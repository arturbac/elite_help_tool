#include <boost/ut.hpp>
#include <biology.h>
#include <array>

auto main() -> int
  {
  using namespace boost::ut;

  "surface"_test = []
  {
    "a degree of longitude on the equator"_test = []
    {
      // a body of 1000 km radius - a degree is 2*pi*1e6/360 = 17453 m
      auto const d{bio::surface_distance_m({0.0, 0.0}, {0.0, 1.0}, 1'000'000.0)};
      expect(d > 17'450.0 and d < 17'456.0) << d;
    };

    "a step near the pole is short even across many degrees of longitude"_test = []
    {
      auto const d{bio::surface_distance_m({89.999, 0.0}, {89.999, 90.0}, 1'000'000.0)};
      expect(d < 30.0) << d;
    };

    "bearing east and north"_test = []
    {
      expect(std::abs(bio::bearing_deg({0.0, 0.0}, {0.0, 1.0}) - 90.0) < 0.01);
      expect(std::abs(bio::bearing_deg({0.0, 0.0}, {1.0, 0.0}) - 0.0) < 0.01);
      expect(std::abs(bio::bearing_deg({0.0, 0.0}, {-1.0, 0.0}) - 180.0) < 0.01);
    };
  };

  "colony"_test = []
  {
    expect(bio::colony_range_m("Bacterium") == 500_u);
    expect(bio::colony_range_m("Osseus") == 800_u);
    expect(bio::colony_range_m("Luteolum Anemone") == 100_u);
    expect(bio::colony_range_m("Brain Trees") == 100_u);
    expect(bio::colony_range_m("Nothing") == 0_u);
  };

  "value"_test = []
  {
    expect(bio::species_value("Stratum Tectonicas") == 19'010'800u);
    // the journal's name for a brain tree carries its kind in front of the family
    expect(bio::species_value("Roseum Brain Tree") == 1'593'700u);
    expect(not bio::species_value("Nothing Whatsoever").has_value());
  };

  "predict"_test = []
  {
    using bio::species_record_t;
    std::array const history{
      species_record_t{
        "Stratum", "Stratum Tectonicas", "High metal content body", "CarbonDioxide", "", 170.0, 5.0, 1000.0, "M"
      },
      species_record_t{
        "Stratum", "Stratum Tectonicas", "High metal content body", "CarbonDioxide", "", 180.0, 4.0, 1000.0, "K"
      },
      species_record_t{"Stratum", "Stratum Excutitus", "Rocky body", "SulphurDioxide", "", 170.0, 2.0, 1000.0, "K"},
      species_record_t{"Stratum", "Stratum Paleas", "Rocky body", "CarbonDioxide", "", 250.0, 3.0, 1000.0, "F"},
      species_record_t{"Bacterium", "Bacterium Aurasus", "Rocky body", "CarbonDioxide", "", 175.0, 4.0, 1000.0, "K"},
    };

    bio::conditions_t const world{
      .planet_class = "High metal content body",
      .atmosphere_type = "CarbonDioxide",
      .volcanism = "",
      .surface_temperature = 175.0,
      .surface_gravity = 4.5,
      .surface_pressure = 1000.0,
      .star_type = "K"
    };

    auto const guess{bio::predict("Stratum", world, history)};
    expect(fatal(guess.size() == 2_u)) << "the sulphur-dioxide species must drop out";
    expect(guess[0].species == "Stratum Tectonicas");
    expect(guess[0].fit == bio::fit_e::fits);
    expect(guess[0].seen == 2_u);
    expect(guess[0].share == 1.0_d);
    expect(guess[1].species == "Stratum Paleas");
    expect(guess[1].fit == bio::fit_e::near) << "seen only far warmer";

    "a genus never seen under this atmosphere still says what it was elsewhere"_test = [&]
    {
      auto const other{bio::predict("Stratum", bio::conditions_t{.atmosphere_type = "Neon"}, history)};
      expect(other.size() == 3_u);
      expect(other[0].fit == bio::fit_e::unlike);
    };
  };
  }
