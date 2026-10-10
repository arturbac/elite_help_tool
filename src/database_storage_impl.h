#pragma once
///\brief what every database_storage_*.cc shares - the tables, their rows and the sqlite helpers
#include <eht_settings.h>
#include <databse_storage.h>
#include <elite_data.h>
#include <set>
#include <sqlite3.h>
#include <filesystem>
#include <json_glaze.h>
#include <events/carrier.h>
#include <events/station.h>
#include <events/system_info.h>
#include <star_system.h>
#include <map>
#include <tuple>
#include <spdlog/spdlog.h>
#include <simple_enum/enum_cast.hpp>
#include <simple_enum/std_format.hpp>

using events::body_id_t;

namespace sql_iface
  {

struct star_details_t
  {
  uint64_t oid;
  uint64_t ref_body_oid;

  std::string star_type;
  std::string luminosity;
  double stellar_mass;
  double absolute_magnitude;
  double surface_temperature;
  std::optional<double> rotation_period;
  uint32_t age_my;
  uint8_t sub_class;
  double ascending_node;
  double mean_anomaly;
  std::optional<events::body_id_t> parent_star;
  std::optional<events::body_id_t> parent_barycenter;
  uint8_t nearest_parent;
  };

[[nodiscard]]
inline auto to_db_fromat(uint64_t ref_body_oid, ::star_details_t const & v) noexcept -> sql_iface::star_details_t
  {
  return sql_iface::star_details_t{
    .ref_body_oid = ref_body_oid,
    .star_type = v.star_type,
    .luminosity = v.luminosity,
    .stellar_mass = v.stellar_mass,
    .absolute_magnitude = v.absolute_magnitude,
    .surface_temperature = v.surface_temperature,
    .rotation_period = v.rotation_period,
    .age_my = v.age_my,
    .sub_class = v.sub_class,
    .ascending_node = v.ascending_node,
    .mean_anomaly = v.mean_anomaly,
    .parent_star = v.parent_star,
    .parent_barycenter = v.parent_barycenter,
    .nearest_parent = v.nearest_parent
  };
  }

[[nodiscard]]
inline auto to_native_fromat(sql_iface::star_details_t const & v) noexcept -> ::star_details_t
  {
  return ::star_details_t{
    .star_type = v.star_type,
    .luminosity = v.luminosity,
    .stellar_mass = v.stellar_mass,
    .absolute_magnitude = v.absolute_magnitude,
    .surface_temperature = v.surface_temperature,
    .rotation_period = v.rotation_period,
    .age_my = v.age_my,
    .sub_class = v.sub_class,
    .ascending_node = v.ascending_node,
    .mean_anomaly = v.mean_anomaly,
    .parent_star = v.parent_star,
    .parent_barycenter = v.parent_barycenter,
    .nearest_parent = v.nearest_parent
  };
  }

struct atmosphere_element_t
  {
  uint64_t oid;
  uint64_t ref_body_oid;

  events::atmosphere_gas_type_e name;
  float percent;
  };

[[nodiscard]]
inline auto to_db_fromat(uint64_t ref_body_oid, events::atmosphere_element_t const & v) noexcept
  -> sql_iface::atmosphere_element_t
  {
  return sql_iface::atmosphere_element_t{.ref_body_oid = ref_body_oid, .name = v.Name, .percent = v.Percent};
  }

struct signal_t
  {
  uint64_t oid;
  uint64_t ref_body_oid;

  std::string type;
  uint16_t count;
  };

[[nodiscard]]
inline auto to_db_fromat(uint64_t ref_body_oid, events::signal_t const & v) noexcept -> sql_iface::signal_t
  {
  return sql_iface::signal_t{.ref_body_oid = ref_body_oid, .type = v.Type_Localised, .count = v.Count};
  }

[[nodiscard]]
inline auto to_native_fromat(sql_iface::signal_t const & v) noexcept -> events::signal_t
  {
  return events::signal_t{.Type_Localised = v.type, .Count = v.count};
  }

struct genus_t
  {
  uint64_t oid;
  uint64_t ref_body_oid;

  std::string genus;
  std::string species;
  };

[[nodiscard]]
inline auto to_db_fromat(uint64_t ref_body_oid, events::genus_t const & v) noexcept -> sql_iface::genus_t
  {
  return sql_iface::genus_t{.ref_body_oid = ref_body_oid, .genus = v.Genus_Localised, .species = v.Species_Localised};
  }

///\brief Sampled stays empty - a sample belongs to the character and comes from genus_progress
[[nodiscard]]
inline auto to_native_fromat(sql_iface::genus_t const & v) noexcept -> events::genus_t
  {
  return events::genus_t{.Genus_Localised = v.genus, .Species_Localised = v.species, .Sampled = {}};
  }

struct ring_t
  {
  uint64_t oid;
  uint64_t ref_system_address;

  std::string name;
  std::string ring_class;
  double mass_mt;
  double inner_rad;
  double outer_rad;
  uint32_t parent_body_id;
  int32_t body_id{-1};  // known after DSS
  };

[[nodiscard]]
inline auto to_db_fromat(uint64_t ref_system_address, ::ring_t const & v) noexcept -> sql_iface::ring_t
  {
  return sql_iface::ring_t{
    .ref_system_address = ref_system_address,
    .name = v.name,
    .ring_class = v.ring_class,
    .mass_mt = v.mass_mt,
    .inner_rad = v.inner_rad,
    .outer_rad = v.outer_rad,
    .parent_body_id = v.parent_body_id,
    .body_id = v.body_id
  };
  }

[[nodiscard]]
inline auto to_native_fromat(sql_iface::ring_t const & v) noexcept -> ::ring_t
  {
  return ::ring_t{
    .name = v.name,
    .ring_class = v.ring_class,
    .mass_mt = v.mass_mt,
    .inner_rad = v.inner_rad,
    .outer_rad = v.outer_rad,
    .parent_body_id = v.parent_body_id,
    .body_id = v.body_id
  };
  }

struct planet_details_t
  {
  uint64_t oid;
  uint64_t ref_body_oid;

  std::optional<events::body_id_t> parent_planet;
  std::optional<events::body_id_t> parent_star;
  std::optional<events::body_id_t> parent_barycenter;
  events::terraform_state_e terraform_state;
  std::string planet_class;
  std::string atmosphere;       // "thick argon rich atmosphere"
  std::string atmosphere_type;  // "ArgonRich"
  std::string volcanism;
  double mass_em;
  double surface_gravity;
  double surface_temperature;
  double surface_pressure;
  double ascending_node;
  double mean_anomaly;
  std::optional<double> rotation_period;
  std::optional<double> axial_tilt;
  bool landable;
  bool tidal_lock;
  bool was_mapped;
  bool was_footfalled;
  uint8_t nearest_parent;
  };

[[nodiscard]]
inline auto to_db_fromat(uint64_t ref_body_oid, ::planet_details_t const & v) noexcept -> sql_iface::planet_details_t
  {
  return sql_iface::planet_details_t{
    .ref_body_oid = ref_body_oid,
    .parent_planet = v.parent_planet,
    .parent_star = v.parent_star,
    .parent_barycenter = v.parent_barycenter,
    .terraform_state = v.terraform_state,
    .planet_class = v.planet_class,
    .atmosphere = v.atmosphere,
    .atmosphere_type = v.atmosphere_type,
    .volcanism = v.volcanism,
    .mass_em = v.mass_em,
    .surface_gravity = v.surface_gravity,
    .surface_temperature = v.surface_temperature,
    .surface_pressure = v.surface_pressure,
    .ascending_node = v.ascending_node,
    .mean_anomaly = v.mean_anomaly,
    .rotation_period = v.rotation_period,
    .axial_tilt = v.axial_tilt,
    .landable = v.landable,
    .tidal_lock = v.tidal_lock,
    .was_mapped = v.was_mapped,
    .was_footfalled = v.was_footfalled,
    .nearest_parent = v.nearest_parent
  };
  }

///\brief mapped i footfalled zostaja puste - to czyny postaci, przychodza z body_progress
[[nodiscard]]
inline auto to_native_fromat(sql_iface::planet_details_t const & v) noexcept -> ::planet_details_t
  {
  return ::planet_details_t{
    .parent_planet = v.parent_planet,
    .parent_star = v.parent_star,
    .parent_barycenter = v.parent_barycenter,
    .terraform_state = v.terraform_state,
    .planet_class = v.planet_class,
    .atmosphere = v.atmosphere,
    .atmosphere_type = v.atmosphere_type,
    .volcanism = v.volcanism,
    .mass_em = v.mass_em,
    .surface_gravity = v.surface_gravity,
    .surface_temperature = v.surface_temperature,
    .surface_pressure = v.surface_pressure,
    .ascending_node = v.ascending_node,
    .mean_anomaly = v.mean_anomaly,
    .rotation_period = v.rotation_period,
    .axial_tilt = v.axial_tilt,
    .landable = v.landable,
    .tidal_lock = v.tidal_lock,
    .was_mapped = v.was_mapped,
    .was_footfalled = v.was_footfalled,
    .mapped = {},
    .footfalled = {},
    .nearest_parent = v.nearest_parent
  };
  }

struct body_t
  {
  uint64_t oid;
  uint64_t ref_system_address;

  uint32_t value;
  body_id_t body_id;
  std::string name;
  double orbital_period;
  double orbital_inclination;
  double distance_from_arrival_ls;
  double semi_major_axis;
  double eccentricity;
  double periapsis;
  double radius;
  bool was_discovered;
  uint8_t details_type;
  std::chrono::sys_seconds scanned_at;
  };

[[nodiscard]]
inline auto to_db_fromat(uint64_t ref_system_address, ::body_t const & v) noexcept -> sql_iface::body_t
  {
  return sql_iface::body_t{
    .ref_system_address = ref_system_address,
    .value = v.value,
    .body_id = v.body_id,
    .name = v.name,
    .orbital_period = v.orbital_period,
    .orbital_inclination = v.orbital_inclination,
    .distance_from_arrival_ls = v.distance_from_arrival_ls,
    .semi_major_axis = v.semi_major_axis,
    .eccentricity = v.eccentricity,
    .periapsis = v.periapsis,
    .radius = v.radius,
    .was_discovered = v.was_discovered,
    .details_type = uint8_t(v.body_type()),
    .scanned_at = v.scanned_at
  };
  }

[[nodiscard]]
inline auto to_native_fromat(sql_iface::body_t && v) noexcept -> ::body_t
  {
  return ::body_t{
    .value = v.value,
    .body_id = v.body_id,
    .name = v.name,
    .orbital_period = v.orbital_period,
    .orbital_inclination = v.orbital_inclination,
    .distance_from_arrival_ls = v.distance_from_arrival_ls,
    .semi_major_axis = v.semi_major_axis,
    .eccentricity = v.eccentricity,
    .periapsis = v.periapsis,
    .radius = v.radius,
    .was_discovered = v.was_discovered,
    .scanned_at = v.scanned_at
  };
  }

struct bary_centre_t
  {
  uint64_t oid;
  uint64_t ref_system_address;

  events::body_id_t body_id;
  double semi_major_axis;
  double eccentricity;
  double orbital_inclination;
  double periapsis;
  double orbital_period;
  double ascending_node;
  double mean_anomaly;
  std::chrono::sys_seconds scanned_at;
  };

[[nodiscard]]
inline auto to_db_fromat(uint64_t ref_system_address, ::bary_centre_t const & bc) noexcept -> sql_iface::bary_centre_t
  {
  return sql_iface::bary_centre_t{
    .ref_system_address = ref_system_address,
    .body_id = bc.body_id,
    .semi_major_axis = bc.semi_major_axis,
    .eccentricity = bc.eccentricity,
    .orbital_inclination = bc.orbital_inclination,
    .periapsis = bc.periapsis,
    .orbital_period = bc.orbital_period,
    .ascending_node = bc.ascending_node,
    .mean_anomaly = bc.mean_anomaly,
    .scanned_at = bc.scanned_at
  };
  }

[[nodiscard]]
inline auto to_native_fromat(sql_iface::bary_centre_t const & bc) noexcept -> ::bary_centre_t
  {
  return ::bary_centre_t{
    .body_id = bc.body_id,
    .semi_major_axis = bc.semi_major_axis,
    .eccentricity = bc.eccentricity,
    .orbital_inclination = bc.orbital_inclination,
    .periapsis = bc.periapsis,
    .orbital_period = bc.orbital_period,
    .ascending_node = bc.ascending_node,
    .mean_anomaly = bc.mean_anomaly,
    .scanned_at = bc.scanned_at
  };
  }

struct star_system_t
  {
  uint64_t system_address;
  std::string name;
  std::string star_type;
  double loc_x;
  double loc_y;
  double loc_z;
  std::string economy;
  std::string second_economy;
  std::string government;
  std::string allegiance;
  std::string security;
  std::string controlling_faction;
  uint64_t population;
  uint32_t body_count;
  };

[[nodiscard]]
inline auto to_db_fromat(::star_system_t const & system) noexcept -> sql_iface::star_system_t
  {
  return sql_iface::star_system_t{
    .system_address = system.system_address,
    .name = system.name,
    .star_type = system.star_type,
    .loc_x = system.system_location[0],
    .loc_y = system.system_location[1],
    .loc_z = system.system_location[2],
    .economy = system.economy,
    .second_economy = system.second_economy,
    .government = system.government,
    .allegiance = system.allegiance,
    .security = system.security,
    .controlling_faction = system.controlling_faction,
    .population = system.population,
    .body_count = system.body_count
  };
  }

[[nodiscard]]
inline auto to_native_fromat(sql_iface::star_system_t && system) noexcept -> ::star_system_t
  {
  return ::star_system_t{
    .system_address = system.system_address,
    .name = std::move(system.name),
    .star_type = std::move(system.star_type),
    .system_location = std::array{system.loc_x, system.loc_y, system.loc_z},
    .fss_complete = {},
    .economy = std::move(system.economy),
    .second_economy = std::move(system.second_economy),
    .government = std::move(system.government),
    .allegiance = std::move(system.allegiance),
    .security = std::move(system.security),
    .controlling_faction = std::move(system.controlling_faction),
    .population = system.population,
    .body_count = system.body_count
  };
  }

///\brief a faction without reputation - that one is personal and sits in the main database
struct faction_info_t
  {
  int64_t oid{-1};
  std::string name;
  info::government_e government;
  info::allegiance_e allegiance;
  info::happiness_e happiness;
  };

[[nodiscard]]
inline auto to_db_fromat(info::faction_info_t const & v) noexcept -> sql_iface::faction_info_t
  {
  return sql_iface::faction_info_t{
    .oid = v.oid,
    .name = v.name,
    .government = v.government,
    .allegiance = v.allegiance,
    .happiness = v.happiness
  };
  }

///\brief the reputation stays at zero - a read from faction_reputation fills it in
[[nodiscard]]
inline auto to_native_fromat(sql_iface::faction_info_t && v) noexcept -> info::faction_info_t
  {
  return info::faction_info_t{
    .name = std::move(v.name),
    .oid = v.oid,
    .reputation = {},
    .government = v.government,
    .allegiance = v.allegiance,
    .happiness = v.happiness
  };
  }

[[nodiscard]]
inline auto to_db_fromat(events::fcmaterials_t const & fc) noexcept -> info::carrier_t
  {
  return info::carrier_t{
    .market_id = fc.MarketID,
    .carrier_name = fc.CarrierName,
    .carrier_id = fc.CarrierID
  };
  }

[[nodiscard]]
inline auto to_db_fromat(int64_t ref_fc, std::chrono::sys_seconds timestamp, events::fcmaterial_t const & fcm) noexcept -> info::fcmaterial_t
  {
  return info::fcmaterial_t{
    .carrier_id = ref_fc,
    .timestamp = timestamp.time_since_epoch().count(),
    .material_id= fcm.id,
    .price = fcm.Price,
    .stock = fcm.Stock,
    .demand = fcm.Demand
  };
  }
  
namespace tables
  {
  // facts about the galaxy - the same for every character, so two accounts can share them
  inline constexpr std::string_view star_system{"galaxy.star_system"};
  inline constexpr std::string_view bary_centre{"galaxy.bary_centre"};
  inline constexpr std::string_view star_details{"galaxy.star_details"};
  inline constexpr std::string_view atmosphere_element{"galaxy.atmosphere_element"};
  inline constexpr std::string_view signal{"galaxy.signal"};
  inline constexpr std::string_view genus{"galaxy.genus"};
  inline constexpr std::string_view ring{"galaxy.ring"};
  inline constexpr std::string_view body{"galaxy.body"};
  inline constexpr std::string_view planet_details{"galaxy.planet_details"};
  inline constexpr std::string_view faction_info{"galaxy.faction_info"};
  inline constexpr std::string_view faction_influence{"galaxy.faction_influence"};
  inline constexpr std::string_view faction_presence{"galaxy.faction_presence"};
  inline constexpr std::string_view system_conflict{"galaxy.system_conflict"};
  inline constexpr std::string_view system_signal{"galaxy.system_signal"};
  inline constexpr std::string_view station{"galaxy.station"};
  // who held a settlement when, and the kills on foot that tell a conflict zone's intensity - facts of the
  // galaxy, rebuilt from journals
  inline constexpr std::string_view settlement_owner{"galaxy.settlement_owner"};
  inline constexpr std::string_view ground_bond{"galaxy.ground_bond"};
  // colonisation: our claims, the construction sites in them and what they need - rebuilt from journals
  inline constexpr std::string_view colony_claim{"galaxy.colony_claim"};
  // carriers' jumps and positions - rebuilt from journals
  inline constexpr std::string_view carrier_movement{"galaxy.carrier_movement"};
  inline constexpr std::string_view construction_depot{"galaxy.construction_depot"};
  inline constexpr std::string_view construction_need{"galaxy.construction_need"};
  inline constexpr std::string_view construction_delivery{"galaxy.construction_delivery"};
  // the tick belongs to the game, not to a character - and rebuilds from journals with the rest of the galaxy
  inline constexpr std::string_view tick_observation{"galaxy.tick_observation"};
  // what cannot be rebuilt sits in a separate file attached as the live schema
  inline constexpr std::string_view market{"live.market"};
  inline constexpr std::string_view commodity{"live.commodity"};
  inline constexpr std::string_view market_item{"live.market_item"};
  // what THIS character did - it stays in the personal database, with natural keys so it survives a galaxy rebuild
  inline constexpr std::string_view db_owner{"db_owner"};
  inline constexpr std::string_view journal_progress{"journal_progress"};
  inline constexpr std::string_view system_progress{"system_progress"};
  inline constexpr std::string_view body_progress{"body_progress"};
  inline constexpr std::string_view genus_progress{"genus_progress"};
  inline constexpr std::string_view faction_reputation{"faction_reputation"};
  inline constexpr std::string_view mission{"mission"};
  inline constexpr std::string_view mission_cargo{"mission_cargo"};
  inline constexpr std::string_view mission_influence{"mission_influence"};
  // both rebuild from journals, so their place is in the personal database, not in live
  inline constexpr std::string_view ship_transfer{"ship_transfer"};
  inline constexpr std::string_view port_visit{"port_visit"};
  // the fleet as the last shipyard and the moves since told it - rebuilt from journals
  inline constexpr std::string_view ship{"ship"};
  inline constexpr std::string_view micro_resource{"live.micro_resource"};
  // selling micro resources is a journal event, so it is rebuildable
  inline constexpr std::string_view micro_sale{"micro_sale"};
  inline constexpr std::string_view micro_sale_item{"micro_sale_item"};
  inline constexpr std::string_view consumable_use{"consumable_use"};
  inline constexpr std::string_view foot_kill{"foot_kill"};
  inline constexpr std::string_view micro_acquisition{"micro_acquisition"};
  // a route plotted outside the game is in no journal at all, so a rebuild would wipe it
  inline constexpr std::string_view neutron_route{"live.neutron_route"};
  inline constexpr std::string_view neutron_progress{"live.neutron_progress"};
  // the commander's own choice, which no journal records - kept with what cannot be rebuilt
  inline constexpr std::string_view construction_abandoned{"live.construction_abandoned"};
  // a disconnect/crash found by scanning netLog and the journal - neither is rebuilt from EHT's own database,
  // so this stays in the live schema like the rest of what cannot be rebuilt
  inline constexpr std::string_view network_incident{"live.network_incident"};
  inline constexpr std::string_view incident_scan_progress{"live.incident_scan_progress"};
  // what is on our carriers - the game does not say for a squadron's, so it is kept here by hand and by
  // the balance of every docking; none of it can be rebuilt from journals
  inline constexpr std::string_view carrier_cargo{"live.carrier_cargo"};
  inline constexpr std::string_view carrier_cargo_change{"live.carrier_cargo_change"};
  inline constexpr std::string_view carrier{"live.carrier"};
  inline constexpr std::string_view carrier_materials{"live.carrier_materials"};
  }  // namespace tables
  };  // namespace sql_iface

