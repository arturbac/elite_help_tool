#include <boost/ut.hpp>
#include <orbit.h>

#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>
#include <span>
#include <optional>

namespace
  {
using ::events::body_location_t;

auto near(double a, double b, double eps = 1e-6) -> bool { return std::abs(a - b) < eps; }

///\brief a fixed moment stood in for "the scan just happened" - every fixture below is both scanned and
/// asked about at this same instant, so dt is zero and the checks below stay about the geometry alone
auto const epoch{std::chrono::sys_days{std::chrono::year{2026} / 7 / 24} + std::chrono::hours{14}
                  + std::chrono::minutes{17} + std::chrono::seconds{13}};

///\brief a lone star, or a star at the far end of a Parents ladder - everything left at its zero default
auto make_star(
  ::events::body_id_t body_id,
  std::string star_system,
  uint64_t system_address,
  std::string star_type,
  double semi_major_axis = 0.0,
  double eccentricity = 0.0,
  double orbital_inclination = 0.0,
  double periapsis = 0.0,
  double orbital_period = 0.0,
  double ascending_node = 0.0,
  double mean_anomaly = 0.0,
  std::vector<::events::parent_t> parents = {}
) -> ::events::scan_detailed_scan_t
  {
  return ::events::scan_detailed_scan_t{
    .BodyName = star_system,
    .Rings = {},
    .RotationPeriod = {},
    .AxialTilt = {},
    .DistanceFromArrivalLS = 0.0,
    .SemiMajorAxis = semi_major_axis,
    .Eccentricity = eccentricity,
    .OrbitalInclination = orbital_inclination,
    .Periapsis = periapsis,
    .OrbitalPeriod = orbital_period,
    .BodyID = body_id,
    .WasDiscovered = true,
    .WasMapped = false,
    .StarSystem = star_system,
    .StarType = star_type,
    .Luminosity = "V",
    .SystemAddress = system_address,
    .StellarMass = 0.5,
    .Radius = 300'000'000.0,
    .AbsoluteMagnitude = 10.0,
    .SurfaceTemperature = 3000.0,
    .Age_MY = 4000,
    .Subclass = 5,
    .Parents = std::move(parents),
    .TerraformState = {},
    .PlanetClass = {},
    .Atmosphere = {},
    .AtmosphereType = {},
    .AtmosphereComposition = {},
    .Volcanism = {},
    .Composition = {},
    .MassEM = 0.0,
    .SurfaceGravity = 0.0,
    .SurfacePressure = 0.0,
    .AscendingNode = ascending_node,
    .MeanAnomaly = mean_anomaly,
    .Landable = false,
    .TidalLock = false,
    .WasFootfalled = false
  };
  }

auto make_planet(
  ::events::body_id_t body_id,
  std::string star_system,
  uint64_t system_address,
  double semi_major_axis,
  double eccentricity,
  double orbital_inclination,
  double periapsis,
  double orbital_period,
  double ascending_node,
  double mean_anomaly,
  std::vector<::events::parent_t> parents
) -> ::events::scan_detailed_scan_t
  {
  return ::events::scan_detailed_scan_t{
    .BodyName = star_system,
    .Rings = {},
    .RotationPeriod = {},
    .AxialTilt = {},
    .DistanceFromArrivalLS = 0.0,
    .SemiMajorAxis = semi_major_axis,
    .Eccentricity = eccentricity,
    .OrbitalInclination = orbital_inclination,
    .Periapsis = periapsis,
    .OrbitalPeriod = orbital_period,
    .BodyID = body_id,
    .WasDiscovered = true,
    .WasMapped = false,
    .StarSystem = star_system,
    .StarType = {},
    .Luminosity = {},  // empty - what tells to_body() this is a planet, not a star
    .SystemAddress = system_address,
    .StellarMass = 0.0,
    .Radius = 3'000'000.0,
    .AbsoluteMagnitude = 0.0,
    .SurfaceTemperature = 200.0,
    .Age_MY = 0,
    .Subclass = 0,
    .Parents = std::move(parents),
    .TerraformState = {},
    .PlanetClass = "Icy body",
    .Atmosphere = {},
    .AtmosphereType = {},
    .AtmosphereComposition = {},
    .Volcanism = {},
    .Composition = {},
    .MassEM = 0.01,
    .SurfaceGravity = 1.0,
    .SurfacePressure = 0.0,
    .AscendingNode = ascending_node,
    .MeanAnomaly = mean_anomaly,
    .Landable = true,
    .TidalLock = false,
    .WasFootfalled = false
  };
  }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;

  "solve_kepler equation residual"_test = []
  {
    // the solver's own free functions are not exported - order_calculation is, and it is exercised
    // below with real orbits; here the physics itself is checked through the values the game gave us
    for(double const e: {0.0, 0.165806, 0.268665, 0.9})
      for(double const m_deg: {0.0, 45.0, 90.0, 180.0, 270.0, 315.109137})
        {
        double const m{m_deg * std::numbers::pi / 180.0};
        // a hand-rolled Newton solve, independent of the production one, to check E - e*sin(E) = M holds
        // for the same inputs the production code is fed below
        double eanom{m};
        for(int i{}; i != 30; ++i)
          eanom -= (eanom - e * std::sin(eanom) - m) / (1.0 - e * std::cos(eanom));
        expect(near(eanom - e * std::sin(eanom), m, 1e-9)) << "e=" << e << " m=" << m_deg;
        }
  };

  "the bodies to visit, in the order to fly them"_test = []
  {
    auto const planet_at = [](::events::body_id_t id, double semi_major_axis)
    {
      auto body{to_body(make_planet(
        id, "Bleia Eohn BD-I a64-0", 5342402204192, semi_major_axis, 0.0, 0.0, 0.0, 86400.0, 0.0, 0.0, {::events::parent_t{.Star = 0}}
      ))};
      body.scanned_at = epoch;
      return body;
    };
    // all on one line out from the star, and given out of order
    auto const near_one{planet_at(1, 1e11)};
    auto const far_one{planet_at(2, 3e11)};
    auto const middle{planet_at(3, 2e11)};
    std::vector<body_t const *> const bodies{&near_one, &far_one, &middle};
    std::array<::events::body_id_t, 3> const all{2, 3, 1};

    auto const outwards{visit_order({}, bodies, all, std::nullopt, epoch)};
    expect(fatal(outwards.size() == 3_ul));
    expect(outwards[0].body_id == 1_u and outwards[1].body_id == 3_u and outwards[2].body_id == 2_u);
    expect(near(outwards[0].distance_ls, 1e11 / 299'792'458.0, 1e-3)) << "the first leg is from the system's centre";

    std::array<::events::body_id_t, 2> const two{1, 3};
    auto const inwards{visit_order({}, bodies, two, ::events::body_id_t{2}, epoch)};
    expect(fatal(inwards.size() == 2_ul));
    expect(inwards[0].body_id == 3_u and inwards[1].body_id == 1_u) << "from the body the commander is at";
    expect(near(inwards[1].from_start_ls, 2e11 / 299'792'458.0, 1e-3)) << "the last stop straight from the start";

    std::array<::events::body_id_t, 1> const one{3};
    auto const single{visit_order({}, bodies, one, std::nullopt, epoch)};
    expect(fatal(single.size() == 1_ul));
    expect(single[0].body_id == 3_u) << "the commander's own place is no stop";

    expect(visit_order({}, bodies, {}, std::nullopt, epoch).empty());
  };

  "a pair of planets round the far star of a binary stands by that star"_test = []
  {
    // "Spoihee YG-B d14-358" from a real journal: A and B round the system's barycentre 0, about 26k Ls
    // apart; A 2 and A 3 round A, B 2 with B 3 round barycentre 27, which circles B. From A the route is
    // A 2, A 3, then B 2 - not B 2 first, as when barycentre 27 was left at the system's origin
    auto const system{"Spoihee YG-B d14-358"};
    std::uint64_t const address{12309786939771};
    auto star_a{to_body(make_star(
      1, system, address, "K", 2473141551017.761, 0.200979, 3.932337, 120.775366, 10570206642.150879, -166.418946,
      63.595394, {::events::parent_t{.Null = 0}}
    ))};
    auto star_b{to_body(make_star(
      2, system, address, "M", 5787709712982.178, 0.200979, 3.932337, 300.775361, 10570206642.150879, -166.418946,
      63.595394, {::events::parent_t{.Null = 0}}
    ))};
    auto a2{to_body(make_planet(
      11, system, address, 172980898618.69812, 0.001749, -0.949956, 117.941374, 38265066.742897, -148.017661,
      86.145459, {::events::parent_t{.Star = 1}, ::events::parent_t{.Null = 0}}
    ))};
    auto a3{to_body(make_planet(
      12, system, address, 321699798107.1472, 0.000514, -0.004964, 338.584065, 97046853.303909, -94.249213,
      93.974539, {::events::parent_t{.Star = 1}, ::events::parent_t{.Null = 0}}
    ))};
    auto b2{to_body(make_planet(
      28, system, address, 106204211.711884, 0.015632, -7.009799, 186.95128, 2960228.741169, -27.729052, 236.052226,
      {::events::parent_t{.Null = 27}, ::events::parent_t{.Star = 2}, ::events::parent_t{.Null = 0}}
    ))};
    for(body_t * body: {&star_a, &star_b, &a2, &a3, &b2})
      body->scanned_at = epoch;
    bary_centre_t const barycentre27{
      .body_id = 27,
      .semi_major_axis = 132058167457.58057,
      .eccentricity = 0.002959,
      .orbital_inclination = -0.024915,
      .periapsis = 277.727981,
      .orbital_period = 39046494.364738,
      .ascending_node = 69.140052,
      .mean_anomaly = 168.73794,
      .scanned_at = epoch
    };
    std::vector<body_t const *> const bodies{&star_a, &star_b, &a2, &a3, &b2};
    std::array<::events::body_id_t, 3> const chosen{28, 11, 12};

    for(std::vector<bary_centre_t> const barycentres: {std::vector{barycentre27}, std::vector<bary_centre_t>{}})
      {
      auto const route{visit_order(barycentres, bodies, chosen, ::events::body_id_t{1}, epoch)};
      expect(fatal(route.size() == 3_ul));
      expect(route[0].body_id == 11_u and route[1].body_id == 12_u and route[2].body_id == 28_u)
        << "barycentre scanned:" << not barycentres.empty();
      expect(route[2].distance_ls > 20'000.0) << "B 2 lies by B, far from A 3:" << route[2].distance_ls;
      }
  };

  "the planets stand where the game's system map shows them"_test = []
  {
    // two systems Sjona scanned, at the moments of two screenshots of the game's system map (12.11): the
    // angle of each planet round the star read off the picture. The map is the mirror of these coordinates
    // (distances are the same either way) and turned by whatever the view was, so it is the spread of the
    // differences that counts, not the differences themselves. Planet 11 of MI-Q d6-67 goes round backwards
    struct planet_t
      {
      ::events::body_id_t id;
      double sma, ecc, incl, periapsis, period, node, anomaly;
      std::chrono::sys_seconds scanned;
      std::optional<::events::body_id_t> barycentre;
      double screen_x, screen_y;
      };
    auto const at = [](int month, int day, int h, int m, int sec)
    {
      return std::chrono::sys_seconds{
        std::chrono::sys_days{std::chrono::year{2026} / month / day} + std::chrono::hours{h}
        + std::chrono::minutes{m} + std::chrono::seconds{sec}
      };
    };
    auto const spread = [](std::span<planet_t const> planets, std::vector<body_t> const & bodies,
                           std::span<bary_centre_t const> barycentres, std::chrono::sys_seconds now,
                           double star_x, double star_y)
    {
      std::vector<body_t const *> scans;
      for(body_t const & body: bodies)
        scans.push_back(&body);
      auto const positions{body_positions_now(barycentres, scans, now)};
      std::vector<double> differences;
      for(size_t ix{}; ix != planets.size(); ++ix)
        {
        if(planets[ix].screen_x == 0.0)
          continue;
        double const screen{std::atan2(-(planets[ix].screen_y - star_y), planets[ix].screen_x - star_x)};
        double const mirrored{-std::atan2(positions[ix].y, positions[ix].x)};
        differences.push_back(screen - mirrored);
        }
      double s{}, c{};
      for(double const d: differences)
        {
        s += std::sin(d);
        c += std::cos(d);
        }
      double const turn{std::atan2(s, c)};
      double worst{};
      for(double const d: differences)
        worst = std::max(worst, std::abs(std::remainder(d - turn, 2.0 * std::numbers::pi)));
      return worst * 180.0 / std::numbers::pi;
    };
    auto const bodies_of = [](std::span<planet_t const> planets, std::string const & system, uint64_t address)
    {
      std::vector<body_t> bodies;
      for(planet_t const & p: planets)
        {
        std::vector<::events::parent_t> parents;
        if(p.barycentre)
          parents.push_back(::events::parent_t{.Null = *p.barycentre});
        parents.push_back(::events::parent_t{.Star = 0});
        bodies.push_back(to_body(make_planet(
          p.id, system, address, p.sma, p.ecc, p.incl, p.periapsis, p.period, p.node, p.anomaly, std::move(parents)
        )));
        bodies.back().scanned_at = p.scanned;
        }
      return bodies;
    };

    // "Bleia Eohn PP-N a61-6", 1 to 9; 2 and 3 round barycentre 3, which the picture shows as one
    std::array<planet_t, 9> const pp_n{
      planet_t{1, 173187613487.24365, 0.001278, 0.20431, 110.798403, 209592664.241791, -21.010639, 20.99779, at(8, 8, 5, 30, 7), {}, 1086, 1022},
      planet_t{4, 641176784.038544, 0.084548, -3.363018, 150.165766, 17913373.708725, -107.826134, 115.057329, at(8, 8, 5, 29, 44), 3, 993, 800},
      planet_t{5, 699011027.812958, 0.084548, -3.363018, 330.165761, 17913373.708725, -107.826134, 115.057386, at(8, 8, 5, 29, 47), 3, 0, 0},
      planet_t{6, 326244741678.2379, 0.001141, -1.879582, 253.169124, 541896259.784698, 165.936543, 309.504262, at(8, 8, 5, 30, 0), {}, 970, 1115},
      planet_t{8, 427183449268.34106, 0.002079, 0.572462, 84.430806, 811937314.271927, 154.538426, 216.337658, at(8, 8, 5, 29, 23), {}, 1266, 1038},
      planet_t{10, 613118195533.7524, 0.005568, 2.573938, 61.229169, 1396102070.808411, 32.470945, 117.926744, at(8, 8, 5, 29, 32), {}, 1373, 803},
      planet_t{12, 733016848564.148, 0.013778, 0.117762, 75.866838, 1825037658.214569, -58.183298, 314.890345, at(8, 8, 5, 30, 14), {}, 1200, 1332},
      planet_t{14, 1017308413982.3914, 0.002991, -7.444695, 287.804514, 2983869254.589081, -146.306859, 285.402605, at(8, 8, 5, 29, 50), {}, 579, 548},
      planet_t{15, 1432673394680.0232, 0.003736, 0.029737, 266.067717, 4986796855.926514, 10.49562, 312.578509, at(8, 8, 5, 29, 38), {}, 1756, 503}
    };
    std::array<bary_centre_t, 1> const pp_n_barycentre{bary_centre_t{
      .body_id = 3,
      .semi_major_axis = 238004529476.16577,
      .eccentricity = 0.001177,
      .orbital_inclination = 0.019838,
      .periapsis = 294.854728,
      .orbital_period = 337659269.571304,
      .ascending_node = 89.880515,
      .mean_anomaly = 127.745528,
      .scanned_at = at(8, 8, 5, 29, 44)
    }};
    auto const pp_n_now{at(11, 11, 23, 6, 37)};
    auto const pp_n_bodies{bodies_of(pp_n, "Bleia Eohn PP-N a61-6", 110897666085384)};
    double const pp_n_spread{spread(pp_n, pp_n_bodies, pp_n_barycentre, pp_n_now, 1034, 935)};
    expect(pp_n_spread < 3.0) << "PP-N a61-6, degrees:" << pp_n_spread;

    // from 7 the shortest way through 4, 5, 2 with 3, and 8 - the order the map shows, not the one a planet
    // carried round the wrong way gave (4, 2, 3, 5, 8: 10379 Ls against 7959)
    std::vector<body_t const *> scans;
    for(body_t const & body: pp_n_bodies)
      scans.push_back(&body);
    std::array<::events::body_id_t, 5> const left{6, 8, 4, 5, 14};
    auto const route{visit_order(pp_n_barycentre, scans, left, ::events::body_id_t{12}, pp_n_now)};
    expect(fatal(route.size() == 5_ul));
    expect(route[0].body_id == 8_u and route[1].body_id == 6_u and route[4].body_id == 14_u);

    // "Bleia Eohn MI-Q d6-67", 7 to 11
    std::array<planet_t, 5> const mi_q{
      planet_t{13, 146601569652.55737, 0.000883, 0.00205, 331.932697, 32576473.95134, -125.968574, 335.63052, at(10, 29, 23, 16, 13), {}, 440, 542},
      planet_t{15, 253905421495.43762, 0.000716, 0.409494, 80.101006, 74251307.845116, -36.95675, 285.341856, at(10, 29, 23, 15, 55), {}, 610, 590},
      planet_t{16, 598765343427.6581, 0.003488, -0.58742, 85.170263, 268893975.019455, 111.575669, 212.264774, at(10, 29, 23, 16, 51), {}, 517, 170},
      planet_t{21, 818605434894.5618, 0.000751, 5.489558, 54.192866, 429841214.418411, -80.687968, 200.8619, at(10, 29, 23, 15, 58), {}, 727, 885},
      planet_t{29, 2732566297054.291, 0.128299, 156.992902, 87.0105, 2621513485.908508, -112.889093, 294.862089, at(10, 29, 23, 15, 49), {}, 1940, 714}
    };
    auto const mi_q_bodies{bodies_of(mi_q, "Bleia Eohn MI-Q d6-67", 2312512703035)};
    double const mi_q_spread{spread(mi_q, mi_q_bodies, {}, at(11, 11, 23, 19, 35), 506, 497)};
    expect(mi_q_spread < 3.0) << "MI-Q d6-67, degrees:" << mi_q_spread;
  };

  "single star at rest is the system's own origin"_test = []
  {
    auto star{make_star(0, "Bleia Eohn XB-K a63-2", 40528921907736, "L")};
    auto body{to_body(std::move(star))};
    body.scanned_at = epoch;
    std::vector<body_t const *> scans{&body};
    auto const positions{order_calculation({}, scans, epoch)};
    expect(fatal(positions.size() == 1_ul));
    expect(near(positions[0].x, 0.0));
    expect(near(positions[0].y, 0.0));
    expect(near(positions[0].z, 0.0));
  };

  "a planet round a star that does not itself orbit anything"_test = []
  {
    // "Bleia Eohn BD-I a64-0 1" from a real journal - Parents: [{"Star":0}], no barycentre involved
    auto planet{make_planet(
      1,
      "Bleia Eohn BD-I a64-0",
      5342402204192,
      323261988162.994385,
      0.000662,
      0.169211,
      253.249324,
      444792354.106903,
      -119.628940,
      205.990204,
      {::events::parent_t{.Star = 0}}
    )};
    auto body{to_body(std::move(planet))};
    body.scanned_at = epoch;
    std::vector<body_t const *> scans{&body};
    // the star itself (body_id 0) was never scanned in this test - same as an unscanned system root:
    // its own contribution is zero, so the planet's local position IS its absolute one
    auto const positions{order_calculation({}, scans, epoch)};
    expect(fatal(positions.size() == 1_ul));
    expect(std::abs(positions[0].x) > 1.0 or std::abs(positions[0].y) > 1.0);
  };

  "a close binary pair sits on opposite sides of its shared barycentre"_test = []
  {
    // stars B and C of "Bleia Eohn LQ-F b31-7" - same inclination/eccentricity/ascending node/mean
    // anomaly/period, periapsis 180 degrees apart, semi-major axes scaled by the mass ratio: the
    // signature of two real bodies sharing one barycentre
    auto star_b{make_star(
      3,
      "Bleia Eohn LQ-F b31-7",
      16060895865097,
      "L",
      33401755690.574646,
      0.268665,
      40.144267,
      86.013054,
      22511249.780655,
      -127.255853,
      315.109137,
      {::events::parent_t{.Null = 2}, ::events::parent_t{.Null = 0}}
    )};
    auto star_c{make_star(
      4,
      "Bleia Eohn LQ-F b31-7",
      16060895865097,
      "L",
      42616034746.170044,
      0.268665,
      40.144267,
      266.013049,
      22511249.780655,
      -127.255853,
      315.109137,
      {::events::parent_t{.Null = 2}, ::events::parent_t{.Null = 0}}
    )};
    auto body_b{to_body(std::move(star_b))};
    auto body_c{to_body(std::move(star_c))};
    body_b.scanned_at = epoch;
    body_c.scanned_at = epoch;
    // barycentre 2 held still at the root (0 is never scanned) so both stars' positions are relative to it
    std::vector<bary_centre_t> barycentres{};
    std::vector<body_t const *> scans{&body_b, &body_c};
    auto const positions{order_calculation(barycentres, scans, epoch)};
    expect(fatal(positions.size() == 2_ul));
    auto const & pb{positions[0]};
    auto const & pc{positions[1]};

    // opposite sides: B and C, the barycentre and the origin all sit on one line - the game's own
    // journal only carries about six significant digits, so a hair short of exactly collinear is
    // still the right answer; sin(angle between them) is what says how far off it really is
    double const cross_x{pb.y * pc.z - pb.z * pc.y};
    double const cross_y{pb.z * pc.x - pb.x * pc.z};
    double const cross_z{pb.x * pc.y - pb.y * pc.x};
    double const mag_b{std::sqrt(pb.x * pb.x + pb.y * pb.y + pb.z * pb.z)};
    double const mag_c{std::sqrt(pc.x * pc.x + pc.y * pc.y + pc.z * pc.z)};
    double const sin_angle{std::sqrt(cross_x * cross_x + cross_y * cross_y + cross_z * cross_z) / (mag_b * mag_c)};
    expect(sin_angle < 1e-4) << sin_angle;

    // and scaled by their own semi-major axes in the opposite direction (a_B / a_C, negated) - again
    // to the journal's own precision, not the machine's
    double const ratio{-33401755690.574646 / 42616034746.170044};
    expect(near(pb.x, pc.x * ratio, std::abs(pb.x) * 1e-4 + 1.0)) << pb.x << " vs " << pc.x * ratio;
    expect(near(pb.y, pc.y * ratio, std::abs(pb.y) * 1e-4 + 1.0)) << pb.y << " vs " << pc.y * ratio;
    expect(near(pb.z, pc.z * ratio, std::abs(pb.z) * 1e-4 + 1.0)) << pb.z << " vs " << pc.z * ratio;

    // neither sits at the origin - a real separation, not a degenerate one
    double const dist2{
      (pb.x - pc.x) * (pb.x - pc.x) + (pb.y - pc.y) * (pb.y - pc.y) + (pb.z - pc.z) * (pb.z - pc.z)
    };
    expect(dist2 > 1e18);
  };

  "a planet round the second star of a binary circles that star, not the barycentre"_test = []
  {
    auto star_b{make_star(
      3,
      "Bleia Eohn LQ-F b31-7",
      16060895865097,
      "L",
      33401755690.574646,
      0.268665,
      40.144267,
      86.013054,
      22511249.780655,
      -127.255853,
      315.109137,
      {::events::parent_t{.Null = 2}, ::events::parent_t{.Null = 0}}
    )};
    // the nearest parent is the star, the barycentres come after it - one of each kind kept loses that
    auto planet{make_planet(
      5,
      "Bleia Eohn LQ-F b31-7",
      16060895865097,
      1'000'000'000.0,
      0.01,
      0.0,
      10.0,
      1'000'000.0,
      0.0,
      90.0,
      {::events::parent_t{.Star = 3}, ::events::parent_t{.Null = 2}, ::events::parent_t{.Null = 0}}
    )};
    auto body_b{to_body(std::move(star_b))};
    auto body_p{to_body(std::move(planet))};
    expect(std::get<planet_details_t>(body_p.details).nearest_parent == parent_kind_star);
    body_b.scanned_at = epoch;
    body_p.scanned_at = epoch;
    std::vector<body_t const *> scans{&body_b, &body_p};
    auto const positions{order_calculation({}, scans, epoch)};
    expect(fatal(positions.size() == 2_ul));
    auto const & pb{positions[0]};
    auto const & pp{positions[1]};
    double const apart{std::sqrt(
      (pp.x - pb.x) * (pp.x - pb.x) + (pp.y - pb.y) * (pp.y - pb.y) + (pp.z - pb.z) * (pp.z - pb.z)
    )};
    expect(apart < 2e9) << "the planet is" << apart << "m from its own star";
  };

  "a nested barycentre's own offset from the root is not lost"_test = []
  {
    // the full trinary: star A orbits root barycentre 0 directly; B and C share the closer barycentre
    // 2, which itself orbits 0 - this is exactly the case the old, broken parent-bookkeeping dropped
    auto star_a{make_star(
      1,
      "Bleia Eohn LQ-F b31-7",
      16060895865097,
      "M",
      311623579263.687134,
      0.165806,
      70.164037,
      83.411722,
      464275825.023651,
      66.120381,
      283.472801,
      {::events::parent_t{.Null = 0}}
    )};
    auto star_c{make_star(
      4,
      "Bleia Eohn LQ-F b31-7",
      16060895865097,
      "L",
      42616034746.170044,
      0.268665,
      40.144267,
      266.013049,
      22511249.780655,
      -127.255853,
      315.109137,
      {::events::parent_t{.Null = 2}, ::events::parent_t{.Null = 0}}
    )};
    auto body_a{to_body(std::move(star_a))};
    auto body_c{to_body(std::move(star_c))};
    body_a.scanned_at = epoch;
    body_c.scanned_at = epoch;

    bary_centre_t const barycentre2{
      .body_id = 2,
      .semi_major_axis = 462720745801.925659,
      .eccentricity = 0.165806,
      .orbital_inclination = 70.164037,
      .periapsis = 263.411716,
      .orbital_period = 464275825.023651,
      .ascending_node = 66.120381,
      .mean_anomaly = 283.472801,
      .scanned_at = epoch
    };
    std::vector<bary_centre_t> barycentres{barycentre2};
    std::vector<body_t const *> scans{&body_a, &body_c};
    auto const positions{order_calculation(barycentres, scans, epoch)};
    expect(fatal(positions.size() == 2_ul));
    auto const & pa{positions[0]};
    auto const & pc{positions[1]};

    // star A only climbs to the unscanned root (0), which contributes nothing - its own local orbit
    // around the barycentre 462 million km out is a sizeable position, not near the origin
    double const a_dist2{pa.x * pa.x + pa.y * pa.y + pa.z * pa.z};
    expect(a_dist2 > 1e20) << a_dist2;

    // star C must carry BOTH its own orbit around barycentre 2 AND barycentre 2's own orbit around the
    // root - so it sits roughly semi_major_axis(C) + semi_major_axis(barycentre 2) from the origin at
    // most, and at least their difference, never just its own ~42.6 billion m local swing alone
    double const c_dist{std::sqrt(pc.x * pc.x + pc.y * pc.y + pc.z * pc.z)}, local_only{42616034746.170044};
    expect(c_dist > local_only * 2.0) << c_dist << " should dwarf the local-only (broken) " << local_only;
    };

  "a moon's orbit is turned by its parent's, so it stands where the game says"_test = []
  {
    // "Spoihee IT-F d12-1113" from Dunkan's journal of 14.11: 4 with 5 round barycentre 24, 7 with 8 round
    // barycentre 49, moons of 5, 6, 7 and 9. Each scan's DistanceFromArrivalLS is the game's own distance of
    // that body from the star; unturned, 9 f stood 189 Ls off it, 5 a 49, 7 e 28 the wrong way round from 7 f;
    // untipped by 6's axial tilt, 6 d 16 and 6 e 23
    struct scanned_t
      {
      ::events::body_id_t id;
      double sma, ecc, incl, periapsis, period, node, anomaly;
      std::vector<::events::parent_t> parents;
      double from_star_ls;
      };
    auto const system{"Spoihee IT-F d12-1113"};
    std::uint64_t const address{1};
    using p = ::events::parent_t;
    std::array<scanned_t, 12> const scanned{
      scanned_t{25, 9141125202.178955, 0.152064, 4.590093, 3.440926, 19244332.909584, -106.888113, 95.041041, {p{.Null = 24}, p{.Star = 0}}, 2140.016675},
      scanned_t{34, 16145188808.441162, 0.152064, 4.590093, 183.440918, 19244332.909584, -106.888113, 95.041146, {p{.Null = 24}, p{.Star = 0}}, 2073.737833},
      scanned_t{35, 902526170.015335, 6.2e-05, 0.084723, 167.039844, 215827.75712, 38.366323, 6.485029, {p{.Planet = 34}, p{.Null = 24}, p{.Star = 0}}, 2073.824599},
      scanned_t{59, 53130000829.696655, 0.087764, -1.871319, 296.033215, 81172223.687172, -39.614079, 28.895067, {p{.Null = 49}, p{.Star = 0}}, 4031.904003},
      scanned_t{50, 121401786.804199, 0.087764, -1.871319, 116.033221, 81172223.687172, -39.614079, 28.89511, {p{.Null = 49}, p{.Star = 0}}, 3898.345682},
      scanned_t{57, 5133154988.288879, 0.000277, -0.166626, 184.47034, 2432109.236717, -16.181152, 86.872377, {p{.Planet = 50}, p{.Null = 49}, p{.Star = 0}}, 3913.276574},
      scanned_t{58, 8843000173.568726, 0.010263, 0.335059, 184.98005, 5499282.717705, 85.149677, 316.151399, {p{.Planet = 50}, p{.Null = 49}, p{.Star = 0}}, 3872.177516},
      scanned_t{60, 1552852094173.4314, 0.035056, 0.044546, 114.519962, 844563317.298889, 114.085616, 14.076228, {p{.Star = 0}}, 5004.023652},
      scanned_t{39, 904078024625.7782, 0.008865, -0.254062, 95.272427, 375185394.287109, 61.101233, 141.139179, {p{.Star = 0}}, 3036.588422},
      scanned_t{44, 4729721009.731293, 0.003238, 0.091783, 77.385118, 1975884.079933, -142.160204, 45.907521, {p{.Planet = 39}, p{.Star = 0}}, 3034.275888},
      scanned_t{45, 7011571288.108826, 0.005562, -0.550114, 165.561402, 3566409.349442, 95.676846, 326.912852, {p{.Planet = 39}, p{.Star = 0}}, 3019.224456},
      scanned_t{67, 31294034123.420715, 0.007278, 11.251822, 111.599677, 43757728.934288, 118.393266, 214.599713, {p{.Planet = 60}, p{.Star = 0}}, 5095.033267}
    };
    std::array<bary_centre_t, 2> const barycentres{
      bary_centre_t{
        .body_id = 24,
        .semi_major_axis = 633771836757.6599,
        .eccentricity = 0.000876,
        .orbital_inclination = 2.362229,
        .periapsis = 351.852547,
        .orbital_period = 220209676.027298,
        .ascending_node = -131.231126,
        .mean_anomaly = 175.139171,
        .scanned_at = epoch
      },
      bary_centre_t{
        .body_id = 49,
        .semi_major_axis = 1168241739273.0713,
        .eccentricity = 0.000468,
        .orbital_inclination = -0.001246,
        .periapsis = 55.144693,
        .orbital_period = 551107257.604599,
        .ascending_node = -20.744241,
        .mean_anomaly = 174.133503,
        .scanned_at = epoch
      }
    };
    // the scans came within three minutes of one another - a fraction of a degree of the fastest moon here
    std::vector<body_t> bodies;
    for(scanned_t const & s: scanned)
      {
      auto scan{
        make_planet(s.id, system, address, s.sma, s.ecc, s.incl, s.periapsis, s.period, s.node, s.anomaly, s.parents)
      };
      if(s.id == 39)
        scan.AxialTilt = -2.409882;
      bodies.push_back(to_body(std::move(scan)));
      bodies.back().scanned_at = epoch;
      }
    std::vector<body_t const *> scans;
    for(body_t const & body: bodies)
      scans.push_back(&body);
    auto const positions{body_positions_now(barycentres, scans, epoch)};
    expect(fatal(positions.size() == scanned.size()));
    for(size_t ix{}; ix != scanned.size(); ++ix)
      {
      auto const & at{positions[ix]};
      double const from_star_ls{std::sqrt(at.x * at.x + at.y * at.y + at.z * at.z) / 299'792'458.0};
      expect(std::abs(from_star_ls - scanned[ix].from_star_ls) < 8.0)
        << "body" << scanned[ix].id << ":" << from_star_ls << "Ls, the game" << scanned[ix].from_star_ls;
      }

    // from the star 7 f comes before 7 e, as the game's navigation panel lists them
    std::array<::events::body_id_t, 2> const moons{57, 58};
    auto const route{visit_order(barycentres, scans, moons, std::nullopt, epoch)};
    expect(fatal(route.size() == 2_ul));
    expect(route[0].body_id == 58_u and route[1].body_id == 57_u);
  };
  }
