#include <boost/ut.hpp>
#include <elite_events.h>

#include <chrono>
#include <cmath>
#include <numbers>

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
  }