using namespace std::string_view_literals;

namespace sqlite
  {
namespace details
  {
  template<typename T>
  struct is_optional_impl : std::false_type
    {
    };

  template<typename U>
  struct is_optional_impl<std::optional<U>> : std::true_type
    {
    };
  }  // namespace details

template<typename T>
concept is_optional = details::is_optional_impl<std::remove_cvref_t<T>>::value;

template<typename T>
constexpr auto reflection_type_name() -> std::string_view
  {
  if constexpr(is_optional<T>)
    return reflection_type_name<typename T::value_type>();
  else if constexpr(std::same_as<T, std::chrono::sys_seconds>)
    return "TEXT"sv;
  else if constexpr(simple_enum::bounded_enum<T>)
    return "TEXT"sv;
  else if constexpr(std::integral<T>)
    return "INTEGER"sv;
  else if constexpr(std::floating_point<T>)
    return "REAL"sv;
  else if constexpr(std::same_as<T, std::string> or std::same_as<T, std::string_view>)
    return "TEXT"sv;
  else
    static_assert(false);
  }

///\brief sqlite does not always set an error text, and formatting nullptr as {} ends in strlen(nullptr)
[[nodiscard]]
inline auto sql_error_text(char const * err_msg) noexcept -> char const *
  {
  return err_msg != nullptr ? err_msg : "no error text given";
  }

[[nodiscard]]
constexpr auto escape_sql_quotes(std::string_view const value) -> std::string
  {
  auto const extra_space = std::ranges::count(value, '\'');
  if(extra_space == 0) [[likely]]
    return std::string{value};

  std::string result;
  auto const max_size = value.size() + extra_space;
  result.resize_and_overwrite(
    max_size,
    [value](char * buf, std::size_t buf_size) noexcept -> std::size_t
    {
      auto * out = buf;
      for(char const c: value)
        if(c == '\'') [[unlikely]]
          {
          *out++ = '\'';
          *out++ = '\'';
          }
        else
          *out++ = c;
      return static_cast<std::size_t>(out - buf);
    }
  );

  return result;
  }

static_assert("''b''s''"sv == escape_sql_quotes("'b's'"));

template<typename T>
constexpr auto serialize(T const & value) -> std::string
  {
  if constexpr(is_optional<T>)
    if(not value)
      return std::string{"NULL"};
    else
      return serialize(*value);
  else if constexpr(std::same_as<T, std::chrono::sys_seconds>)
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", value);
  else if constexpr(simple_enum::bounded_enum<T>)
    return std::string(simple_enum::enum_name(value));
  else if constexpr(std::same_as<T, bool>)
    {
    return value ? "1" : "0";
    }
  else if constexpr(std::integral<T>)
    {
    std::array<char, 25> buffer{};  // Large enough for 64-bit integers
    auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);

    if(ec != std::errc{})
      return "0";  // Logical error: fallback for failed conversion
    return std::string(buffer.data(), ptr);
    }
  else if constexpr(std::floating_point<T>)
    {
    std::array<char, 64> buffer{};
    auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if(ec != std::errc{})
      return "0.0";
    return std::string(buffer.data(), ptr);
    }
  else if constexpr(std::same_as<T, std::string> or std::same_as<T, std::string_view>)
    {
    return escape_sql_quotes(value);
    }
  else
    static_assert(false);
  }

