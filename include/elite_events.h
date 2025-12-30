#pragma once
#include <variant>
#include <string>
#include <simple_enum/glaze_json_enum_name.hpp>
#include <chrono>

namespace color_codes_t
  {
inline constexpr std::string_view reset = "\033[m";
inline constexpr std::string_view red = "\033[31m";
inline constexpr std::string_view green = "\033[32m";
inline constexpr std::string_view blue = "\033[34m";
inline constexpr std::string_view yellow = "\033[33m";
  };  // namespace color_codes_t

namespace events
  {

enum struct event_e
  {
  FSDTarget,
  FSDJump,
  StartJump,
  ReceiveText,
  FSSDiscoveryScan,
  FSSBodySignals,
  FSSAllBodiesFound,
  Scan,
  ScanBaryCentre,
  SAAScanComplete,
  SAASignalsFound,
  Cargo,
  Loadout,
  Missions,
  Location,
  LoadGame,
  Statistics,
  EngineerProgress,
  Reputation,
  Progress,
  Rank,
  Materials,
  Commander,
  Fileheader,
  Music,
  FuelScoop,
  Shutdown,
  ReservoirReplenished,
  ShipLocker,
  NavRoute,
  NavRouteClear
  };

consteval auto adl_enum_bounds(event_e)
  {
  using enum event_e;
  return simple_enum::adl_info{FSDTarget, NavRouteClear};
  }
enum struct scan_type_e
  {
  AutoScan,
  Detailed
  };

consteval auto adl_enum_bounds(scan_type_e)
  {
  using enum scan_type_e;
  return simple_enum::adl_info{AutoScan, Detailed};
  }

struct generic_event_t
  {
  // std::string timestamp;
  event_e event;
  std::optional<scan_type_e> ScanType;
  };

using utc_time_point_t = std::chrono::sys_time<std::chrono::milliseconds>;

[[nodiscard]]
auto parse_timestamp_t(std::string_view input) -> std::optional<utc_time_point_t>;

/*
{
  "timestamp": "2025-12-24T10:13:34Z",
  "event": "FSDTarget",
  "Name": "Pru Theia PE-G d11-4",
  "SystemAddress": 149376059619,
  "StarClass": "F",
  "RemainingJumpsInRoute": 17
}
 */
struct fsd_target_t
  {
  std::string Name;
  uint64_t SystemAddress;
  std::string StarClass;
  uint32_t RemainingJumpsInRoute;
  };

enum struct jump_type_e
  {
  Hyperspace,
  Supercruise
  };

consteval auto adl_enum_bounds(jump_type_e)
  {
  using enum jump_type_e;
  return simple_enum::adl_info{Hyperspace, Hyperspace};
  }

// {
//   "timestamp": "2025-12-24T10:06:29Z",
//   "event": "StartJump",
//   "JumpType": "Hyperspace",
//   "Taxi": false,
//   "StarSystem": "Pru Theia IV-I c24-0",
//   "SystemAddress": 95194386898,
//   "StarClass": "K"
// }

///\brief When written: at the start of a Hyperspace or Supercruise jump (start of countdown)
struct start_jump_t
  {
  jump_type_e JumpType;
  std::string StarSystem;
  uint64_t SystemAddress;
  std::string StarClass;
  };

/*

{
  "timestamp": "2025-12-24T10:13:46Z",
  "event": "FSDJump",
  "Taxi": false,
  "Multicrew": false,
  "StarSystem": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490,
  "StarPos": [
    6858.90625,
    537.96875,
    -1302.59375
  ],
  "SystemAllegiance": "",
  "SystemEconomy": "$economy_None;",
  "SystemEconomy_Localised": "None",
  "SystemSecondEconomy": "$economy_None;",
  "SystemSecondEconomy_Localised": "None",
  "SystemGovernment": "$government_None;",
  "SystemGovernment_Localised": "None",
  "SystemSecurity": "$GAlAXY_MAP_INFO_state_anarchy;",
  "SystemSecurity_Localised": "Anarchy",
  "Population": 0,
  "Body": "Pru Theia LV-I c24-0",
  "BodyID": 0,
  "BodyType": "Star",
  "JumpDist": 74.633,
  "FuelUsed": 4.967102,
  "FuelLevel": 11.401766
}

 */
using body_id_t = uint32_t;

///\brief When written: when jumping from one star system to another
///\detail Note, when following a multi-jump route, this will typically appear for the next star, during a jump, ie
/// after "StartJump" but before the "FSDJump"
struct fsd_jump_t
  {
  // utc_time_point_t timestamp;
  bool Taxi;
  bool Multicrew;
  std::string StarSystem;
  uint64_t SystemAddress;
  std::array<double, 3> StarPos;
  std::string SystemAllegiance;
  std::string SystemEconomy_Localised;
  std::string SystemSecondEconomy_Localised;
  std::string SystemGovernment_Localised;
  std::string SystemSecurity_Localised;
  uint64_t Population;
  std::string Body;
  body_id_t BodyID;
  std::string BodyType;
  double JumpDist;
  double FuelUsed;
  double FuelLevel;
  };

/*
{
  "timestamp": "2025-12-24T10:13:51Z",
  "event": "Scan",
  "ScanType": "AutoScan",
  "BodyName": "Pru Theia LV-I c24-0",
  "BodyID": 0,
  "StarSystem": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490,
  "DistanceFromArrivalLS": 0.000000,
  "StarType": "M",
  "Subclass": 0,
  "StellarMass": 0.460938,
  "Radius": 437807552.000000,
  "AbsoluteMagnitude": 7.889389,
  "Age_MY": 6622,
  "SurfaceTemperature": 3601.000000,
  "Luminosity": "Va",
  "RotationPeriod": 186821.514346,
  "AxialTilt": 0.000000,
  "WasDiscovered": false,
  "WasMapped": false,
  "WasFootfalled": false
}
*/
// { "timestamp":"2025-12-24T09:19:22Z", "event":"NavRoute" }
// {
//   "timestamp": "2020-04-27T08:02:52Z",
//   "event": "Route",
//   "Route": [
//     { "StarSystem": "i Bootis", "SystemAddress": 1281787693419, "StarPos": [-22.37500,34.84375,4.00000], "StarClass":
//     "G" }, { "StarSystem": "Acihaut", "SystemAddress": 11665802405289, "StarPos": [-18.50000,25.28125,-4.00000],
//     "StarClass": "M" }, { "StarSystem": "LHS 455", "SystemAddress": 3686969379179, "StarPos":
//     [-16.90625,10.21875,-3.43750], "StarClass": "DQ" }, { "StarSystem": "SPF-LF 1", "SystemAddress": 22661187052961,
//     "StarPos": [2.90625,6.31250,-9.56250], "StarClass": "M" }, { "StarSystem": "Luyten's Star", "SystemAddress":7
//     268024264097, "StarPos": [6.56250,2.34375,-10.25000], "StarClass": "M" }] }
//   ]
// }
///\brief When plotting a multi-star route, the file "NavRoute.json" is written in the same directory as the journal,
/// with a list of stars along that route
struct nav_route_t
  {
  };

struct nav_route_clear_t
  {
  };

/*
{
  "timestamp": "2025-12-24T10:13:57Z",
  "event": "FSSDiscoveryScan",
  "Progress": 0.157770,
  "BodyCount": 12,
  "NonBodyCount": 0,
  "SystemName": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490
}
 */
struct fss_discovery_scan_t
  {
  double Progress;
  uint32_t BodyCount;
  uint32_t NonBodyCount;
  std::string SystemName;
  uint64_t SystemAddress;
  };

/*
{
  "timestamp": "2025-12-24T20:44:10Z",
  "event": "Scan",
  "ScanType": "Detailed",
  "BodyName": "Pru Theia KH-L d8-3 A 2",
  "BodyID": 7,
  "Parents": [
    {
      "Star": 1
    },
    {
      "Null": 0
    }
  ],
  "StarSystem": "Pru Theia KH-L d8-3",
  "SystemAddress": 115100190923,
  "DistanceFromArrivalLS": 1076.367354,
  "TidalLock": false,
  "TerraformState": "Terraformable",
  "PlanetClass": "High metal content body",
  "Atmosphere": "thin sulfur dioxide atmosphere",
  "AtmosphereType": "SulphurDioxide",
  "AtmosphereComposition": [
    {
      "Name": "SulphurDioxide",
      "Percent": 100.000000
    }
  ],
  "Volcanism": "",
  "MassEM": 0.125184,
  "Radius": 3196670.250000,
  "SurfaceGravity": 4.882730,
  "SurfaceTemperature": 274.352539,
  "SurfacePressure": 340.478302,
  "Landable": true,
  "Materials": [
    {
      "Name": "iron",
      "Percent": 21.236635
    },
    {
      "Name": "nickel",
      "Percent": 16.062502
    },
    {
      "Name": "sulphur",
      "Percent": 15.123203
    },
    {
      "Name": "carbon",
      "Percent": 12.717049
    },
    {
      "Name": "chromium",
      "Percent": 9.550821
    },
    {
      "Name": "manganese",
      "Percent": 8.770514
    },
    {
      "Name": "phosphorus",
      "Percent": 8.141672
    },
    {
      "Name": "germanium",
      "Percent": 4.423352
    },
    {
      "Name": "niobium",
      "Percent": 1.451411
    },
    {
      "Name": "tin",
      "Percent": 1.377183
    },
    {
      "Name": "tellurium",
      "Percent": 1.145657
    }
  ],
  "Composition": {
    "Ice": 0.000000,
    "Rock": 0.668990,
    "Metal": 0.331010
  },
  "SemiMajorAxis": 321729689836.502075,
  "Eccentricity": 0.003082,
  "OrbitalInclination": 0.085047,
  "Periapsis": 290.671999,
  "OrbitalPeriod": 82118505.239487,
  "AscendingNode": -169.761241,
  "MeanAnomaly": 164.778660,
  "RotationPeriod": 134641.899559,
  "AxialTilt": -2.560178,
  "WasDiscovered": true,
  "WasMapped": false,
  "WasFootfalled": false
}
 */
struct parent_t
  {
  std::optional<uint32_t> Star;
  std::optional<uint32_t> Null;

  auto id() const noexcept -> uint32_t
    {
    if(Star)
      return *Star;
    if(Null)
      return *Null;
    return 0;
    }
  };

struct element_t
  {
  std::string Name;
  double Percent;
  };

struct scan_detailed_scan_t
  {
  std::string BodyName;
  body_id_t BodyID;
  std::vector<parent_t> Parents;
  std::string StarSystem;
  uint64_t SystemAddress;
  double DistanceFromArrivalLS;
  bool TidalLock;
  std::string TerraformState;
  std::string PlanetClass;     // "High metal content body"
  std::string Atmosphere;      // "thick argon rich atmosphere"
  std::string AtmosphereType;  // "ArgonRich"
  std::vector<element_t> AtmosphereComposition;
  std::string Volcanism;
  std::optional<std::string> StarType;  //: Stellar classification (for a star) – see §15.2
  std::optional<uint8_t> Subclass;      //: Star's heat classification 0..9
  std::optional<double> StellarMass;    //: mass as multiple of Sol's mass
  std::optional<std::string> Luminosity;
  double MassEM;
  double Radius;
  double AbsoluteMagnitude;
  double SurfaceGravity;
  double SurfaceTemperature;
  double SurfacePressure;
  bool Landable;
  // std::vector<element_t> Composition;
  double SemiMajorAxis;
  double Eccentricity;
  double OrbitalInclination;
  double Periapsis;
  double OrbitalPeriod;
  double AscendingNode;
  double MeanAnomaly;
  double RotationPeriod;
  double AxialTilt;
  bool WasDiscovered;
  bool WasMapped;
  bool WasFootfalled;
  // Assume timestamp of the scan is relevant, but for relative position
  // we use current time minus game epoch or reference timestamp.
  };

// {
//   "timestamp": "2025-12-24T09:09:22Z",
//   "event": "SAAScanComplete",
//   "BodyName": "Pru Theia IB-H c25-0 1",
//   "SystemAddress": 94925951450,
//   "BodyID": 1,
//   "ProbesUsed": 2,
//   "EfficiencyTarget": 4
// }

struct saa_scan_complete_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  uint16_t ProbesUsed;
  uint16_t EfficiencyTarget;
  };

