#pragma once
#include <events/common.h>
#include <events/event_kind.h>
#include <simple_enum/simple_enum.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

///\brief journal events: scans of bodies and signals - FSS, DSS, barycentres, organics
namespace events
  {

struct fss_discovery_scan_t
  {
  double Progress;
  uint32_t BodyCount;
  uint32_t NonBodyCount;
  std::string SystemName;
  uint64_t SystemAddress;
  };

struct parent_t
  {
  std::optional<uint32_t> Planet;
  std::optional<uint32_t> Star;
  std::optional<uint32_t> Null;

  auto id() const noexcept -> uint32_t
    {
    if(Planet)
      return *Planet;
    if(Star)
      return *Star;
    if(Null)
      return *Null;
    return 0;
    }
  };

// Gases in AtmosphereComposition
enum struct atmosphere_gas_type_e : uint8_t
  {
  Water,
  Oxygen,
  CarbonDioxide,
  SulphurDioxide,
  Ammonia,
  Methane,
  Nitrogen,
  Hydrogen,
  Helium,
  Neon,
  Argon,
  Silicates,
  Iron
  };

consteval auto adl_enum_bounds(atmosphere_gas_type_e)
  {
  using enum atmosphere_gas_type_e;
  return simple_enum::adl_info{Water, Iron};
  }

struct atmosphere_element_t
  {
  atmosphere_gas_type_e Name;
  float Percent;
  };

struct ring_t
  {
  std::string Name;
  std::string RingClass;
  double MassMT;
  double InnerRad;
  double OuterRad;
  };

enum struct terraform_state_e : uint8_t
  {
  none,
  Terraformable,
  Terraforming,
  Terraformed
  };

consteval auto adl_enum_bounds(terraform_state_e)
  {
  using enum terraform_state_e;
  return simple_enum::adl_info{none, Terraformed};
  }

struct composition_t
  {
  float Ice;
  float Rock;
  float Metal;
  };

struct scan_detailed_scan_t
  {
  std::string BodyName;
  std::vector<ring_t> Rings;
  std::optional<double> RotationPeriod;
  std::optional<double> AxialTilt;
  double DistanceFromArrivalLS;
  double SemiMajorAxis;
  double Eccentricity;
  double OrbitalInclination;
  double Periapsis;
  double OrbitalPeriod;
  body_id_t BodyID;
  bool WasDiscovered;
  bool WasMapped;

  // star
  std::string StarSystem;
  std::string StarType;
  std::string Luminosity;
  uint64_t SystemAddress;
  double StellarMass;
  double Radius;
  double AbsoluteMagnitude;
  double SurfaceTemperature;
  uint32_t Age_MY;
  uint8_t Subclass;

  // planets
  std::vector<parent_t> Parents;
  std::string TerraformState;
  std::string PlanetClass;
  std::string Atmosphere;
  std::string AtmosphereType;
  std::vector<atmosphere_element_t> AtmosphereComposition;
  std::string Volcanism;
  composition_t Composition;
  double MassEM;
  double SurfaceGravity;
  double SurfacePressure;
  double AscendingNode;
  double MeanAnomaly;

  bool Landable;
  bool TidalLock;
  bool WasFootfalled;
  };

struct saa_scan_complete_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  uint16_t ProbesUsed;
  uint16_t EfficiencyTarget;
  };

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

struct signal_t
  {
  std::string Type_Localised;
  uint16_t Count;
  };

struct genus_t
  {
  std::string Genus_Localised;
  ///\brief filled in only after sampling; mapping alone gives just the genus
  std::string Species_Localised;
  ///\brief the sample is complete - the game ends the Log, Sample, Sample, Analyse sequence with that last one
  bool Sampled;
  };

///\brief pobranie probki organicznej, dopowiada gatunek do rodzaju znanego z mapowania
struct scan_organic_t
  {
  scan_type_e ScanType;
  std::string Genus_Localised;
  std::string Species_Localised;
  std::string Variant_Localised;
  ///\brief this commander has logged the species before - the codex entry is not new to them
  bool WasLogged;
  uint64_t SystemAddress;
  body_id_t Body;
  };

///\brief a signal found by an FSS scan - a station, an installation, a POI, a phenomenon
struct fss_signal_discovered_t
  {
  uint64_t SystemAddress;
  std::string SignalName;
  std::string SignalName_Localised;
  std::string SignalType;
  bool IsStation;
  ///\brief present only for USS signals, which expire after a few minutes
  std::optional<double> TimeRemaining;
  };

///\brief a find written into the codex - the one sign in the journal that a commander was at a phenomenon
///\detail a Lagrange cloud or a space life form goes in as Biology under "Geology and Anomalies", the same as
/// the geology on the ground - what tells them apart is the place, which a find in space does not have
struct codex_entry_t
  {
  uint64_t SystemAddress;
  std::string Name;
  std::string Name_Localised;
  std::string Category;
  std::string SubCategory;
  std::optional<double> Latitude;
  };

struct fss_body_signals_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  std::vector<signal_t> Signals;
  };

struct dss_body_signals_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  std::vector<signal_t> Signals;
  std::vector<genus_t> Genuses;
  };

// { "event":"SAASignalsFound", "BodyName":"Fedgau MY-G d11-4 1", "SystemAddress":149191313507, "BodyID":1,
// "Signals":[ { "Type":"$SAA_SignalType_Biological;", "Type_Localised":"Biological", "Count":1 } ],
// "Genuses":[ { "Genus":"$Codex_Ent_Bacterial_Genus_Name;", "Genus_Localised":"Bacterium" } ] }

struct fss_all_bodies_found_t
  {
  std::string SystemName;
  uint64_t SystemAddress;
  uint32_t Count;
  };

  }  // namespace events