///\brief the column names on one line, for an error message
[[nodiscard]]
inline auto fmt_join(std::vector<std::string> const & values) -> std::string
  {
  std::string result;
  for(std::string const & value: values)
    {
    if(not result.empty())
      result.append(", ");
    result.append(value);
    }
  return result;
  }

inline int collect_column_names(void * d, int argc, char ** argv, char **)
  {
  // PRAGMA table_info returns descriptive columns; the name stands in the second position
  if(argc > 1 and argv[1] != nullptr)
    static_cast<std::vector<std::string> *>(d)->emplace_back(argv[1]);
  return 0;
  }

///\brief the column names of an existing table; it works for schema-prefixed names as well
[[nodiscard]]
inline auto table_columns(sqlite3 * db, std::string_view name) -> expected_ec<std::vector<std::string>>
  {
  std::string pragma;
  if(auto const dot{name.find('.')}; dot != std::string_view::npos)
    pragma = std::format("PRAGMA {}.table_info({});", name.substr(0, dot), name.substr(dot + 1));
  else
    pragma = std::format("PRAGMA table_info({});", name);

  std::vector<std::string> columns;
  if(sqlite3_exec(db, pragma.c_str(), &collect_column_names, &columns, nullptr) != SQLITE_OK) [[unlikely]]
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));

  return columns;
  }