// {
//   "timestamp": "2025-12-24T10:14:57Z",
//   "event": "ScanBaryCentre",
//   "StarSystem": "Pru Theia LV-I c24-0",
//   "SystemAddress": 95395713490,
//   "BodyID": 2,
//   "SemiMajorAxis": 239472025632.858276,
//   "Eccentricity": 0.000229,
//   "OrbitalInclination": -0.050906,
//   "Periapsis": 151.681238,
//   "OrbitalPeriod": 94125960.469246,
//   "AscendingNode": -125.950369,
//   "MeanAnomaly": 322.039855
// }
struct scan_bary_centre_t
  {
  std::string StarSystem;
  uint64_t SystemAddress;
  body_id_t BodyID;
  double SemiMajorAxis;
  double Eccentricity;
  double OrbitalInclination;
  double Periapsis;
  double OrbitalPeriod;
  double AscendingNode;
  double MeanAnomaly;
  };
// {
//   "timestamp": "2025-12-24T10:15:10Z",
//   "event": "FSSBodySignals",
//   "BodyName": "Pru Theia LV-I c24-0 4 a",
//   "BodyID": 6,
//   "SystemAddress": 95395713490,
//   "Signals": [
//     {
//       "Type": "$SAA_SignalType_Biological;",
//       "Type_Localised": "Biological",
//       "Count": 2
//     }
//   ],
// "Genuses": [
//   {
//     "Genus": "$Codex_Ent_Bacterial_Genus_Name;",
//     "Genus_Localised": "Bacterium"
//   },
//   {
//     "Genus": "$Codex_Ent_Stratum_Genus_Name;",
//     "Genus_Localised": "Stratum"
//   }
// ]
// }