///\brief checks whether an existing table has the shape the code expects
///\detail CREATE TABLE IF NOT EXISTS says nothing when the table exists in another shape, and the database
/// gathered live is never dropped - without this check every write would fail on its own during work
template<typename table_type>
inline auto verify_table(sqlite3 * db, std::string_view name) -> expected_ec<void>
  {
  std::string pragma;
  if(auto const dot{name.find('.')}; dot != std::string_view::npos)
    pragma = std::format("PRAGMA {}.table_info({});", name.substr(0, dot), name.substr(dot + 1));
  else
    pragma = std::format("PRAGMA table_info({});", name);

  std::vector<std::string> columns;
  if(sqlite3_exec(db, pragma.c_str(), &collect_column_names, &columns, nullptr) != SQLITE_OK) [[unlikely]]
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));

  uint32_t ix{};
  bool matches{columns.size() == glz::reflect<table_type>::keys.size()};
  glz::for_each_field(
    table_type{},
    [&columns, &ix, &matches]<typename T>(T &)
    {
      auto const key{glz::reflect<table_type>::keys[ix]};
      if(std::ranges::find(columns, key) == columns.end())
        matches = false;
      ++ix;
    }
  );

  if(matches)
    return {};

  spdlog::error("[sql] table {} has a different shape than the code expects - columns in file: {}", name,
                fmt_join(columns));
  return cxx23::unexpected(std::make_error_code(std::errc::bad_message));
  }

template<typename table_type>
inline auto create_table(sqlite3 * db, std::string_view const pk, std::string_view name) -> expected_ec<void>
  {
  std::string query{std::format("CREATE TABLE IF NOT EXISTS {} (", name)};

  uint32_t ix{};
  glz::for_each_field(
    table_type{},
    [&query, &ix, &pk]<typename T>(T &)
    {
      auto const key{glz::reflect<table_type>::keys[ix]};
      if(key != pk)
        query.append(std::format("{} {},", key, sqlite::reflection_type_name<T>()));
      else
        query.append(std::format("{} {} PRIMARY KEY,", pk, sqlite::reflection_type_name<T>()));
      ++ix;
    }
  );
  query.pop_back();  // drop ,
  query.append(");");

  char * err_msg = nullptr;
  int const rc = sqlite3_exec(db, query.c_str(), nullptr, nullptr, &err_msg);

  if(rc != SQLITE_OK)
    {
    spdlog::error("[sql] {} {}", query, sql_error_text(err_msg));
    sqlite3_free(err_msg);
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));
    }
  spdlog::debug("[sql] {}", query);
  return verify_table<table_type>(db, name);
  }

template<typename table_type, bool with_pk_store = false>
inline auto insert_into(sqlite3 * db, std::string_view const pk, std::string_view name, table_type const & record)
  -> expected_ec<void>
  {
  std::string query{std::format("INSERT INTO {} (", name)};
  // (name) VALUES ('{}');
  uint32_t ix{};
  glz::for_each_field(
    record,
    [&query, &ix, &pk]<typename T>(T &)
    {
      auto const key{glz::reflect<table_type>::keys[ix]};
      if constexpr(with_pk_store)
        query.append(std::format("{},", key));
      else if(key != pk)
        query.append(std::format("{},", key));
      ++ix;
    }
  );
  query.pop_back();  // drop ,
  query.append(") VALUES (");
  ix = 0;
  glz::for_each_field(
    record,
    [&query, &ix, &pk]<typename T>(T & value)
    {
      auto const key{glz::reflect<table_type>::keys[ix]};
      if constexpr(with_pk_store)
        query.append(std::format("'{}',", serialize(value)));
      else if(key != pk)
        query.append(std::format("'{}',", serialize(value)));
      ++ix;
    }
  );
  query.pop_back();  // drop ,
  query.append(");");

  char * err_msg = nullptr;
  int const rc = sqlite3_exec(db, query.c_str(), nullptr, nullptr, &err_msg);

  if(rc != SQLITE_OK)
    {
    spdlog::error("[sql] {} {}", query, sql_error_text(err_msg));
    sqlite3_free(err_msg);
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));
    }
  spdlog::debug("[sql] {}", query);
  return {};
  }