enum struct signal_type_e
  {
  Biological,
  Geological,
  Human
  };

consteval auto adl_enum_bounds(signal_type_e)
  {
  using enum signal_type_e;
  return simple_enum::adl_info{Biological, Human};
  }

struct signal_t
  {
  signal_type_e Type_Localised;
  uint16_t Count;
  };

struct fss_body_signals_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  std::vector<signal_t> Signals;
  };

// {
//   "timestamp": "2025-12-24T10:15:10Z",
//   "event": "FSSAllBodiesFound",
//   "SystemName": "Pru Theia LV-I c24-0",
//   "SystemAddress": 95395713490,
//   "Count": 12
// }

struct fss_all_bodies_found_t
  {
  std::string SystemName;
  uint64_t SystemAddress;
  uint32_t Count;
  };

  }  // namespace events

enum struct planet_value_e
  {
  low,
  medium,
  high
  };

consteval auto adl_enum_bounds(planet_value_e)
  {
  using enum planet_value_e;
  return simple_enum::adl_info{low, high};
  }

struct sell_value_t
  {
  uint32_t discovery;
  uint32_t mapping;
  };

[[nodiscard]]
auto value_class(sell_value_t const sv) noexcept -> planet_value_e;

struct body_t
  {
  sell_value_t value;

  [[nodiscard]]
  auto value_class() const noexcept -> planet_value_e
    {
    return ::value_class(value);
    }

  std::string name;
  std::string planet_class;
  bool was_discovered;
  bool was_mapped;
  bool mapped;
  events::scan_detailed_scan_t scan;
  };