template<typename table_type, typename pk_type>
inline auto update_pk(
  sqlite3 * db, std::string_view const pk, std::string_view name, table_type const & record, pk_type const & pk_value
) -> expected_ec<void>
  {
  std::string query{std::format("UPDATE {} SET ", name)};
  uint32_t ix{};
  glz::for_each_field(
    record,
    [&query, &ix, &pk]<typename T>(T & value)
    {
      auto const key{glz::reflect<table_type>::keys[ix]};
      if(key != pk)
        query.append(std::format("{}='{}',", key, serialize(value)));
      ++ix;
    }
  );
  query.pop_back();  // drop ,
  query.append(std::format(" WHERE {}='{}'", pk, serialize(pk_value)));

  char * err_msg = nullptr;
  int const rc = sqlite3_exec(db, query.c_str(), nullptr, nullptr, &err_msg);

  if(rc != SQLITE_OK)
    {
    spdlog::error("[sql] {} {}", query, sql_error_text(err_msg));
    sqlite3_free(err_msg);
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));
    }
  spdlog::debug("[sql] {}", query);
  return {};
  }

template<typename T>
constexpr auto deserialize(std::string_view const value) -> T
  {
  if constexpr(is_optional<T>)
    if("NULL"sv == value)
      return T{};
    else
      return deserialize<typename T::value_type>(value);
  else if constexpr(std::same_as<T, std::chrono::sys_seconds>)
    {
    using namespace std::string_literals;
    std::chrono::sys_seconds tp;
    std::stringstream ss{std::string{value}};

    ss >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ"s, tp);

    if(ss.fail())
      return std::chrono::sys_seconds{std::chrono::seconds{0}};

    return tp;
    }
  else if constexpr(simple_enum::bounded_enum<T>)
    {
    auto res{simple_enum::enum_cast<T>(value)};
    if(res)
      return *res;
    return T{};
    }
  else if constexpr(std::same_as<T, bool>)
    {
    return value == "1"sv;
    }
  else if constexpr(std::integral<T>)
    {
    T result{};
    auto const [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if(ec != std::errc{}) [[unlikely]]
      return T{};
    return result;
    }
  else if constexpr(std::floating_point<T>)
    {
    T result{};
    auto const [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if(ec != std::errc{}) [[unlikely]]
      return T{};
    return result;
    }
  else if constexpr(std::same_as<T, std::string>)
    {
    return std::string(value);
    }
  else
    static_assert(false);
  }

template<typename table_type>
inline int select_callback(
  void * d,         /* Data provided in the 4th argument of sqlite3_exec() */
  int argc,         /* The number of columns in row */
  char ** argv,     /* An array of strings representing fields in the row */
  char ** azcolname /* An array of strings representing column names */
)
  {
  std::span<char *> fields{argv, size_t(argc)};
  std::span<char *> columns{azcolname, size_t(argc)};
  table_type record{};
  uint32_t ix = 0;
  glz::for_each_field(
    record,
    [&ix, &fields, &columns]<typename T>(T & value)
    {
      auto const key{glz::reflect<table_type>::keys[ix]};
      assert(key == std::string_view{columns[ix]});
      // a real SQL NULL comes as a null pointer - it is what a column added in place holds in the rows
      // written before it, and a string_view built from it reads memory that is not there
      value = fields[ix] != nullptr ? deserialize<T>(fields[ix]) : T{};
      ++ix;
    }
  );
  auto data{static_cast<std::vector<table_type> *>(d)};
  data->emplace_back(std::move(record));
  return 0;
  }

template<typename table_type>
inline auto select_from(sqlite3 * db, std::string_view name, std::string_view where_clause)
  -> cxx23::expected<std::vector<table_type>, std::error_code>
  {
  std::string query{std::format("SELECT ")};
  uint32_t ix{};
  glz::for_each_field(
    table_type{},
    [&query, &ix]<typename T>(T &)
    {
      auto const key{glz::reflect<table_type>::keys[ix]};
      query.append(std::format("{},", key));
      ++ix;
    }
  );
  query.pop_back();  // drop ,
  query.append(std::format(" FROM {} {}", name, where_clause));
  std::vector<table_type> result;

  char * err_msg = nullptr;
  int const rc = sqlite3_exec(db, query.c_str(), &select_callback<table_type>, &result, &err_msg);

  if(rc != SQLITE_OK)
    {
    spdlog::error("[sql] {} {}", query, sql_error_text(err_msg));
    sqlite3_free(err_msg);
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));
    }
  spdlog::debug("[sql] {}", query);
  return result;
  }

template<typename value_type>
inline int select_single_callback(
  void * d,     /* Data provided in the 4th argument of sqlite3_exec() */
  int argc,     /* The number of columns in row */
  char ** argv, /* An array of strings representing fields in the row */
  char **       /* An array of strings representing column names */
)
  {
  assert(argc == 1);
  std::span<char *> fields{argv, size_t(argc)};
  std::optional<value_type> & value{*static_cast<std::optional<value_type> *>(d)};

  // an aggregate over an empty set (max, min, sum) returns a row of NULL, which sqlite hands over as a
  // nullptr. a string_view built from nullptr is UB, so no value has to stay no value
  if(fields[0] == nullptr)
    {
    value.reset();
    return 0;
    }

  value = deserialize<value_type>(fields[0]);

  return 0;
  }

template<typename value_type>
inline auto select_signle_from(sqlite3 * db, std::string_view query)
  -> cxx23::expected<std::optional<value_type>, std::error_code>
  {
  std::optional<value_type> value{};
  char * err_msg = nullptr;
  int const rc = sqlite3_exec(db, query.data(), &select_single_callback<value_type>, &value, &err_msg);

  if(rc != SQLITE_OK)
    {
    spdlog::error("[sql] {} {}", query, sql_error_text(err_msg));
    sqlite3_free(err_msg);
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));
    }
  spdlog::debug("[sql] {}", query);
  return value;
  }