struct state_t
  {
  std::string system_name;
  std::string star_class;
  uint64_t system_address;
  bool fss_complete;
  std::vector<events::scan_bary_centre_t> scan_bary_centre;
  std::vector<body_t> bodies;
  };

struct discovery_state_t
  {
  state_t * state;
  auto simple_discovery(std::string_view input) const -> void;
  };

struct planet_value_info_t
  {
  std::string_view planet_class;
  uint32_t discovery_value;          // FSS (Base)
  uint32_t mapping_value;            // FSS + DSS + Efficiency (Base)
  double bonus_first_fss;            // Multiplier for being the first to scan
  double bonus_first_dss;            // Multiplier for being the first to map
  double terraform_multiplier{1.0};  // Nowe pole
  };

// avg
// static constexpr std::array<planet_value_info_t, 19> exploration_values{
//   {{"Metal rich body", 65'000, 250'000, 1.5, 3.45},
//    {"High metal content body", 30'000, 110'000, 1.5, 3.45, 5.4 },
//    {"Rocky body", 500, 1'500, 1.5, 3.45, 260.0},
//    {"Icy body", 500, 1'500, 1.5, 3.45},
//    {"Rocky ice body", 500, 1'800, 1.5, 3.45},
//    {"Earthlike body", 270'000, 1'100'000, 1.5, 3.45},
//    {"Water world", 140'000, 550'000, 1.5, 3.45, 2.1},
//    {"Ammonia world", 140'000, 580'000, 1.5, 3.45},
//    {"Water giant", 1'000, 4'000, 1.5, 3.45},
//    {"Water giant with life", 1'500, 6'000, 1.5, 3.45},
//    {"Gas giant with water based life", 3'000, 12'000, 1.5, 3.45},
//    {"Gas giant with ammonia based life", 1'500, 6'000, 1.5, 3.45},
//    {"Sudarsky class I gas giant", 2'500, 10'000, 1.5, 3.45},
//    {"Sudarsky class II gas giant", 28'000, 110'000, 1.5, 3.45},
//    {"Sudarsky class III gas giant", 1'000, 4'000, 1.5, 3.45},
//    {"Sudarsky class IV gas giant", 3'000, 12'000, 1.5, 3.45},
//    {"Sudarsky class V gas giant", 3'500, 14'000, 1.5, 3.45},
//    {"Helium rich gas giant", 3'000, 12'000, 1.5, 3.45},
//    {"Helium gas giant", 500, 2'000, 1.5, 3.45}}
// };
// for use with earth mass mult
static constexpr std::array<planet_value_info_t, 19> exploration_values{
  {{"Metal rich body", 21'790, 129'900, 1.5, 3.45},
   {"High metal content body", 9'693, 57'700, 1.5, 3.45, 5.4},
   {"Rocky body", 300, 1'500, 1.5, 3.45, 260.0},
   {"Icy body", 300, 1'500, 1.5, 3.45},
   {"Rocky ice body", 300, 1'800, 1.5, 3.45},
   {"Earthlike body", 64'831, 386'400, 1.5, 3.45},
   {"Water world", 24'831, 148'000, 1.5, 3.45, 2.1},  // Mnożnik terra ok. 2.1
   {"Ammonia world", 33'268, 198'300, 1.5, 3.45},
   {"Water giant", 1'000, 4'000, 1.5, 3.45},
   {"Water giant with life", 1'500, 6'000, 1.5, 3.45},
   {"Gas giant with water based life", 3'000, 12'000, 1.5, 3.45},
   {"Gas giant with ammonia based life", 1'500, 6'000, 1.5, 3.45},
   {"Sudarsky class I gas giant", 1'650, 9'800, 1.5, 3.45},
   {"Sudarsky class II gas giant", 9'650, 57'500, 1.5, 3.45},
   {"Sudarsky class III gas giant", 500, 3'000, 1.5, 3.45},
   {"Sudarsky class IV gas giant", 2'800, 16'000, 1.5, 3.45},
   {"Sudarsky class V gas giant", 3'100, 18'000, 1.5, 3.45},
   {"Helium rich gas giant", 3'000, 12'000, 1.5, 3.45},
   {"Helium gas giant", 500, 2'000, 1.5, 3.45}}
};

namespace exploration
  {
[[nodiscard]]
auto is_high_value_star(std::string_view star_class) noexcept -> bool;

[[nodiscard]]
auto extract_mass_code(std::string_view name) noexcept -> char;

[[nodiscard]]
auto system_approx_value(std::string_view star_class, std::string_view system_name) noexcept -> planet_value_e;

  }  // namespace exploration

/*
 * Pru Theia LV-I c24-0
 * Praea Theia DM-M d7-4
 *
 Star classification

 O, B, A, F, G, K, M, L, T, Y, Proto, Carbon, Wolf-Rayet, White Dwarf, and non sequence

 VII means that the star is a white dwarf.

VI means that the star is a subdwarf. These are kinda similar to IV stars because I see no difference between VI stars
and regular V stars which I’m about to get to.

V is the most common roman numeral that you will see because it just says that the star is a regular main-sequence star.
These make up pretty much all of the stars in the galaxy and you are most likely to find valuable data in these stars.
You will want to spend most of your time in these star systems but if you want to be adventurous and make noteworthy
discoveries, go ahead and spend some time visiting I, II, III, and VII stars.

IV means that the star is a sub giant, most sub giants are A and B type stars from what i have seen. After comparing the
mass and size of a B type sub giant to regular B type stars I didn't see much of a difference but go ahead and google
them if you wanna find out more about them.

III means that it's a giant. From what I have seen these are mostly M types (probably just because that is the most
common star type), but they can be B, A, F ,G, K, and of course M stars. There is an abundance of these in the core so I
suggest looking by and in the core for them. In my experience, if you go to the edge of the core and maybe 1000 light
years down, you can find huge clusters of giants, S type giants, white dwarfs, and black holes. You can spend days in
these clusters and not run out of cool stars to explore. I will say somewhere near colonia maybe 1000 light years down
in one of these clusters lies an S type giant with an ELW that I found but never cashed in because I deleted that save.

II stars are bright giants......

I stars are supergiant stars (a nice and close one is Antares), these can be split up into 3 groups.

The first one are Ib (example: A2 Ib) stars, these are less luminous super giants.

Iab stars are intermediate supergiants.

Ia means it's a luminous supergiant.

And then there are actually 0 stars or “la+” stars which are hypergiants. “0”/ “la+” are what they are called in real
life, but elite dangerous calls them “La0” stars. I’m going off what one CMDR told me about these stars because I have
never seen one in Elite Dangerous. This maybe because all this time I was looking for “la+” and not “la0” lol. These
tend to be A, B, and O type stars. Ridiculously large but unfortunately they tend to not have any planets.


 */