inline auto execute_query_no_result(sqlite3 * db, std::string_view query) -> expected_ec<void>
  {
  char * err_msg = nullptr;
  int const rc = sqlite3_exec(db, query.data(), nullptr, nullptr, &err_msg);

  if(rc != SQLITE_OK)
    {
    spdlog::error("[sql] {} {}", query, sql_error_text(err_msg));
    sqlite3_free(err_msg);
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));
    }
  spdlog::debug("[sql] {}", query);
  return {};
  }

///\brief an index on a table that may sit in an attached schema
///\detail in CREATE INDEX the schema stands with the index name, not with the table - giving
/// "galaxy.body" in both places is a syntax error, so the name is split apart here
[[nodiscard]]
inline auto create_index(
  sqlite3 * db, std::string_view table, std::string_view columns, std::string_view suffix = "key", bool unique = false
) -> expected_ec<void>
  {
  auto const dot{table.find('.')};
  std::string_view const schema{dot == std::string_view::npos ? std::string_view{} : table.substr(0, dot + 1)};
  std::string_view const bare{dot == std::string_view::npos ? table : table.substr(dot + 1)};

  return execute_query_no_result(
    db,
    std::format(
      "CREATE {}INDEX IF NOT EXISTS {}{}_{} ON {} ({});",
      unique ? "UNIQUE " : "",
      schema,
      bare,
      suffix,
      bare,
      columns
    )
  );
  }
  }  // namespace sqlite

struct sqlite3_handle_t
  {
  sqlite3 * db{};

  sqlite3_handle_t() noexcept = default;
  sqlite3_handle_t(sqlite3_handle_t &&) noexcept = delete;
  auto operator=(sqlite3_handle_t &&) noexcept -> sqlite3_handle_t & = delete;

  void close()
    {
    if(db)
      {
      sqlite3_close(db);
      db = nullptr;
      }
    }

  ~sqlite3_handle_t()
    {
    if(db)
      sqlite3_close(db);
    }
  };
