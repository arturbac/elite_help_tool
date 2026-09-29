// #define SPDLOG_USE_STD_FORMAT
#include <eht_settings.h>
#include <databse_storage.h>
#include <set>
#include <sqlite3.h>
#include <filesystem>
#include <glaze/glaze.hpp>
#include <elite_events.h>
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
  };

[[nodiscard]]
auto to_db_fromat(uint64_t ref_body_oid, ::star_details_t const & v) noexcept -> sql_iface::star_details_t
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
    .parent_barycenter = v.parent_barycenter
  };
  }

[[nodiscard]]
auto to_native_fromat(sql_iface::star_details_t const & v) noexcept -> ::star_details_t
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
    .parent_barycenter = v.parent_barycenter
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
auto to_db_fromat(uint64_t ref_body_oid, events::atmosphere_element_t const & v) noexcept
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
auto to_db_fromat(uint64_t ref_body_oid, events::signal_t const & v) noexcept -> sql_iface::signal_t
  {
  return sql_iface::signal_t{.ref_body_oid = ref_body_oid, .type = v.Type_Localised, .count = v.Count};
  }

[[nodiscard]]
auto to_native_fromat(sql_iface::signal_t const & v) noexcept -> events::signal_t
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
auto to_db_fromat(uint64_t ref_body_oid, events::genus_t const & v) noexcept -> sql_iface::genus_t
  {
  return sql_iface::genus_t{.ref_body_oid = ref_body_oid, .genus = v.Genus_Localised, .species = v.Species_Localised};
  }

///\brief Sampled stays empty - a sample belongs to the character and comes from genus_progress
[[nodiscard]]
auto to_native_fromat(sql_iface::genus_t const & v) noexcept -> events::genus_t
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
auto to_db_fromat(uint64_t ref_system_address, ::ring_t const & v) noexcept -> sql_iface::ring_t
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
auto to_native_fromat(sql_iface::ring_t const & v) noexcept -> ::ring_t
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
  };

[[nodiscard]]
auto to_db_fromat(uint64_t ref_body_oid, ::planet_details_t const & v) noexcept -> sql_iface::planet_details_t
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
    .was_footfalled = v.was_footfalled
  };
  }

///\brief mapped i footfalled zostaja puste - to czyny postaci, przychodza z body_progress
[[nodiscard]]
auto to_native_fromat(sql_iface::planet_details_t const & v) noexcept -> ::planet_details_t
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
    .footfalled = {}
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
auto to_db_fromat(uint64_t ref_system_address, ::body_t const & v) noexcept -> sql_iface::body_t
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
auto to_native_fromat(sql_iface::body_t && v) noexcept -> ::body_t
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
auto to_db_fromat(uint64_t ref_system_address, ::bary_centre_t const & bc) noexcept -> sql_iface::bary_centre_t
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
auto to_native_fromat(sql_iface::bary_centre_t const & bc) noexcept -> ::bary_centre_t
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
auto to_db_fromat(::star_system_t const & system) noexcept -> sql_iface::star_system_t
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
auto to_native_fromat(sql_iface::star_system_t && system) noexcept -> ::star_system_t
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
auto to_db_fromat(info::faction_info_t const & v) noexcept -> sql_iface::faction_info_t
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
auto to_native_fromat(sql_iface::faction_info_t && v) noexcept -> info::faction_info_t
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
auto to_db_fromat(events::fcmaterials_t const & fc) noexcept -> info::carrier_t
  {
  return info::carrier_t{
    .market_id = fc.MarketID,
    .carrier_name = fc.CarrierName,
    .carrier_id = fc.CarrierID
  };
  }

[[nodiscard]]
auto to_db_fromat(int64_t ref_fc, std::chrono::sys_seconds timestamp, events::fcmaterial_t const & fcm) noexcept -> info::fcmaterial_t
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
  // the commander's own choice, which no journal records - kept with what cannot be rebuilt
  inline constexpr std::string_view construction_abandoned{"live.construction_abandoned"};
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
auto sql_error_text(char const * err_msg) noexcept -> char const *
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
static auto fmt_join(std::vector<std::string> const & values) -> std::string
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

static int collect_column_names(void * d, int argc, char ** argv, char **)
  {
  // PRAGMA table_info returns descriptive columns; the name stands in the second position
  if(argc > 1 and argv[1] != nullptr)
    static_cast<std::vector<std::string> *>(d)->emplace_back(argv[1]);
  return 0;
  }

///\brief the column names of an existing table; it works for schema-prefixed names as well
[[nodiscard]]
static auto table_columns(sqlite3 * db, std::string_view name) -> expected_ec<std::vector<std::string>>
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
static auto verify_table(sqlite3 * db, std::string_view name) -> expected_ec<void>
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
static auto create_table(sqlite3 * db, std::string_view const pk, std::string_view name) -> expected_ec<void>
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
static auto insert_into(sqlite3 * db, std::string_view const pk, std::string_view name, table_type const & record)
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
static auto update_pk(
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
static int select_callback(
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
static auto select_from(sqlite3 * db, std::string_view name, std::string_view where_clause)
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
static int select_single_callback(
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
static auto select_signle_from(sqlite3 * db, std::string_view query)
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

static auto execute_query_no_result(sqlite3 * db, std::string_view query) -> expected_ec<void>
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
static auto create_index(
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

database_storage_t::database_storage_t(std::string_view db_path) :
    db_path_{db_path},
    db_{std::make_unique<sqlite3_handle_t>()}
  {
  // the side files lie next to the main database and each lives its own life
  std::filesystem::path sibling{db_path};
  sibling.replace_filename("live.sqlite");
  live_db_path_ = sibling.string();
  sibling.replace_filename("galaxy.sqlite");
  galaxy_db_path_ = sibling.string();
  }

database_storage_t::~database_storage_t() { close(); }

auto database_storage_t::open(storage_mode_e mode) -> expected_ec<void>
  {
  int const rc = sqlite3_open(db_path_.c_str(), &db_->db);

  if(rc != SQLITE_OK)
    return cxx23::unexpected(std::make_error_code(std::errc::io_error));

  // reading from the gui and writing from the following thread are separate connections; we wait rather than take SQLITE_BUSY
  sqlite3_busy_timeout(db_->db, 3000);

  // data gathered live and knowledge of the galaxy in separate files - ATTACH creates them when they do not exist
  for(auto const & [schema, path]:
      {std::pair{"live"sv, std::cref(live_db_path_)}, std::pair{"galaxy"sv, std::cref(galaxy_db_path_)}})
    if(
      auto res{sqlite::execute_query_no_result(
        db_->db, std::format("ATTACH DATABASE '{}' AS {};", sqlite::escape_sql_quotes(path.get()), schema)
      )};
      not res
    ) [[unlikely]]
      return res;

  if(mode == storage_mode_e::bulk_import)
    {
    // every insert is a transaction of its own, and importing the whole of the logs makes hundreds of
    // thousands of them - without an fsync per row and with the journal in memory the import runs many
    // times faster. A crash ends in a damaged database, but the import builds it from scratch anyway.
    //
    // the schema prefix is no ornament: an unqualified journal_mode and synchronous reach EVERY attached
    // database, so they would take these safeguards off live.sqlite as well - and that file cannot be
    // rebuilt and is sometimes shared with a second, running instance. main and galaxy the import builds
    // from scratch, so the shortcuts belong there - and are necessary, because most rows go to galaxy;
    // leaving it with an fsync per row slows the whole import down more than tenfold.
    // temp_store belongs to the connection, not to a database, so it stays without a prefix
    for(std::string_view pragma:
        {"PRAGMA main.synchronous = OFF;"sv,
         "PRAGMA main.journal_mode = MEMORY;"sv,
         "PRAGMA galaxy.synchronous = OFF;"sv,
         "PRAGMA galaxy.journal_mode = MEMORY;"sv,
         "PRAGMA temp_store = MEMORY;"sv})
      if(auto res{sqlite::execute_query_no_result(db_->db, pragma)}; not res) [[unlikely]]
        return res;
    }
  else
    {
    // the gui, the tool windows and the overlay read from separate connections while the journal thread
    // writes. With a rollback journal such a reader waits for the writer and after busy_timeout gets
    // "database is locked"; under WAL it does not wait at all, reading the last consistent image beside
    // the write in progress
    for(std::string_view pragma:
        {"PRAGMA journal_mode = WAL;"sv, "PRAGMA live.journal_mode = WAL;"sv, "PRAGMA galaxy.journal_mode = WAL;"sv})
      if(auto res{sqlite::execute_query_no_result(db_->db, pragma)}; not res) [[unlikely]]
        return res;
    }

  // migration first, because create_database checks the shape of the tables and would refuse to open an old one
  if(auto res{migrate_live_schema()}; not res) [[unlikely]]
    return res;

  // every CREATE is IF NOT EXISTS, so an existing database gains the tables and indexes it lacks
  return create_database();
  }

auto database_storage_t::migrate_live_schema() -> expected_ec<void>
  {
  // a column added in place instead of rebuilding the whole database. for live.sqlite it is the only
  // way, because that file cannot be rebuilt from journals; for galaxy it is a courtesy - the tool starts
  // at once, and a rebuild will fill the column in hindsight whenever one comes along anyway
  struct addition_t
    {
    std::string_view table;
    std::string_view column;
    std::string_view type;
    };

  for(addition_t const & add:
      {addition_t{sql_iface::tables::market_item, "producer"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::market_item, "consumer"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::station, "controlling_faction"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::station, "dist_from_star_ls"sv, "REAL DEFAULT 0"sv},
       addition_t{sql_iface::tables::station, "body_id"sv, "INTEGER"sv},
       // where on that body the settlement stands, from ApproachSettlement - unknown for rows written
       // before it was kept, or where the approach came from orbit rather than on foot
       addition_t{sql_iface::tables::station, "latitude"sv, "REAL"sv},
       addition_t{sql_iface::tables::station, "longitude"sv, "REAL"sv},
       // when the scan behind a body's orbital elements was taken - rows written before it was kept
       // stay at the epoch, as distant in the past as a position "now" could ever be carried forward to
       addition_t{sql_iface::tables::body, "scanned_at"sv, "TEXT DEFAULT '1970-01-01T00:00:00Z'"sv},
       addition_t{sql_iface::tables::bary_centre, "scanned_at"sv, "TEXT DEFAULT '1970-01-01T00:00:00Z'"sv},
       // what a star orbits - stars written before it was kept stay NULL until a rebuild or a rescan
       addition_t{sql_iface::tables::star_details, "parent_star"sv, "INTEGER"sv},
       addition_t{sql_iface::tables::star_details, "parent_barycenter"sv, "INTEGER"sv},
       // a star's own phase on the orbit it is written above - rows written before it was kept stay 0,
       // as if the star sat still at the moment of every scan
       addition_t{sql_iface::tables::star_details, "ascending_node"sv, "REAL DEFAULT 0"sv},
       addition_t{sql_iface::tables::star_details, "mean_anomaly"sv, "REAL DEFAULT 0"sv},
       // what the discovery scan counted - systems honked before it was kept say 0, as if never honked
       addition_t{sql_iface::tables::star_system, "body_count"sv, "INTEGER DEFAULT 0"sv},
       // the carrier's state from CarrierStats - added in place, because live.sqlite is never created anew
       addition_t{sql_iface::tables::carrier, "carrier_type"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::carrier, "docking_access"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::carrier, "fuel_level"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "jump_range_curr"sv, "REAL DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "jump_range_max"sv, "REAL DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "total_capacity"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "free_space"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "cargo"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "balance"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "available_balance"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "stats_seen"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::commodity, "key"sv, "TEXT DEFAULT ''"sv},
       // ships flown before it was kept stay empty until a rebuild from journals
       addition_t{sql_iface::tables::ship, "flown"sv, "TEXT DEFAULT ''"sv},
       // missions handed in before the bars were kept say 0 until filled in from the journals
       addition_t{sql_iface::tables::mission_influence, "economy"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::mission_influence, "security"sv, "INTEGER DEFAULT 0"sv},
       // before purchases and barters were kept every transaction was a sale, and every item went away
       addition_t{sql_iface::tables::micro_sale, "kind"sv, "TEXT DEFAULT 'sold'"sv},
       addition_t{sql_iface::tables::micro_sale_item, "received"sv, "INTEGER DEFAULT 0"sv}})
    {
    auto known{sqlite::table_columns(db_->db, add.table)};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};

    // an empty list means the table is not there yet - it will be created in its final shape at once
    if(known->empty() or std::ranges::find(*known, add.column) != known->end())
      continue;

    spdlog::info("adding column {} to {}", add.column, add.table);
    if(
      auto res{sqlite::execute_query_no_result(
        db_->db, std::format("ALTER TABLE {} ADD COLUMN {} {}", add.table, add.column, add.type)
      )};
      not res
    ) [[unlikely]]
      return res;
    }

  return {};
  }

auto database_storage_t::create_database() -> expected_ec<void>
  {
  if(not db_->db)
    return cxx23::unexpected(std::make_error_code(std::errc::not_connected));

  if(auto res{
       sqlite::create_table<sql_iface::star_system_t>(db_->db, "system_address"sv, sql_iface::tables::star_system)
     };
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::bary_centre_t>(db_->db, "oid"sv, sql_iface::tables::bary_centre)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::body_t>(db_->db, "oid"sv, sql_iface::tables::body)}; not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::ring_t>(db_->db, "oid"sv, sql_iface::tables::ring)}; not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::planet_details_t>(db_->db, "oid"sv, sql_iface::tables::planet_details)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::signal_t>(db_->db, "oid"sv, sql_iface::tables::signal)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::genus_t>(db_->db, "oid"sv, sql_iface::tables::genus)}; not res)
    [[unlikely]]
    return res;

  if(auto res{
       sqlite::create_table<sql_iface::atmosphere_element_t>(db_->db, "oid"sv, sql_iface::tables::atmosphere_element)
     };
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::star_details_t>(db_->db, "oid"sv, sql_iface::tables::star_details)};
     not res) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<sql_iface::faction_info_t>(db_->db, "oid"sv, sql_iface::tables::faction_info)};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::faction_influence_t>(db_->db, "oid"sv, sql_iface::tables::faction_influence)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::conflict_t>(db_->db, "oid"sv, sql_iface::tables::system_conflict)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<system_signal_t>(db_->db, "oid"sv, sql_iface::tables::system_signal)}; not res)
    [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::tick_observation_t>(db_->db, "oid"sv, sql_iface::tables::tick_observation)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::station_t>(db_->db, "market_id"sv, sql_iface::tables::station)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::settlement_owner_t>(db_->db, "oid"sv, sql_iface::tables::settlement_owner)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::ground_bond_t>(db_->db, "oid"sv, sql_iface::tables::ground_bond)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::colony_claim_t>(db_->db, "system_address"sv, sql_iface::tables::colony_claim)};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_table<info::carrier_movement_t>(db_->db, "oid"sv, sql_iface::tables::carrier_movement)};
     not res) [[unlikely]]
    return res;
  if(auto res{
       sqlite::create_table<info::construction_depot_t>(db_->db, "market_id"sv, sql_iface::tables::construction_depot)
     };
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_table<info::construction_need_t>(db_->db, "oid"sv, sql_iface::tables::construction_need)};
     not res) [[unlikely]]
    return res;
  if(auto res{
       sqlite::create_table<info::construction_delivery_t>(db_->db, "oid"sv, sql_iface::tables::construction_delivery)
     };
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::market_info_t>(db_->db, "market_id"sv, sql_iface::tables::market)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::commodity_t>(db_->db, "id"sv, sql_iface::tables::commodity)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::market_item_t>(db_->db, "oid"sv, sql_iface::tables::market_item)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::mission_t>(db_->db, "mission_id"sv, sql_iface::tables::mission)}; not res)
    [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::mission_cargo_t>(db_->db, "mission_id"sv, sql_iface::tables::mission_cargo)};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{
      sqlite::create_table<info::mission_influence_t>(db_->db, "oid"sv, sql_iface::tables::mission_influence)
    };
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::ship_transfer_t>(db_->db, "oid"sv, sql_iface::tables::ship_transfer)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::port_visit_t>(db_->db, "market_id"sv, sql_iface::tables::port_visit)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::ship_t>(db_->db, "ship_id"sv, sql_iface::tables::ship)}; not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_resource_t>(db_->db, "name"sv, sql_iface::tables::micro_resource)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_sale_t>(db_->db, "oid"sv, sql_iface::tables::micro_sale)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_sale_item_t>(db_->db, "oid"sv, sql_iface::tables::micro_sale_item)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::consumable_use_t>(db_->db, "oid"sv, sql_iface::tables::consumable_use)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::foot_kill_t>(db_->db, "oid"sv, sql_iface::tables::foot_kill)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_acquisition_t>(db_->db, "oid"sv, sql_iface::tables::micro_acquisition)};
     not res) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::neutron_waypoint_t>(db_->db, "oid"sv, sql_iface::tables::neutron_route)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::construction_abandoned_t>(
       db_->db, "market_id"sv, sql_iface::tables::construction_abandoned
     )};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_table<info::carrier_cargo_t>(db_->db, "oid"sv, sql_iface::tables::carrier_cargo)};
     not res) [[unlikely]]
    return res;
  if(auto res{
       sqlite::create_table<info::carrier_cargo_change_t>(db_->db, "oid"sv, sql_iface::tables::carrier_cargo_change)
     };
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::carrier_t>(db_->db, "oid"sv, sql_iface::tables::carrier)}; not res)
    [[unlikely]]
    return res;
    
  if(auto res{sqlite::create_table<info::fcmaterial_t>(db_->db, "oid"sv, sql_iface::tables::carrier_materials)}; not res)
    [[unlikely]]
    return res;

  // looking a faction up by name and finding its last influence row happens at every system visited
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::faction_info, "name", "name")}; not res) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::faction_influence, "faction_oid, system_address, timestamp"
    )};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::faction_presence_t>(db_->db, "oid"sv, sql_iface::tables::faction_presence)};
    not res
  ) [[unlikely]]
    return res;

  // the key has to be unique, because recording presence upserts through ON CONFLICT
  if(
    auto res{
      sqlite::create_index(db_->db, sql_iface::tables::faction_presence, "faction_oid, system_address", "key", true)
    };
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::system_conflict, "system_address, faction1, faction2, timestamp"
    )};
    not res
  ) [[unlikely]]
    return res;

  // the same signal comes back with every fss scan, so each one meets a check first
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::system_signal, "system_address, name")}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::market_item, "market_id")}; not res) [[unlikely]]
    return res;
  // and by commodity, for where to get what the missions need
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::market_item, "commodity_id", "commodity")}; not res)
    [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::carrier_materials, "carrier_id, material_id, timestamp"
    )};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::journal_progress_t>(db_->db, "id"sv, sql_iface::tables::journal_progress)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::db_owner_t>(db_->db, "fid"sv, sql_iface::tables::db_owner)}; not res)
    [[unlikely]]
    return res;

  // this character's progress - apart from knowledge of the galaxy, so in the main database
  if(
    auto res{
      sqlite::create_table<info::system_progress_t>(db_->db, "system_address"sv, sql_iface::tables::system_progress)
    };
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::body_progress_t>(db_->db, "oid"sv, sql_iface::tables::body_progress)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::genus_progress_t>(db_->db, "oid"sv, sql_iface::tables::genus_progress)};
     not res) [[unlikely]]
    return res;

  if(
    auto res{
      sqlite::create_table<info::faction_reputation_t>(db_->db, "faction"sv, sql_iface::tables::faction_reputation)
    };
    not res
  ) [[unlikely]]
    return res;

  // progress upserts through ON CONFLICT, so the keys have to be unique
  if(
    auto res{sqlite::create_index(db_->db, sql_iface::tables::body_progress, "system_address, body_id", "key", true)};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::genus_progress, "system_address, body_id, genus", "key", true
    )};
    not res
  ) [[unlikely]]
    return res;

  // every find checks whether we know it already, and there are a hundred and some thousand of them
  if(
    auto res{sqlite::create_index(db_->db, sql_iface::tables::micro_acquisition, "timestamp, market_id, name")};
    not res
  ) [[unlikely]]
    return res;

  // a mission moves several factions at once, so there are many times more rows than missions. The same
  // key serves both to sift out repeats during a rebuild and to search by system
  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::mission_influence, "mission_id, faction, system_address", "key", true
    )};
    not res
  ) [[unlikely]]
    return res;

  // the same window falls once per system, and a rebuild repeats it from the beginning
  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::tick_observation, "kind, system_address, window_begin, window_end", "key", true
    )};
    not res
  ) [[unlikely]]
    return res;

  // the search for the last ticks goes by the end of the window
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::tick_observation, "kind, window_end", "recent")};
     not res) [[unlikely]]
    return res;

  // a settlement's owners are looked up one settlement at a time, and so are the kills made at it
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::settlement_owner, "market_id, first_seen", "place")};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::ground_bond, "market_id, timestamp", "place")};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::construction_need, "market_id", "site")}; not res)
    [[unlikely]]
    return res;

  // a system is read with its bodies and each body with its details, so every one of these is looked up by
  // its owner - without an index each look is a scan of the whole table, some twenty-five thousand rows,
  // a dozen times for every system read
  for(auto const & [table, column]:
      {std::pair{sql_iface::tables::body, "ref_system_address"sv},
       std::pair{sql_iface::tables::bary_centre, "ref_system_address"sv},
       std::pair{sql_iface::tables::ring, "ref_system_address"sv},
       std::pair{sql_iface::tables::planet_details, "ref_body_oid"sv},
       std::pair{sql_iface::tables::star_details, "ref_body_oid"sv},
       std::pair{sql_iface::tables::atmosphere_element, "ref_body_oid"sv},
       std::pair{sql_iface::tables::signal, "ref_body_oid"sv},
       std::pair{sql_iface::tables::genus, "ref_body_oid"sv},
       std::pair{sql_iface::tables::station, "system_address"sv}})
    if(auto res{sqlite::create_index(db_->db, table, column, "owner")}; not res) [[unlikely]]
      return res;

  // a system's influence and presence are asked for by the system, while the keys above lead with the
  // faction - the first reading of a system, asked once for every tick observation, walked the whole key
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::faction_influence, "system_address, timestamp", "system")};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::faction_presence, "system_address, last_seen", "system")};
     not res) [[unlikely]]
    return res;
  // the positions of the fleet's and the carriers' systems are looked up by name, a few times a second
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::star_system, "name", "name")}; not res) [[unlikely]]
    return res;

  return {};
  }

auto database_storage_t::store(info::mission_t const & value) -> expected_ec<void>
  {
  if(not db_->db)
    return cxx23::unexpected(std::make_error_code(std::errc::not_connected));

  return sqlite::insert_into<info::mission_t, true>(db_->db, "mission_id"sv, sql_iface::tables::mission, value);
  }

auto database_storage_t::load_missions() -> expected_ec<std::vector<info::mission_t>>
  {
  return sqlite::select_from<info::mission_t>(
    db_->db,
    sql_iface::tables::mission,
    // redirected means done and waiting to be handed in - past its deadline it is closed just as an
    // unhanded one is, because either it was lost or the handing-in never reached the journal
    std::format(
      " WHERE expiry > '{:%Y-%m-%dT%H:%M:%SZ}' AND (status='accepted' OR status='redirected')",
      std::chrono::system_clock::now()
    )
  );
  }

auto database_storage_t::mission_exists(uint64_t mission_id) ->expected_ec<bool>
{
  if( auto res{sqlite::select_signle_from<uint32_t>(db_->db,
    std::format("select count(*) from {} where mission_id={}",sql_iface::tables::mission,mission_id) )}; not res)
    return cxx23::unexpected{res.error()};
  else
    return *res != 0;
}
auto database_storage_t::change_mission_status(
  uint64_t mission_id, info::mission_status_e const status, std::chrono::sys_seconds when
) -> expected_ec<void>
  {
  std::string query{std::format(
    "UPDATE {} SET status='{}', closed='{:%Y-%m-%dT%H:%M:%SZ}' WHERE mission_id={}",
    sql_iface::tables::mission,
    status,
    when,
    mission_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(info::mission_cargo_t const & value) -> expected_ec<void>
  {
  // replaying the journal meets the same mission again, so there is to be one row only
  return sqlite::insert_into<info::mission_cargo_t, true>(
    db_->db, "mission_id"sv, sql_iface::tables::mission_cargo, value
  );
  }

auto database_storage_t::load_cargo_needs() -> expected_ec<std::vector<info::cargo_need_t>>
  {
  return sqlite::select_from<info::cargo_need_t>(
    db_->db,
    std::format(
      "(SELECT mc.commodity AS commodity, sum(mc.count) AS count"
      " FROM {0} mc JOIN {1} m ON m.mission_id = mc.mission_id"
      " WHERE m.status IN ('{2}','{3}') AND mc.commodity <> ''"
      " GROUP BY mc.commodity ORDER BY count DESC)",
      sql_iface::tables::mission_cargo,
      sql_iface::tables::mission,
      info::mission_status_e::accepted,
      info::mission_status_e::redirected
    ),
    ""
  );
  }

auto database_storage_t::load_producers() -> expected_ec<std::vector<info::supply_option_t>>
  {
  // the same as load_supply_options, but without the condition on stock - what matters is the bare fact
  // that the market trades in it, so as to tell a momentary emptiness from having no source at all
  return sqlite::select_from<info::supply_option_t>(
    db_->db,
    std::format(
      // the missions' needs worked out once and the markets reached from them - as a subquery in FROM the
      // planner walked every market item first and summed the needs again for each, seconds for an empty answer
      "(WITH n AS MATERIALIZED (SELECT mc.commodity AS commodity, sum(mc.count) AS needed"
      "       FROM {0} mc JOIN {1} m ON m.mission_id = mc.mission_id"
      "       WHERE m.status IN ('{2}','{3}') AND mc.commodity <> ''"
      "       GROUP BY mc.commodity)"
      " SELECT i.market_id AS market_id,"
      " coalesce(st.name,'') AS station,"
      " coalesce(st.station_type,'') AS station_type,"
      " coalesce(ss.name,'') AS system,"
      " c.name AS commodity,"
      " n.needed AS needed,"
      " i.stock AS stock,"
      " i.buy_price AS buy_price"
      " FROM n"
      " JOIN {4} c ON lower(c.name) = lower(n.commodity)"
      " JOIN {5} i ON i.commodity_id = c.id AND i.producer <> 0"
      " LEFT JOIN {6} st ON st.market_id = i.market_id"
      " LEFT JOIN {7} ss ON ss.system_address = st.system_address)",
      sql_iface::tables::mission_cargo,
      sql_iface::tables::mission,
      info::mission_status_e::accepted,
      info::mission_status_e::redirected,
      sql_iface::tables::commodity,
      sql_iface::tables::market_item,
      sql_iface::tables::station,
      sql_iface::tables::star_system
    ),
    ""
  );
  }

auto database_storage_t::load_supply_options() -> expected_ec<std::vector<info::supply_option_t>>
  {
  // the commodity dictionary and the stock sit in the live database, the missions in the main one - hence one query across both
  return sqlite::select_from<info::supply_option_t>(
    db_->db,
    std::format(
      // the missions' needs worked out once and the markets reached from them - as a subquery in FROM the
      // planner walked every market item first and summed the needs again for each, seconds for an empty answer
      "(WITH n AS MATERIALIZED (SELECT mc.commodity AS commodity, sum(mc.count) AS needed"
      "       FROM {0} mc JOIN {1} m ON m.mission_id = mc.mission_id"
      "       WHERE m.status IN ('{2}','{3}') AND mc.commodity <> ''"
      "       GROUP BY mc.commodity)"
      " SELECT i.market_id AS market_id,"
      " coalesce(st.name,'') AS station,"
      " coalesce(st.station_type,'') AS station_type,"
      " coalesce(ss.name,'') AS system,"
      " c.name AS commodity,"
      " n.needed AS needed,"
      " i.stock AS stock,"
      " i.buy_price AS buy_price"
      " FROM n"
      " JOIN {4} c ON lower(c.name) = lower(n.commodity)"
      " JOIN {5} i ON i.commodity_id = c.id AND i.stock >= n.needed AND i.buy_price > 0"
      " LEFT JOIN {6} st ON st.market_id = i.market_id"
      " LEFT JOIN {7} ss ON ss.system_address = st.system_address)",
      sql_iface::tables::mission_cargo,
      sql_iface::tables::mission,
      info::mission_status_e::accepted,
      info::mission_status_e::redirected,
      sql_iface::tables::commodity,
      sql_iface::tables::market_item,
      sql_iface::tables::station,
      sql_iface::tables::star_system
    ),
    ""
  );
  }

auto database_storage_t::store_faction_seen(int64_t faction_oid, uint64_t system_address, std::chrono::sys_seconds when)
  -> expected_ec<void>
  {
  // one row per faction and system, moved forward at every reading of that system
  std::string query{std::format(
    "INSERT INTO {0} (faction_oid, system_address, last_seen) VALUES ({1}, {2}, '{3:%Y-%m-%dT%H:%M:%SZ}')"
    " ON CONFLICT(faction_oid, system_address) DO UPDATE SET last_seen = excluded.last_seen",
    sql_iface::tables::faction_presence,
    faction_oid,
    system_address,
    when
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::load_present_factions(uint64_t system_address) -> expected_ec<std::vector<info::faction_ref_t>>
  {
  // present are the ones seen at the newest reading of this system
  return sqlite::select_from<info::faction_ref_t>(
    db_->db,
    std::format(
      "(SELECT faction_oid AS faction_oid FROM {0} WHERE system_address = {1}"
      " AND last_seen = (SELECT max(last_seen) FROM {0} WHERE system_address = {1}))",
      sql_iface::tables::faction_presence,
      system_address
    ),
    ""
  );
  }

auto database_storage_t::load_trade_options(uint64_t market_id, unsigned limit, bool bring_here, uint64_t in_system)
  -> expected_ec<std::vector<info::trade_option_t>>
  {
  // the "here" market is the one we stand in, "other" is any other one we have ever seen.
  // the direction decides nothing but which side buys and which sells
  std::string_view const buy_side{bring_here ? "other" : "here"};
  std::string_view const sell_side{bring_here ? "here" : "other"};
  // a rate on a single tonne is no rate - below this threshold the hint only litters the screen
  unsigned const minimum_quantity{eht::settings()->trade.minimum_quantity};
  // the game names only the system a route ends in, never the station, so the whole system is the narrowest
  std::string const system_filter{in_system != 0u ? std::format(" AND st.system_address = {}", in_system) : ""};

  return sqlite::select_from<info::trade_option_t>(
    db_->db,
    std::format(
      "(SELECT other.market_id AS market_id,"
      " coalesce(st.name,'') AS station,"
      " coalesce(ss.name,'') AS system,"
      " c.name AS commodity,"
      " {6}.buy_price AS buy_price,"
      " {7}.sell_price AS sell_price,"
      " {6}.stock AS stock,"
      " {7}.demand AS demand"
      " FROM {0} here"
      " JOIN {0} other ON other.commodity_id = here.commodity_id AND other.market_id <> here.market_id"
      " JOIN {1} c ON c.id = here.commodity_id"
      " LEFT JOIN {2} st ON st.market_id = other.market_id"
      " LEFT JOIN {3} ss ON ss.system_address = st.system_address"
      " WHERE here.market_id = {4}"
      "   AND {6}.stock >= {8} AND {6}.buy_price > 0"
      "   AND {7}.demand >= {8} AND {7}.sell_price > {6}.buy_price{9}"
      // the hold has a finite capacity, so what decides the earnings is the margin per tonne, not the percentage
      " ORDER BY ({7}.sell_price - {6}.buy_price) DESC"
      " LIMIT {5})",
      sql_iface::tables::market_item,
      sql_iface::tables::commodity,
      sql_iface::tables::station,
      sql_iface::tables::star_system,
      market_id,
      limit,
      buy_side,
      sell_side,
      minimum_quantity,
      system_filter
    ),
    ""
  );
  }

auto database_storage_t::reopen_mission(uint64_t mission_id, std::chrono::sys_seconds expiry) -> expected_ec<void>
  {
  // the Missions event is a snapshot from the moment the game started; replayed later it closes missions
  // taken after it, whereas MissionAccepted is the stronger witness - it says outright that at that
  // moment the mission was open
  std::string query{std::format(
    "UPDATE {} SET status='{}', closed='', expiry='{:%Y-%m-%dT%H:%M:%SZ}' WHERE mission_id={}",
    sql_iface::tables::mission,
    info::mission_status_e::accepted,
    expiry,
    mission_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::complete_mission(uint64_t mission_id, std::chrono::sys_seconds when, uint64_t reward)
  -> expected_ec<void>
  {
  // the sum in MissionAccepted is the offer; only MissionCompleted says what actually came in
  std::string query{std::format(
    "UPDATE {} SET status='{}', closed='{:%Y-%m-%dT%H:%M:%SZ}', reward={} WHERE mission_id={}",
    sql_iface::tables::mission,
    info::mission_status_e::completed,
    when,
    reward,
    mission_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::expire_missions_outside(std::span<uint64_t const> active, std::chrono::sys_seconds when)
  -> expected_ec<void>
  {
  std::string listed;
  for(uint64_t const mission_id: active)
    {
    if(not listed.empty())
      listed += ',';
    listed += std::to_string(mission_id);
    }

  // an empty list carries information too - it means the game has no open mission left
  std::string const exclusion{listed.empty() ? std::string{} : std::format(" AND mission_id NOT IN ({})", listed)};

  std::string query{std::format(
    "UPDATE {} SET status='{}', closed='{:%Y-%m-%dT%H:%M:%SZ}' WHERE status IN ('{}','{}'){}",
    sql_iface::tables::mission,
    info::mission_status_e::expired,
    when,
    info::mission_status_e::accepted,
    info::mission_status_e::redirected,
    exclusion
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::redirect_mission(
  uint64_t mission_id, std::string_view system, std::string_view station, std::string_view settlement
) -> expected_ec<void>
  {
  std::string query{std::format(
    "UPDATE {} SET status='{}', redirected_system='{}', redirected_station='{}', redirected_settlement='{}' WHERE "
    "mission_id={}",
    sql_iface::tables::mission,
    info::mission_status_e::redirected,
    sqlite::escape_sql_quotes(system),
    sqlite::escape_sql_quotes(station),
    sqlite::escape_sql_quotes(settlement),
    mission_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(star_system_t const & system) -> expected_ec<void>
  {
  if(not db_->db)
    return cxx23::unexpected(std::make_error_code(std::errc::not_connected));

  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::star_system, sql_iface::to_db_fromat(system))};
     not res) [[unlikely]]
    return res;

  if(system.fss_complete)
    if(auto res{store_fss_complete(system.system_address)}; not res) [[unlikely]]
      return res;
  // auto const star_system_oid{sqlite3_last_insert_rowid(db_->db)};
  for(bary_centre_t const & bc: system.bary_centre)
    if(auto res{store(system.system_address, bc)}; not res) [[unlikely]]
      return res;

  for(body_t const & b: system.bodies)
    if(auto res{store(system.system_address, b)}; not res) [[unlikely]]
      return cxx23::unexpected{res.error()};
  return {};
  }

auto database_storage_t::store_fss_complete(uint64_t system_address) -> expected_ec<void>
  {
  // a scan belongs to the character, not to the system - hence a table of its own in the main database
  std::string query{std::format(
    "INSERT INTO {0} (system_address, fss_complete) VALUES ({1}, 1)"
    " ON CONFLICT(system_address) DO UPDATE SET fss_complete = 1",
    sql_iface::tables::system_progress,
    system_address
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

namespace
  {
///\brief the query behind the history, over whichever names the tables have on the connection
[[nodiscard]]
auto species_history_source(
  std::string_view genus, std::string_view body, std::string_view planet_details, std::string_view star_details
) -> std::string
  {
  // the planet and the star both have a surface temperature, so the join is wrapped in a query of its own
  // that names each column once - the generator then reads it like a table
  return std::format(
    "(SELECT g.genus AS genus, g.species AS species, pd.planet_class AS planet_class,"
    " pd.atmosphere_type AS atmosphere_type, pd.volcanism AS volcanism,"
    " pd.surface_temperature AS surface_temperature, pd.surface_gravity AS surface_gravity,"
    " pd.surface_pressure AS surface_pressure, sd.star_type AS star_type,"
    " b.ref_system_address AS system_address, b.body_id AS body_id"
    " FROM {0} g JOIN {1} b ON b.oid = g.ref_body_oid JOIN {2} pd ON pd.ref_body_oid = b.oid"
    " LEFT JOIN {1} bs ON bs.ref_system_address = b.ref_system_address AND bs.body_id = pd.parent_star"
    " LEFT JOIN {3} sd ON sd.ref_body_oid = bs.oid"
    " WHERE g.species <> '') AS history",
    genus,
    body,
    planet_details,
    star_details
  );
  }
  }  // namespace

auto database_storage_t::load_species_history() -> expected_ec<std::vector<bio::species_record_t>>
  {
  return sqlite::select_from<bio::species_record_t>(
    db_->db,
    species_history_source(
      sql_iface::tables::genus,
      sql_iface::tables::body,
      sql_iface::tables::planet_details,
      sql_iface::tables::star_details
    ),
    ""
  );
  }

auto database_storage_t::load_species_history_from(std::string const & galaxy_path)
  -> expected_ec<std::vector<bio::species_record_t>>
  {
  sqlite3_handle_t other;
  if(sqlite3_open_v2(galaxy_path.c_str(), &other.db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
    spdlog::warn("the shared galaxy {} could not be opened", galaxy_path);
    return cxx23::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }
  // the other account may be writing it this very moment - a short wait instead of an error
  sqlite3_busy_timeout(other.db, 200);
  return sqlite::select_from<bio::species_record_t>(
    other.db, species_history_source("genus", "body", "planet_details", "star_details"), ""
  );
  }

auto database_storage_t::load_codex_finds() -> expected_ec<std::vector<bio::find_t>>
  {
  return sqlite::select_from<bio::find_t>(
    db_->db,
    std::format(
      "(SELECT g.genus AS genus, g.species AS species, s.name AS system_name, b.name AS body_name,"
      " b.ref_system_address AS system_address, b.body_id AS body_id, pd.planet_class AS planet_class,"
      " pd.atmosphere_type AS atmosphere_type, pd.volcanism AS volcanism,"
      " pd.surface_temperature AS surface_temperature, pd.surface_gravity AS surface_gravity,"
      " pd.surface_pressure AS surface_pressure, sd.star_type AS star_type,"
      " b.distance_from_arrival_ls AS distance_from_arrival_ls, s.loc_x AS loc_x, s.loc_y AS loc_y,"
      " s.loc_z AS loc_z, coalesce(gp.sampled, 0) AS sampled"
      " FROM {0} g JOIN {1} b ON b.oid = g.ref_body_oid JOIN {2} pd ON pd.ref_body_oid = b.oid"
      " JOIN {4} s ON s.system_address = b.ref_system_address"
      " LEFT JOIN {1} bs ON bs.ref_system_address = b.ref_system_address AND bs.body_id = pd.parent_star"
      " LEFT JOIN {3} sd ON sd.ref_body_oid = bs.oid"
      // the codex is this commander's own - a find is theirs when they logged it, whatever else the galaxy knows
      " JOIN {5} gp ON gp.system_address = b.ref_system_address AND gp.body_id = b.body_id"
      " AND gp.genus = g.genus"
      " WHERE g.species <> '') AS finds",
      sql_iface::tables::genus,
      sql_iface::tables::body,
      sql_iface::tables::planet_details,
      sql_iface::tables::star_details,
      sql_iface::tables::star_system,
      sql_iface::tables::genus_progress
    ),
    " ORDER BY genus, species, system_name, body_name"
  );
  }

auto database_storage_t::store_body_count(uint64_t system_address, uint32_t body_count) -> expected_ec<void>
  {
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "UPDATE {} SET body_count={} WHERE system_address={}", sql_iface::tables::star_system, body_count, system_address
    )
  );
  }

auto database_storage_t::store_system_location(uint64_t system_address, std::array<double, 3> const & loc)
  -> expected_ec<void>
  {
  std::string query{std::format(
    "UPDATE {} SET loc_x={}, loc_y={}, loc_z={} WHERE system_address={}",
    sql_iface::tables::star_system,
    loc[0],
    loc[1],
    loc[2],
    system_address
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(uint64_t system_address, bary_centre_t const & bc) -> expected_ec<void>
  {
  // a rescan repeats the barycentre with the mean anomaly of the moment - same orbit, one row
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db,
      std::format(
        "DELETE FROM {} WHERE ref_system_address={} AND body_id={}",
        sql_iface::tables::bary_centre,
        system_address,
        bc.body_id
      )
    )};
    not res
  ) [[unlikely]]
    return res;

  return sqlite::insert_into(
    db_->db, "oid"sv, sql_iface::tables::bary_centre, sql_iface::to_db_fromat(system_address, bc)
  );
  }

auto database_storage_t::store(uint64_t system_address, body_t const & value) -> expected_ec<uint64_t>
  {
  if(auto res{
       sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::body, sql_iface::to_db_fromat(system_address, value))
     };
     not res)
    return cxx23::unexpected{res.error()};

  auto const body_oid{sqlite3_last_insert_rowid(db_->db)};
  if(value.body_type() == body_type_e::planet)
    {
    planet_details_t const & pd{std::get<planet_details_t>(value.details)};
    if(auto res{
         sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::planet_details, sql_iface::to_db_fromat(body_oid, pd))
       };
       not res)
      return cxx23::unexpected{res.error()};

    // the state held in memory may already carry a mark of mapping - that belongs to the character, not to the body
    if(pd.mapped)
      if(auto res{store_dss_complete(system_address, value.body_id)}; not res)
        return cxx23::unexpected{res.error()};

    for(events::signal_t const & sig: pd.signals_)
      if(auto res{store(body_oid, sig)}; not res)
        return cxx23::unexpected{res.error()};

    for(events::genus_t const & sig: pd.genuses_)
      if(auto res{store(body_oid, sig)}; not res)
        return cxx23::unexpected{res.error()};

    for(events::atmosphere_element_t const & el: pd.atmosphere_composition)
      if(auto res{store(body_oid, el)}; not res)
        return cxx23::unexpected{res.error()};
    }
  else
    {
    if(auto res{sqlite::insert_into(
         db_->db,
         "oid"sv,
         sql_iface::tables::star_details,
         sql_iface::to_db_fromat(body_oid, std::get<star_details_t>(value.details))
       )};
       not res)
      return cxx23::unexpected{res.error()};
    }

  return body_oid;
  }

auto database_storage_t::store_dss_complete(uint64_t system_address, events::body_id_t body_id) -> expected_ec<void>
  {
  // the key is the system and the game's body number, not an oid - an oid changes with every galaxy rebuild
  std::string query{std::format(
    "INSERT INTO {0} (system_address, body_id, mapped, footfalled) VALUES ({1}, {2}, 1, 0)"
    " ON CONFLICT(system_address, body_id) DO UPDATE SET mapped = 1",
    sql_iface::tables::body_progress,
    system_address,
    body_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store_ring_body_id(
  uint64_t system_address, events::body_id_t parent_body_id, std::string_view ring_name, events::body_id_t ring_body_id
) -> expected_ec<void>
  {
  std::string query{
    std::format(
      "UPDATE {} SET body_id={}  WHERE ref_system_address={} AND parent_body_id={} AND name='{}'",
      sql_iface::tables::ring,
      ring_body_id,
      system_address,
      parent_body_id,
      sqlite::escape_sql_quotes(ring_name)
    )

  };
  spdlog::warn("{}", query);
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(uint64_t ref_body_oid, events::signal_t const & value) -> expected_ec<void>
  {
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::signal, sql_iface::to_db_fromat(ref_body_oid, value));
  }

auto database_storage_t::store(uint64_t ref_body_oid, events::genus_t const & value) -> expected_ec<void>
  {
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::genus, sql_iface::to_db_fromat(ref_body_oid, value));
  }

auto database_storage_t::store_genus_species(
  uint64_t system_address,
  events::body_id_t body_id,
  std::string_view genus,
  std::string_view species,
  bool personal,
  bool analysed
) -> expected_ec<void>
  {
  auto body_oid{oid_for_body(system_address, body_id)};
  if(not body_oid) [[unlikely]]
    return cxx23::unexpected{body_oid.error()};

  // a sample can arrive for a body we have not mapped yet
  if(not *body_oid)
    return {};

  // the species grows there whoever sampled it
  std::string query{std::format(
    "UPDATE {} SET species='{}' WHERE ref_body_oid={} AND genus='{}'",
    sql_iface::tables::genus,
    sqlite::escape_sql_quotes(species),
    **body_oid,
    sqlite::escape_sql_quotes(genus)
  )};
  if(auto res{sqlite::execute_query_no_result(db_->db, query)}; not res) [[unlikely]]
    return res;

  // landed without mapping first, the genus never reached the table - and without its row the species
  // would be lost to the history the next guess is made from
  if(not species.empty())
    {
    auto known{sqlite::select_signle_from<uint64_t>(
      db_->db,
      std::format(
        "SELECT count(*) FROM {} WHERE ref_body_oid={} AND genus='{}'",
        sql_iface::tables::genus,
        **body_oid,
        sqlite::escape_sql_quotes(genus)
      )
    )};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};
    if(not *known or **known == 0u)
      if(
        auto res{sqlite::execute_query_no_result(
          db_->db,
          std::format(
            "INSERT INTO {} (ref_body_oid, genus, species) VALUES ({}, '{}', '{}')",
            sql_iface::tables::genus,
            **body_oid,
            sqlite::escape_sql_quotes(genus),
            sqlite::escape_sql_quotes(species)
          )
        )};
        not res
      ) [[unlikely]]
        return res;
    }

  // the species grows there for everyone; that THIS commander logged it is theirs alone, and what makes
  // it a line of their codex rather than a fact learnt from another account's journals
  if(not personal)
    return {};

  // the sampled mark is never taken off - a further Log of the same genus does not undo the taking
  std::string progress{std::format(
    "INSERT INTO {0} (system_address, body_id, genus, sampled) VALUES ({1}, {2}, '{3}', {4})"
    " ON CONFLICT(system_address, body_id, genus) DO UPDATE SET sampled = max(sampled, excluded.sampled)",
    sql_iface::tables::genus_progress,
    system_address,
    body_id,
    sqlite::escape_sql_quotes(genus),
    analysed ? 1 : 0
  )};
  return sqlite::execute_query_no_result(db_->db, progress);
  }

auto database_storage_t::store(uint64_t system_address, ring_t const & value) -> expected_ec<void>
  {
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::ring, sql_iface::to_db_fromat(system_address, value));
  }

auto database_storage_t::oid_for_body(uint64_t system_address, events::body_id_t body_id)
  -> expected_ec<std::optional<uint64_t>>
  {
  std::string query{std::format(
    "SELECT oid from {} WHERE ref_system_address={} AND body_id='{}'", sql_iface::tables::body, system_address, body_id
  )};
  return sqlite::select_signle_from<uint64_t>(db_->db, query);
  }

auto database_storage_t::store(
  uint64_t system_address, events::body_id_t body_id, std::span<events::signal_t const> signals
) -> expected_ec<void>
  {
  auto resoid{oid_for_body(system_address, body_id)};
  if(not resoid)
    return cxx23::unexpected{resoid.error()};

  std::optional<uint64_t> boid_oid{*resoid};
  if(not boid_oid)
    return {};

  // The event carries the body's whole list, so it replaces what was there rather than adding to
  // it. Appending stacked a fresh copy every time a body was scanned again or a journal replayed
  // after the tool was restarted - one body in the archive had ended up with a hundred and forty
  // copies of the same signal, and the count of what is worth landing for grew with them
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db, std::format("DELETE FROM {} WHERE ref_body_oid={}", sql_iface::tables::signal, *boid_oid)
    )};
    not res
  ) [[unlikely]]
    return res;

  for(events::signal_t const & sig: signals)
    if(auto res{store(*boid_oid, sig)}; not res)
      return cxx23::unexpected{res.error()};

  return {};
  }

auto database_storage_t::store(
  uint64_t system_address, events::body_id_t body_id, std::span<events::genus_t const> genuses
) -> expected_ec<void>
  {
  auto resoid{oid_for_body(system_address, body_id)};
  if(not resoid)
    return cxx23::unexpected{resoid.error()};
  std::optional<uint64_t> boid_oid{*resoid};
  for(events::genus_t const & gen: genuses)
    if(auto res{store(*boid_oid, gen)}; not res)
      return cxx23::unexpected{res.error()};

  return {};
  }

auto database_storage_t::store(uint64_t system_address, std::span<ring_t const> rings) -> expected_ec<void>
  {
  for(ring_t const & ring: rings)
    if(auto res{store(system_address, ring)}; not res)
      return cxx23::unexpected{res.error()};
  return {};
  }

auto database_storage_t::store(uint64_t ref_body_oid, events::atmosphere_element_t const & value) -> expected_ec<void>
  {
  if(auto res{sqlite::insert_into(
       db_->db, "oid"sv, sql_iface::tables::atmosphere_element, sql_iface::to_db_fromat(ref_body_oid, value)
     )};
     not res)
    return cxx23::unexpected{res.error()};
  return {};
  }

[[nodiscard]]
auto database_storage_t::faction_oid(std::string_view name) -> expected_ec<std::optional<uint64_t>>
  {
  std::string query{
    std::format("SELECT oid FROM {} WHERE name='{}'", sql_iface::tables::faction_info, sqlite::escape_sql_quotes(name))
  };
  return sqlite::select_signle_from<uint64_t>(db_->db, query);
  }

[[nodiscard]]
auto database_storage_t::load_faction(std::string_view name) -> expected_ec<std::optional<info::faction_info_t>>
  {
  auto res{sqlite::select_from<sql_iface::faction_info_t>(
    db_->db, sql_iface::tables::faction_info, std::format(" WHERE name='{}'", sqlite::escape_sql_quotes(name))
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};
  if(not res->empty())
    {
    if(res->size() != 1) [[unlikely]]
      spdlog::error("multiple faction records for {}", name);

    info::faction_info_t faction{sql_iface::to_native_fromat(std::move(res->front()))};
    auto rep{sqlite::select_from<info::faction_reputation_t>(
      db_->db, sql_iface::tables::faction_reputation, std::format(" WHERE faction='{}'", sqlite::escape_sql_quotes(name))
    )};
    if(not rep) [[unlikely]]
      return cxx23::unexpected{rep.error()};
    if(not rep->empty())
      faction.reputation = rep->front().reputation;
    return faction;
    }
  return {};
  }

auto database_storage_t::store(info::faction_influence_t const & value) -> expected_ec<void>
  {
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::faction_influence, value);
  }

auto database_storage_t::last_influence(int64_t faction_oid, uint64_t system_address)
  -> expected_ec<std::optional<info::faction_influence_t>>
  {
  auto res{sqlite::select_from<info::faction_influence_t>(
    db_->db,
    sql_iface::tables::faction_influence,
    std::format(
      " WHERE faction_oid={} AND system_address={} ORDER BY timestamp DESC LIMIT 1", faction_oid, system_address
    )
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::faction_influence_t>{};

  return std::optional<info::faction_influence_t>{std::move((*res)[0])};
  }

auto database_storage_t::update_system_info(star_system_t const & system) -> expected_ec<void>
  {
  std::string query{std::format(
    "UPDATE {} SET economy='{}', second_economy='{}', government='{}', allegiance='{}', security='{}', "
    "controlling_faction='{}', population={} WHERE system_address={}",
    sql_iface::tables::star_system,
    sqlite::escape_sql_quotes(system.economy),
    sqlite::escape_sql_quotes(system.second_economy),
    sqlite::escape_sql_quotes(system.government),
    sqlite::escape_sql_quotes(system.allegiance),
    sqlite::escape_sql_quotes(system.security),
    sqlite::escape_sql_quotes(system.controlling_faction),
    system.population,
    system.system_address
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(info::station_t const & value) -> expected_ec<void>
  {
  auto known{load_station(value.market_id)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(not *known)
    return sqlite::insert_into<info::station_t, true>(db_->db, "market_id"sv, sql_iface::tables::station, value);

  // the sources describe a place differently - Docked knows the station type, ApproachSettlement the
  // settlement's economy. An empty field does not erase what we know, a non-empty one overwrites: a
  // finished construction changes the name and the journal is the only source of truth on what it is called now
  info::station_t merged{**known};
  auto const fill = [](std::string & target, std::string const & source)
  {
    if(not source.empty())
      target = source;
  };
  fill(merged.name, value.name);
  fill(merged.station_type, value.station_type);
  fill(merged.economy, value.economy);
  fill(merged.government, value.government);
  fill(merged.controlling_faction, value.controlling_faction);
  if(value.dist_from_star_ls > 0.0)
    merged.dist_from_star_ls = value.dist_from_star_ls;
  if(value.body_id)
    merged.body_id = value.body_id;
  if(value.latitude)
    merged.latitude = value.latitude;
  if(value.longitude)
    merged.longitude = value.longitude;
  if(merged.system_address == 0)
    merged.system_address = value.system_address;

  return sqlite::update_pk(db_->db, "market_id"sv, sql_iface::tables::station, merged, merged.market_id);
  }

auto database_storage_t::load_carriers() -> expected_ec<std::vector<info::carrier_t>>
  {
  // wlasny flotowiec na gorze, reszta to tlo z odwiedzin
  return sqlite::select_from<info::carrier_t>(
    db_->db, sql_iface::tables::carrier, " ORDER BY tracked DESC, carrier_name"
  );
  }

namespace
  {
  ///\brief the readings of a carrier's shelf joined with the dictionary - a reading keeps its time in seconds,
  /// the reading row takes it as the text every other time is kept in
  auto shelf_readings(std::string_view where) -> std::string
    {
    return std::format(
      "(SELECT r.name AS name, r.localised AS localised, r.category AS category, m.price AS price, m.stock AS stock,"
      " m.demand AS demand, strftime('%Y-%m-%dT%H:%M:%SZ', m.timestamp, 'unixepoch') AS timestamp"
      " FROM {} m JOIN {} c ON c.oid = m.carrier_id LEFT JOIN {} r ON r.id = m.material_id{})",
      sql_iface::tables::carrier_materials,
      sql_iface::tables::carrier,
      sql_iface::tables::micro_resource,
      where
    );
    }
  }  // namespace

auto database_storage_t::load_carrier_stock(std::string_view carrier_id)
  -> expected_ec<std::vector<info::carrier_stock_t>>
  {
  // what counts is the last reading, the earlier ones are the history of sales
  return sqlite::select_from<info::carrier_stock_t>(
    db_->db,
    shelf_readings(std::format(
      " WHERE c.carrier_id = '{}' AND m.timestamp = (SELECT max(timestamp) FROM {} WHERE carrier_id = c.oid)"
      " ORDER BY r.category, r.localised",
      sqlite::escape_sql_quotes(carrier_id),
      sql_iface::tables::carrier_materials
    )),
    ""
  );
  }

auto database_storage_t::load_port_sale_rows() -> expected_ec<std::vector<bar::port_sale_row_t>>
  {
  // a carrier's bar pays what its owner set, so only the ports' sales say what a kind is worth
  return sqlite::select_from<bar::port_sale_row_t>(
    db_->db,
    std::format(
      "(SELECT s.oid AS sale_oid, s.price AS price, i.name AS name, i.count AS count"
      " FROM {0} s JOIN {1} i ON i.sale_oid = s.oid"
      " WHERE s.kind = 'sold' AND s.market_id NOT IN (SELECT market_id FROM {2})"
      " AND s.market_id NOT IN (SELECT market_id FROM {3} WHERE station_type = 'FleetCarrier'))",
      sql_iface::tables::micro_sale,
      sql_iface::tables::micro_sale_item,
      sql_iface::tables::carrier,
      sql_iface::tables::station
    ),
    ""
  );
  }

auto database_storage_t::load_micro_resource_names() -> expected_ec<std::map<std::string, std::string>>
  {
  auto rows{sqlite::select_from<info::micro_resource_t>(db_->db, sql_iface::tables::micro_resource, "")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::map<std::string, std::string> names;
  for(info::micro_resource_t & row: *rows)
    names.emplace(std::move(row.name), std::move(row.localised));
  return names;
  }

auto database_storage_t::load_mission_rewards(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<bar::mission_reward_row_t>>
  {
  // a reward is written in the same event that completes the mission, so the second of it is the link
  return sqlite::select_from<bar::mission_reward_row_t>(
    db_->db,
    std::format(
      "(SELECT m.mission_id AS mission_id, m.type AS type, coalesce(a.name, '') AS name,"
      " coalesce(r.category, '') AS category, coalesce(a.count, 0) AS count"
      " FROM {0} m LEFT JOIN {1} a ON a.timestamp = m.closed AND a.source = 'mission_reward'"
      " LEFT JOIN {3} r ON r.name = a.name"
      " WHERE m.status = 'completed' AND m.closed >= '{2:%Y-%m-%dT%H:%M:%SZ}')",
      sql_iface::tables::mission,
      sql_iface::tables::micro_acquisition,
      since,
      sql_iface::tables::micro_resource
    ),
    ""
  );
  }

auto database_storage_t::load_carrier_history(std::string_view carrier_id, std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::carrier_stock_t>>
  {
  // the reading before the period is taken too - without it the first fall in the period has nothing to fall from
  return sqlite::select_from<info::carrier_stock_t>(
    db_->db,
    shelf_readings(std::format(
      " WHERE c.carrier_id = '{0}' AND m.timestamp >= coalesce((SELECT max(timestamp) FROM {1}"
      " WHERE carrier_id = c.oid AND timestamp < {2}), 0)"
      " ORDER BY r.name, m.timestamp",
      sqlite::escape_sql_quotes(carrier_id),
      sql_iface::tables::carrier_materials,
      since.time_since_epoch().count()
    )),
    ""
  );
  }

auto database_storage_t::load_mission_stats(std::chrono::sys_seconds since, uint64_t system_address)
  -> expected_ec<std::vector<info::mission_stat_t>>
  {
  // narrowing to a system goes through the station the mission was taken at,
  // and each source has its own station alias, so the condition is built separately for each
  auto const scope_for{
    [system_address](std::string_view alias) -> std::string
    {
      if(system_address == 0)
        return {};
      return std::format(" AND {}.system_address = {}", alias, system_address);
    }
  };
  std::string const scope{scope_for("st")};
  std::string const inner_scope{scope_for("bs")};

  return sqlite::select_from<info::mission_stat_t>(
    db_->db,
    std::format(
      "(SELECT m.faction AS faction, count(*) AS missions, sum(m.reward) AS rewards,"
      " (SELECT b.type FROM {0} b LEFT JOIN {1} bs ON bs.market_id = b.market_id"
      "  WHERE b.faction = m.faction AND b.status = 'completed' AND b.closed >= '{2:%Y-%m-%dT%H:%M:%SZ}'{4}"
      "  GROUP BY b.type ORDER BY count(*) DESC LIMIT 1) AS top_type"
      " FROM {0} m LEFT JOIN {1} st ON st.market_id = m.market_id"
      " WHERE m.status = 'completed' AND m.closed >= '{2:%Y-%m-%dT%H:%M:%SZ}'{3}"
      " GROUP BY m.faction ORDER BY missions DESC)",
      sql_iface::tables::mission,
      sql_iface::tables::station,
      since,
      scope,
      inner_scope
    ),
    ""
  );
  }

auto database_storage_t::load_acquisition_summary(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::acquisition_summary_t>>
  {
  // a place's economy comes from the station, so single finds gain it in hindsight.
  // it is counted once, in a single pass over the history - the same per row cost a second
  // "Mostly from" follows the chosen period - otherwise it would speak of a place from half a year ago
  std::string const window{std::format(" WHERE b.timestamp >= '{:%Y-%m-%dT%H:%M:%SZ}'", since)};

  return sqlite::select_from<info::acquisition_summary_t>(
    db_->db,
    std::format(
      "(SELECT a.name AS name, r.localised AS localised, r.category AS category,"
      " sum(CASE WHEN a.source = 'collected' THEN a.count ELSE 0 END) AS collected,"
      " sum(CASE WHEN a.source = 'mission_reward' THEN a.count ELSE 0 END) AS from_missions,"
      " coalesce(e.economy, '?') AS top_economy,"
      " max(a.timestamp) AS last_seen"
      " FROM {0} a"
      " LEFT JOIN {1} r ON r.name = a.name"
      " LEFT JOIN (SELECT name, economy FROM"
      "   (SELECT b.name AS name, coalesce(nullif(st.economy, ''), '?') AS economy,"
      "           row_number() OVER (PARTITION BY b.name ORDER BY sum(b.count) DESC) AS pick"
      "    FROM {0} b LEFT JOIN {2} st ON st.market_id = b.market_id{3}"
      "    GROUP BY b.name, st.economy)"
      "   WHERE pick = 1) e ON e.name = a.name"
      " WHERE a.timestamp >= '{4:%Y-%m-%dT%H:%M:%SZ}'"
      " GROUP BY a.name ORDER BY collected + from_missions DESC)",
      sql_iface::tables::micro_acquisition,
      sql_iface::tables::micro_resource,
      sql_iface::tables::station,
      window,
      since
    ),
    ""
  );
  }

auto database_storage_t::store(info::micro_acquisition_t const & value) -> expected_ec<void>
  {
  // replaying the journal repeats the finds; time, place and material tell them apart
  std::string known_query{std::format(
    "SELECT count(*) FROM {} WHERE timestamp='{:%Y-%m-%dT%H:%M:%SZ}' AND market_id={} AND name='{}'",
    sql_iface::tables::micro_acquisition,
    value.timestamp,
    value.market_id,
    sqlite::escape_sql_quotes(value.name)
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, known_query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::micro_acquisition, value);
  }

auto database_storage_t::store(info::mission_influence_t const & value) -> expected_ec<void>
  {
  // a rebuild from journals repeats every mission - mission, faction and system tell them apart
  std::string known_query{std::format(
    "SELECT count(*) FROM {} WHERE mission_id={} AND faction='{}' AND system_address={}",
    sql_iface::tables::mission_influence,
    value.mission_id,
    sqlite::escape_sql_quotes(value.faction),
    value.system_address
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, known_query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::mission_influence, value);
  }

auto database_storage_t::last_system_seen(uint64_t system_address)
  -> expected_ec<std::optional<std::chrono::sys_seconds>>
  {
  // a reading of a system covers every faction present at once, so the newest row of any of them says
  // when we last looked at this system
  // no aggregate - for a system never visited there is to be no row at all, not a row of NULL
  auto res{sqlite::select_signle_from<std::chrono::sys_seconds>(
    db_->db,
    std::format(
      "SELECT last_seen FROM {} WHERE system_address={} ORDER BY last_seen DESC LIMIT 1",
      sql_iface::tables::faction_presence,
      system_address
    )
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  return *res;
  }

auto database_storage_t::store(info::tick_observation_t const & value) -> expected_ec<void>
  {
  std::string known_query{std::format(
    "SELECT count(*) FROM {} WHERE kind='{}' AND system_address={}"
    " AND window_begin='{:%Y-%m-%dT%H:%M:%SZ}' AND window_end='{:%Y-%m-%dT%H:%M:%SZ}'",
    sql_iface::tables::tick_observation,
    simple_enum::enum_name(value.kind),
    value.system_address,
    value.window_begin,
    value.window_end
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, known_query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::tick_observation, value);
  }

namespace
  {
///\brief the longest gap between changes that still passes for the same recalculation wave
///
/// The galaxy recalculates system by system and the spread between them reaches hours, so a wave needs
/// room. On the other hand the next one usually comes after a day, over a weekend after two, so a
/// threshold around half a day separates them reliably
[[nodiscard]]
auto same_wave_gap() -> std::chrono::hours { return std::chrono::hours{eht::settings()->ticks.same_wave_gap_h}; }

///\brief by this much the start of a wave is moved back against what we saw
///
/// A reading with the old value does not prove the server has not recalculated - it proves only that it
/// has not reached us yet. The data shows it: windows sometimes start exactly on the hour or the half
/// hour, which is where the tick most likely really fell. When handing missions in, an error in this
/// direction is the safe one - better to take the day as having closed earlier than to hand in too late
[[nodiscard]]
auto client_lag() -> std::chrono::minutes { return std::chrono::minutes{eht::settings()->ticks.client_lag_min}; }

///\brief the nearest Thursday 07:00 UTC after the given moment, that is the game's weekly recalculation
///
/// %w counts days from Sunday, so Thursday is 4. When it is Thursday already but past the hour, the right
/// one is next week's
/// the modulo is written with a single per cent sign - in std::format it is not special, so doubling it
/// would reach SQL verbatim and break the query
constexpr std::string_view next_weekly_tick{
  "strftime('%Y-%m-%dT%H:%M:%SZ', datetime(date({0}, '+' || ("
  "  CASE WHEN ((4 - CAST(strftime('%w', {0}) AS INTEGER) + 7) % 7) = 0 AND time({0}) >= '07:00:00'"
  "       THEN 7 ELSE ((4 - CAST(strftime('%w', {0}) AS INTEGER) + 7) % 7) END"
  ") || ' days'), '+7 hours'))"
};

///\brief the time after colonisation in which influence runs to a rhythm of its own
///
/// A freshly colonised system has its influence set from above, and until the first **weekly**
/// recalculation - Thursday 07:00 UTC, that is 09:00 local time - it either stands still or jumps by a
/// fraction of a point on the main faction. Neither of the two is a trace of the daily tick
inline auto settled_colony_clause() -> std::string
  {
  std::string const first_seen{
    "( SELECT min(fi.timestamp) FROM galaxy.faction_influence fi"
    "  WHERE fi.system_address = tick_observation.system_address )"
  };

  return std::format(
    " AND ({0} IS NULL OR tick_observation.window_end >= {1})",
    first_seen,
    std::vformat(next_weekly_tick, std::make_format_args(first_seen))
  );
  }

  }  // namespace

auto database_storage_t::load_recent_ticks(info::tick_kind_e kind, uint32_t within_days)
  -> expected_ec<std::vector<info::tick_fact_t>>
  {
  using namespace std::chrono;

  auto rows{sqlite::select_from<info::tick_observation_t>(
    db_->db,
    sql_iface::tables::tick_observation,
    std::format(
      // a window wider than a few hours comes of a longer absence from the system and says nothing about
      // when the recalculation fell, while breaking the wave into separate rows - it stays out of the clustering
      " WHERE kind='{0}' AND (julianday(window_end) - julianday(window_begin)) * 24 <= 3"
      " AND window_end >="
      " (SELECT strftime('%Y-%m-%dT%H:%M:%SZ', max(window_end), '-{1} days') FROM {2} WHERE kind='{0}'){3}"
      " ORDER BY window_end ASC",
      simple_enum::enum_name(kind),
      within_days,
      sql_iface::tables::tick_observation,
      settled_colony_clause()
    )
  )};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};

  // What separates waves is a gap, not a calendar day and not an overlap of windows. Windows from
  // different systems need not intersect at all, because every system recalculates on its own - trying to
  // intersect them gave an empty set and lost most of the days
  std::vector<info::tick_fact_t> facts;
  for(auto it{rows->begin()}; it != rows->end();)
    {
    auto const wave_begin{it};
    auto last_end{it->window_end};
    for(; it != rows->end() and it->window_end - last_end <= same_wave_gap(); ++it)
      last_end = it->window_end;

    // sorted by the end of the window, so the first element of a wave recalculated earliest
    // and the last one latest
    auto const & first{*wave_begin};
    auto const & last{*std::prev(it)};

    std::vector<uint64_t> touched;
    touched.reserve(size_t(std::distance(wave_begin, it)));
    for(auto scan{wave_begin}; scan != it; ++scan)
      touched.push_back(scan->system_address);
    std::ranges::sort(touched);
    auto const unique_systems{std::ranges::unique(touched)};

    facts.push_back(info::tick_fact_t{
      .kind = kind,
      .start_begin = first.window_begin - client_lag(),
      .start_end = first.window_end,
      .end_begin = last.window_begin,
      .end_end = last.window_end,
      .samples = uint32_t(std::distance(wave_begin, it)),
      .systems = uint32_t(touched.size() - size_t(std::ranges::distance(unique_systems)))
    });
    }

  // the newest wave first - that is the one answering the question "when was the last"
  std::ranges::reverse(facts);
  return facts;
  }

auto database_storage_t::last_local_tick(uint64_t system_address, info::tick_kind_e kind)
  -> expected_ec<std::optional<std::chrono::sys_seconds>>
  {
  // influence changes at the influence tick, days won at the war tick - so each clock has a table
  // of its own and a last change of its own. A row of influence is written for a changed state too, and the
  // game names a faction's state differently on FSDJump and on Location (Retreat, then None, with the same
  // influence) - so only a row whose influence differs from the one before is a sign of the tick
  auto res{sqlite::select_signle_from<std::chrono::sys_seconds>(
    db_->db,
    kind == info::tick_kind_e::influence
      ? std::format(
          "SELECT timestamp FROM (SELECT timestamp, influence,"
          " LAG(influence) OVER (PARTITION BY faction_oid ORDER BY timestamp) AS previous"
          " FROM {} WHERE system_address={})"
          " WHERE previous IS NULL OR abs(influence - previous) > 1e-9 ORDER BY timestamp DESC LIMIT 1",
          sql_iface::tables::faction_influence,
          system_address
        )
      : std::format(
          "SELECT timestamp FROM {} WHERE system_address={} ORDER BY timestamp DESC LIMIT 1",
          sql_iface::tables::system_conflict,
          system_address
        )
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  return *res;
  }

auto database_storage_t::last_seen(uint64_t system_address) -> expected_ec<std::optional<std::chrono::sys_seconds>>
  {
  // influence is written only when it changed, the presence at every reading
  return sqlite::select_signle_from<std::chrono::sys_seconds>(
    db_->db,
    std::format(
      "SELECT last_seen FROM {} WHERE system_address={} ORDER BY last_seen DESC LIMIT 1",
      sql_iface::tables::faction_presence,
      system_address
    )
  );
  }

namespace
  {
///\brief a faction's influence by the last sample no later than the given moment, in percent
[[nodiscard]]
auto influence_at(sqlite3 * db, uint64_t system_address, std::string_view faction, std::chrono::sys_seconds when)
  -> std::optional<double>
  {
  auto res{sqlite::select_signle_from<double>(
    db,
    std::format(
      "SELECT fi.influence * 100 FROM {0} fi JOIN {1} f ON f.oid = fi.faction_oid"
      " WHERE fi.system_address={2} AND f.name='{3}' AND fi.timestamp <= '{4:%Y-%m-%dT%H:%M:%SZ}'"
      " ORDER BY fi.timestamp DESC LIMIT 1",
      sql_iface::tables::faction_influence,
      sql_iface::tables::faction_info,
      system_address,
      sqlite::escape_sql_quotes(faction),
      when
    )
  )};
  if(not res)
    return std::nullopt;

  return *res;
  }

///\brief a faction's state by the same sample the pre-wave influence is read from
[[nodiscard]]
auto state_at(sqlite3 * db, uint64_t system_address, std::string_view faction, std::chrono::sys_seconds when)
  -> std::string
  {
  auto res{sqlite::select_signle_from<std::string>(
    db,
    std::format(
      "SELECT fi.active_states FROM {0} fi JOIN {1} f ON f.oid = fi.faction_oid"
      " WHERE fi.system_address={2} AND f.name='{3}' AND fi.timestamp <= '{4:%Y-%m-%dT%H:%M:%SZ}'"
      " ORDER BY fi.timestamp DESC LIMIT 1",
      sql_iface::tables::faction_influence,
      sql_iface::tables::faction_info,
      system_address,
      sqlite::escape_sql_quotes(faction),
      when
    )
  )};
  if(not res or not *res)
    return {};

  // the states of this system - FactionState is a state of the faction taken from one of its systems
  return **res;
  }

///\brief the first change of influence in the system after the given moment, of any faction
///
/// It serves as proof that we really were there after the wave. No such change means either that we were
/// not, or that nothing moved - in both cases the day must not be settled
[[nodiscard]]
auto first_change_after(sqlite3 * db, uint64_t system_address, std::chrono::sys_seconds when)
  -> std::optional<std::chrono::sys_seconds>
  {
  auto res{sqlite::select_signle_from<std::chrono::sys_seconds>(
    db,
    std::format(
      "SELECT timestamp FROM {0} WHERE system_address={1} AND timestamp > '{2:%Y-%m-%dT%H:%M:%SZ}'"
      " ORDER BY timestamp ASC LIMIT 1",
      sql_iface::tables::faction_influence,
      system_address,
      when
    )
  )};
  if(not res)
    return std::nullopt;

  return *res;
  }

  }  // namespace

namespace bgs_detail
  {
///\brief a raw plus from a mission together with the system described - the field names must match the
/// query's aliases, and the struct itself needs external linkage, because glaze reflection does not reach
/// into an anonymous namespace
struct effort_row_t
  {
  uint64_t system_address;
  std::string system_name;
  uint64_t population;
  std::string faction;
  std::chrono::sys_seconds timestamp;
  int32_t pluses;
  uint64_t mission_id;
  };
  }  // namespace bgs_detail

auto database_storage_t::load_bgs_effort(uint32_t within_days, uint64_t system_address)
  -> expected_ec<std::vector<info::bgs_effort_t>>
  {
  using namespace std::chrono;

  auto waves{load_recent_ticks(info::tick_kind_e::influence, within_days)};
  if(not waves) [[unlikely]]
    return cxx23::unexpected{waves.error()};

  // waves come newest first, and the boundaries of days are easier to look for ascending. An empty list is
  // no error - on a database not yet rebuilt no wave has been detected, and the work was done all the same
  // and is to be shown, only as a single day not yet settled
  std::vector<info::tick_fact_t> ordered{*waves};
  std::ranges::reverse(ordered);

  // The period is counted from the last recorded work, not from the clock - the database is sometimes
  // older than today, and an empty report would not say whether there was no work or only older work
  auto cutoff{sqlite::select_signle_from<std::chrono::sys_seconds>(
    db_->db,
    std::format(
      "SELECT strftime('%Y-%m-%dT%H:%M:%SZ', max(timestamp), '-{} days') FROM {}",
      within_days,
      sql_iface::tables::mission_influence
    )
  )};
  if(not cutoff) [[unlikely]]
    return cxx23::unexpected{cutoff.error()};

  if(not *cutoff)
    return std::vector<info::bgs_effort_t>{};

  // A BGS day does not end at the period's boundary, so cutting straight through it would take away part
  // of the oldest day's pluses and understate its rate - the same day would look different at 7 days and
  // at 14. Instead we step back to the recalculation that opened that day
  std::chrono::sys_seconds since{**cutoff};
  for(info::tick_fact_t const & wave: ordered)
    if(wave.start_end <= **cutoff)
      since = wave.start_end;

  std::string const scope{system_address != 0u ? std::format(" AND mi.system_address={}", system_address) : ""};
  auto rows{sqlite::select_from<bgs_detail::effort_row_t>(
    db_->db,
    std::format(
      "(SELECT mi.system_address AS system_address, coalesce(ss.name, '') AS system_name,"
      " coalesce(ss.population, 0) AS population, mi.faction AS faction, mi.timestamp AS timestamp,"
      " mi.pluses AS pluses, mi.mission_id AS mission_id"
      " FROM {0} mi LEFT JOIN {1} ss ON ss.system_address = mi.system_address"
      " WHERE mi.timestamp >= '{2:%Y-%m-%dT%H:%M:%SZ}'{3})",
      sql_iface::tables::mission_influence,
      sql_iface::tables::star_system,
      since,
      scope
    ),
    ""
  )};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};

  struct bucket_t
    {
    info::bgs_effort_t effort;
    std::set<uint64_t> missions;
    };
  std::map<std::tuple<uint64_t, std::string, sys_seconds>, bucket_t> buckets;

  for(bgs_detail::effort_row_t const & row: *rows)
    {
    // a mission handed in before a wave counts towards the day that wave closes. Falling inside the wave's
    // own window cannot be decided either way, so it goes to the day being closed - there the mission most
    // likely still made it
    auto const closing{std::ranges::find_if(ordered, [&](info::tick_fact_t const & w)
                                            { return w.start_end >= row.timestamp; })};

    // after the last wave sits a day not yet settled - a zero marker says "still running"
    sys_seconds const closed_by{closing != ordered.end() ? closing->start_end : sys_seconds{}};

    auto & bucket{buckets[{row.system_address, row.faction, closed_by}]};
    if(bucket.effort.faction.empty())
      bucket.effort = info::bgs_effort_t{
        .system_address = row.system_address,
        .system_name = row.system_name,
        .population = row.population,
        .faction = row.faction,
        .closed_by = closed_by,
        .pushed_up = {},
        .pushed_down = {},
        .missions = {},
        .influence_before = {},
        .influence_after = {},
        .faction_state = {},
        .system_pushed_up = {},
        .system_gain = {}
      };

    if(row.pluses > 0)
      bucket.effort.pushed_up += row.pluses;
    else
      bucket.effort.pushed_down -= row.pluses;

    bucket.missions.insert(row.mission_id);
    }

  std::vector<info::bgs_effort_t> result;
  result.reserve(buckets.size());
  for(auto & [key, bucket]: buckets)
    {
    bucket.effort.missions = int32_t(bucket.missions.size());

    // a day not yet settled has nothing to close it, and to the rest we add the influence from both sides of the wave
    if(bucket.effort.closed_by != sys_seconds{})
      {
      auto const closing{std::ranges::find_if(
        ordered, [&](info::tick_fact_t const & w) { return w.start_end == bucket.effort.closed_by; }
      )};
      if(closing != ordered.end())
        {
        bucket.effort.influence_before
          = influence_at(db_->db, bucket.effort.system_address, bucket.effort.faction, closing->start_begin);
        bucket.effort.faction_state
          = state_at(db_->db, bucket.effort.system_address, bucket.effort.faction, closing->start_begin);

        // the value after the wave may be shown only with proof that we were there afterwards
        if(auto seen{first_change_after(db_->db, bucket.effort.system_address, closing->end_end)}; seen)
          bucket.effort.influence_after
            = influence_at(db_->db, bucket.effort.system_address, bucket.effort.faction, *seen);
        }
      }

    result.push_back(std::move(bucket.effort));
    }

  // The gain in influence is shared among the factions pushed on the same day in the same system, because
  // the percentages add up to a hundred. The cost of a point is therefore a property of the system and the
  // day, not of the faction; a single faction's share follows from how much of all the upward work went to it
  struct pool_t
    {
    int32_t pushed_up;
    double gain;
    bool complete;
    };
  std::map<std::pair<uint64_t, sys_seconds>, pool_t> pools;

  for(info::bgs_effort_t const & row: result)
    {
    if(row.pushed_up <= 0)
      continue;

    auto & pool{pools.try_emplace({row.system_address, row.closed_by}, pool_t{0, 0.0, true}).first->second};
    pool.pushed_up += row.pushed_up;

    // without a reading on both sides of the wave there is no telling how much this faction took from the
    // pool, and then the rest cannot be shared out fairly - the whole split of that day is lost
    if(not row.influence_before or not row.influence_after)
      pool.complete = false;
    else
      pool.gain += *row.influence_after - *row.influence_before;
    }

  for(info::bgs_effort_t & row: result)
    if(auto const found{pools.find({row.system_address, row.closed_by})}; found != pools.end())
      {
      row.system_pushed_up = found->second.pushed_up;
      if(found->second.complete)
        row.system_gain = found->second.gain;
      }

  // The newest days on top, and within a day the largest effort first. A day not yet settled is the
  // newest there can be, and its marker is zero - without the substitution it would land at the very
  // end, that is furthest from what we are doing now
  auto const freshness = [](info::bgs_effort_t const & row)
  { return row.closed_by == sys_seconds{} ? sys_seconds::max() : row.closed_by; };

  std::ranges::sort(
    result,
    [&freshness](info::bgs_effort_t const & l, info::bgs_effort_t const & r)
    {
      if(freshness(l) != freshness(r))
        return freshness(l) > freshness(r);
      return l.pushed_up + l.pushed_down > r.pushed_up + r.pushed_down;
    }
  );

  return result;
  }

auto database_storage_t::store_neutron_route(std::span<info::neutron_waypoint_t const> route) -> expected_ec<void>
  {
  // one remembered route - loading a new one replaces the previous one entirely
  if(auto res{sqlite::execute_query_no_result(db_->db, std::format("DELETE FROM {}", sql_iface::tables::neutron_route))};
     not res) [[unlikely]]
    return res;

  for(info::neutron_waypoint_t const & waypoint: route)
    if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::neutron_route, waypoint)}; not res)
      [[unlikely]]
      return res;

  return {};
  }

auto database_storage_t::load_neutron_route() -> expected_ec<std::vector<info::neutron_waypoint_t>>
  {
  return sqlite::select_from<info::neutron_waypoint_t>(
    db_->db, sql_iface::tables::neutron_route, " ORDER BY position"
  );
  }

auto database_storage_t::store(info::ship_transfer_t const & value) -> expected_ec<void>
  {
  // a rebuild from journals repeats every order - the ship and the moment of ordering tell them apart
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE ship_id={} AND ordered='{:%Y-%m-%dT%H:%M:%SZ}'",
      sql_iface::tables::ship_transfer,
      value.ship_id,
      value.ordered
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::ship_transfer, value);
  }

auto database_storage_t::load_transfers_in_flight(std::chrono::sys_seconds now)
  -> expected_ec<std::vector<info::ship_transfer_t>>
  {
  return sqlite::select_from<info::ship_transfer_t>(
    db_->db,
    sql_iface::tables::ship_transfer,
    std::format(" WHERE arrives > '{:%Y-%m-%dT%H:%M:%SZ}' ORDER BY arrives", now)
  );
  }

auto database_storage_t::load_fleet() -> expected_ec<std::vector<info::ship_t>>
  {
  return sqlite::select_from<info::ship_t>(db_->db, sql_iface::tables::ship, " ORDER BY ship_id");
  }

auto database_storage_t::store_fleet(std::span<info::ship_t const> ships) -> expected_ec<void>
  {
  // a few dozen rows, and every change is worked out on the whole list - so the list replaces the table
  if(auto res{sqlite::execute_query_no_result(db_->db, std::format("DELETE FROM {}", sql_iface::tables::ship))};
     not res) [[unlikely]]
    return res;

  for(info::ship_t const & ship: ships)
    if(auto res{sqlite::insert_into<info::ship_t, true>(db_->db, "ship_id"sv, sql_iface::tables::ship, ship)}; not res)
      [[unlikely]]
      return res;

  return {};
  }

namespace sql_iface
  {
///\brief a system's name and position, without the rest of its row
struct system_position_t
  {
  std::string name;
  double loc_x;
  double loc_y;
  double loc_z;
  };
  }  // namespace sql_iface

auto database_storage_t::load_system_positions(std::span<std::string const> names)
  -> expected_ec<std::map<std::string, std::array<double, 3>>>
  {
  std::map<std::string, std::array<double, 3>> result;
  if(names.empty())
    return result;

  std::string list;
  for(std::string const & name: names)
    list += std::format("{}'{}'", list.empty() ? "" : ",", sqlite::escape_sql_quotes(name));

  auto rows{sqlite::select_from<sql_iface::system_position_t>(
    db_->db,
    sql_iface::tables::star_system,
    std::format(" WHERE name IN ({})", list)
  )};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};

  // a system known only by name - from a signal or a mission, never jumped to - has no position; Sol has
  // one at the origin
  for(sql_iface::system_position_t const & row: *rows)
    if(row.loc_x != 0.0 or row.loc_y != 0.0 or row.loc_z != 0.0 or row.name == "Sol")
      result[row.name] = {row.loc_x, row.loc_y, row.loc_z};
  return result;
  }

auto database_storage_t::store(info::port_visit_t const & value) -> expected_ec<void>
  {
  // One row per port, with the marker moved at every further stop - and stops do repeat, so a plain
  // INSERT would fail on the key and leave the date of the first visit behind
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "INSERT INTO {0} (market_id, name, system, station_type, visited)"
      " VALUES ({1}, '{2}', '{3}', '{4}', '{5:%Y-%m-%dT%H:%M:%SZ}')"
      " ON CONFLICT(market_id) DO UPDATE SET name = excluded.name, system = excluded.system,"
      " station_type = excluded.station_type, visited = excluded.visited",
      sql_iface::tables::port_visit,
      value.market_id,
      sqlite::escape_sql_quotes(value.name),
      sqlite::escape_sql_quotes(value.system),
      sqlite::escape_sql_quotes(value.station_type),
      value.visited
    )
  );
  }

auto database_storage_t::load_last_port() -> expected_ec<std::optional<info::port_visit_t>>
  {
  auto res{sqlite::select_from<info::port_visit_t>(
    db_->db, sql_iface::tables::port_visit, " ORDER BY visited DESC LIMIT 100"
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  // visits stored before construction sites were known not to be safe ports are still here - so the rule
  // is applied once more on the way out
  for(info::port_visit_t & visit: *res)
    if(info::is_escape_pod_port(visit.station_type, visit.name))
      return std::optional<info::port_visit_t>{std::move(visit)};
  return std::optional<info::port_visit_t>{};
  }

auto database_storage_t::load_state_effort(uint64_t system_address) -> expected_ec<std::vector<info::state_effort_t>>
  {
  // a mission handed in after a wave's start window counts towards the next day - the same boundary the
  // BGS window puts between days
  auto waves{load_recent_ticks(info::tick_kind_e::influence, 30u)};
  if(not waves) [[unlikely]]
    return cxx23::unexpected{waves.error()};
  // with no wave detected yet there is no day to count within - the whole history would say nothing
  if(waves->empty())
    return std::vector<info::state_effort_t>{};
  std::chrono::sys_seconds const since{waves->front().start_end};

  return sqlite::select_from<info::state_effort_t>(
    db_->db,
    std::format(
      "(SELECT faction,"
      " sum(CASE WHEN pluses > 0 THEN pluses ELSE 0 END) AS influence_up,"
      " sum(CASE WHEN pluses < 0 THEN -pluses ELSE 0 END) AS influence_down,"
      " sum(CASE WHEN economy > 0 THEN economy ELSE 0 END) AS economy_up,"
      " sum(CASE WHEN economy < 0 THEN -economy ELSE 0 END) AS economy_down,"
      " sum(CASE WHEN security > 0 THEN security ELSE 0 END) AS security_up,"
      " sum(CASE WHEN security < 0 THEN -security ELSE 0 END) AS security_down"
      " FROM {0} WHERE system_address = {1} AND timestamp >= '{2:%Y-%m-%dT%H:%M:%SZ}' GROUP BY faction)",
      sql_iface::tables::mission_influence,
      system_address,
      since
    ),
    ""
  );
  }

auto database_storage_t::load_bgs_systems() -> expected_ec<std::vector<info::system_ref_t>>
  {
  // BGS is done where missions are handed in - the list comes from the work itself, with no separate setting
  return sqlite::select_from<info::system_ref_t>(
    db_->db,
    std::format(
      "(SELECT DISTINCT mi.system_address AS system_address, coalesce(ss.name, '') AS name"
      " FROM {0} mi LEFT JOIN {1} ss ON ss.system_address = mi.system_address"
      " ORDER BY name)",
      sql_iface::tables::mission_influence,
      sql_iface::tables::star_system
    ),
    ""
  );
  }

auto database_storage_t::load_war_onsets() -> expected_ec<std::vector<info::war_onset_t>>
  {
  // both markers bound it from one side each: the war started after the last "pending" and no later than
  // the first "active", and how much of that is the game's delay and how much our absence shows only in
  // the width of that span
  return sqlite::select_from<info::war_onset_t>(
    db_->db,
    std::format(
      // the same factions fight each other more than once, so every announcement looks for the passage
      // into war nearest after it, not the earliest in the whole history of that pair
      // the result is attached at the end, from the newest reading of the same war - only that says
      // who won it and by what ratio of days
      "(SELECT o.system_address AS system_address, o.system_name AS system_name, o.war_type AS war_type,"
      " o.faction1 AS faction1, o.faction2 AS faction2, o.pending_last AS pending_last,"
      " o.active_first AS active_first,"
      " coalesce(( SELECT c.won_days1 FROM {0} c WHERE c.system_address = o.system_address"
      "            AND c.faction1 = o.faction1 AND c.faction2 = o.faction2"
      "            AND c.timestamp >= o.active_first ORDER BY c.timestamp DESC LIMIT 1 ), 0) AS won_days1,"
      " coalesce(( SELECT c.won_days2 FROM {0} c WHERE c.system_address = o.system_address"
      "            AND c.faction1 = o.faction1 AND c.faction2 = o.faction2"
      "            AND c.timestamp >= o.active_first ORDER BY c.timestamp DESC LIMIT 1 ), 0) AS won_days2,"
      " coalesce(( SELECT c.status FROM {0} c WHERE c.system_address = o.system_address"
      "            AND c.faction1 = o.faction1 AND c.faction2 = o.faction2"
      "            AND c.timestamp >= o.active_first ORDER BY c.timestamp DESC LIMIT 1 ), '') AS status"
      " FROM ("
      "SELECT system_address, system_name, war_type, faction1, faction2,"
      " max(pending_last) AS pending_last, active_first FROM ("
      "   SELECT p.system_address AS system_address, coalesce(ss.name, '') AS system_name,"
      "   p.war_type AS war_type, p.faction1 AS faction1, p.faction2 AS faction2,"
      "   p.timestamp AS pending_last,"
      "   ( SELECT min(a.timestamp) FROM {0} a"
      "     WHERE a.system_address = p.system_address AND a.faction1 = p.faction1"
      "       AND a.faction2 = p.faction2 AND a.status = 'active' AND a.timestamp > p.timestamp"
      "   ) AS active_first"
      "   FROM {0} p LEFT JOIN {1} ss ON ss.system_address = p.system_address"
      "   WHERE p.status = 'pending')"
      " WHERE active_first IS NOT NULL"
      " GROUP BY system_address, faction1, faction2, active_first) o"
      " ORDER BY o.active_first DESC)",
      sql_iface::tables::system_conflict,
      sql_iface::tables::star_system
    ),
    ""
  );
  }

auto database_storage_t::load_war_countdown(uint64_t system_address)
  -> expected_ec<std::vector<info::war_countdown_t>>
  {
  auto conflicts{load_conflicts(system_address)};
  if(not conflicts) [[unlikely]]
    return cxx23::unexpected{conflicts.error()};

  // the database holds the whole history, and only the newest state of each pair can be counted down from
  std::map<std::pair<std::string, std::string>, info::conflict_t const *> latest;
  for(info::conflict_t const & conflict: *conflicts)
    {
    auto & slot{latest[{conflict.faction1, conflict.faction2}]};
    if(slot == nullptr or slot->timestamp < conflict.timestamp)
      slot = &conflict;
    }

  ///\brief this many days won settles a conflict
  constexpr uint32_t days_to_win{4};

  std::vector<info::war_countdown_t> result;
  for(auto const & [pair, entry]: latest)
    {
    info::conflict_t const & conflict{*entry};

    // an empty status means the conflict has already closed - there is nothing left to count down
    if(conflict.status.empty())
      continue;

    bool const active{conflict.status == "active"};
    uint32_t const won{std::max(conflict.won_days1, conflict.won_days2)};

    result.push_back(info::war_countdown_t{
      .system_address = system_address,
      .war_type = conflict.war_type,
      .faction1 = conflict.faction1,
      .faction2 = conflict.faction2,
      .won_days1 = conflict.won_days1,
      .won_days2 = conflict.won_days2,
      // an announced one needs one more recalculation just to start
      .ticks_left = (active ? 0u : 1u) + (won >= days_to_win ? 0u : days_to_win - won),
      .active = active
    });
    }

  // najblizsze rozstrzygniecia pierwsze - to one decyduja, kiedy miec bondy na reku
  std::ranges::sort(
    result, [](info::war_countdown_t const & l, info::war_countdown_t const & r) { return l.ticks_left < r.ticks_left; }
  );

  return result;
  }

namespace territory_detail
  {
///\brief a system of the territory as the galaxy describes it - the names must match the query's aliases, and
/// the struct needs external linkage, because glaze reflection does not reach into an anonymous namespace
struct system_row_t
  {
  uint64_t system_address;
  std::string name;
  uint64_t population;
  std::string controlling_faction;
  double loc_x;
  double loc_y;
  double loc_z;
  };
  }  // namespace territory_detail

auto database_storage_t::newest_influence_wave() -> expected_ec<std::optional<std::chrono::sys_seconds>>
  {
  auto waves{load_recent_ticks(info::tick_kind_e::influence, 30u)};
  if(not waves) [[unlikely]]
    return cxx23::unexpected{waves.error()};
  if(waves->empty())
    return std::optional<std::chrono::sys_seconds>{};
  return std::optional{waves->front().start_begin};
  }

auto database_storage_t::load_territory(std::span<std::string const> own_factions)
  -> expected_ec<std::vector<territory::system_t>>
  {
  if(own_factions.empty())
    return std::vector<territory::system_t>{};

  std::string names;
  for(std::string const & name: own_factions)
    names += std::format("{}'{}'", names.empty() ? "" : ",", sqlite::escape_sql_quotes(name));

  // a faction that retreated from a system keeps its last row of presence there, so only the newest reading
  // of each system says who is in it
  auto rows{sqlite::select_from<territory_detail::system_row_t>(
    db_->db,
    std::format(
      "(SELECT ss.system_address AS system_address, coalesce(ss.name, '') AS name,"
      " coalesce(ss.population, 0) AS population, coalesce(ss.controlling_faction, '') AS controlling_faction,"
      " coalesce(ss.loc_x, 0) AS loc_x, coalesce(ss.loc_y, 0) AS loc_y, coalesce(ss.loc_z, 0) AS loc_z"
      " FROM {1} ss WHERE ss.system_address IN (SELECT fp.system_address FROM {0} fp JOIN {2} fi ON fi.oid = "
      "fp.faction_oid"
      " WHERE fi.name IN ({3})"
      " AND fp.last_seen = (SELECT max(q.last_seen) FROM {0} q WHERE q.system_address = fp.system_address)))",
      sql_iface::tables::faction_presence,
      sql_iface::tables::star_system,
      sql_iface::tables::faction_info,
      names
    ),
    ""
  )};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};

  auto factions{load_factions()};
  if(not factions) [[unlikely]]
    return cxx23::unexpected{factions.error()};
  std::map<int64_t, std::string_view> faction_names;
  for(info::faction_info_t const & faction: *factions)
    faction_names.emplace(faction.oid, faction.name);

  auto wave{newest_influence_wave()};
  if(not wave) [[unlikely]]
    return cxx23::unexpected{wave.error()};

  std::vector<territory::system_t> result;
  result.reserve(rows->size());
  for(territory_detail::system_row_t & row: *rows)
    {
    territory::system_t system{
      .system_address = row.system_address,
      .name = std::move(row.name),
      .population = row.population,
      .controlling = std::move(row.controlling_faction),
      // the galaxy writes a system down before its position is known - Sol alone really stands at the origin
      .position = row.loc_x != 0.0 or row.loc_y != 0.0 or row.loc_z != 0.0
                    ? std::optional{std::array{row.loc_x, row.loc_y, row.loc_z}}
                    : std::nullopt
    };

    auto present{load_present_factions(system.system_address)};
    auto history{load_influence_history(system.system_address)};
    auto seen{last_seen(system.system_address)};
    auto changed{last_local_tick(system.system_address, info::tick_kind_e::influence)};
    auto wars{load_war_countdown(system.system_address)};
    auto effort{load_state_effort(system.system_address)};
    if(not present or not history or not seen or not changed or not wars or not effort) [[unlikely]]
      return cxx23::unexpected{std::make_error_code(std::errc::io_error)};

    system.seen = *seen;
    system.changed = *changed;
    bool const read_since_wave{*wave and *seen and **seen >= **wave};

    for(info::faction_ref_t const & ref: *present)
      {
      auto const named{faction_names.find(ref.faction_oid)};
      if(named == faction_names.end())
        continue;

      // the newest row is the faction's state now; the last one before the wave is where the tick found it
      info::faction_influence_t const * latest{};
      info::faction_influence_t const * before_wave{};
      for(info::faction_influence_t const & entry: *history)
        {
        if(entry.faction_oid != ref.faction_oid)
          continue;
        latest = &entry;
        if(*wave and entry.timestamp < **wave)
          before_wave = &entry;
        }
      if(latest == nullptr)
        continue;

      territory::faction_t faction{
        .name = std::string{named->second},
        .influence = latest->influence * 100.0,
        .moved = {},
        .active = latest->active_states,
        .pending = latest->pending_states
      };
      // no row since the wave, with a reading since it, means the faction held its ground
      if(read_since_wave and before_wave != nullptr)
        faction.moved = (latest->influence - before_wave->influence) * 100.0;
      system.factions.push_back(std::move(faction));
      }
    std::ranges::sort(system.factions, std::ranges::greater{}, &territory::faction_t::influence);

    for(info::war_countdown_t const & war: *wars)
      system.wars.push_back(
        territory::war_t{
          .war_type = war.war_type,
          .faction1 = war.faction1,
          .faction2 = war.faction2,
          .won_days1 = war.won_days1,
          .won_days2 = war.won_days2,
          .ticks_left = war.ticks_left,
          .active = war.active
        }
      );

    for(info::state_effort_t const & pushed: *effort)
      {
      system.pushed_up += pushed.influence_up;
      system.pushed_down += pushed.influence_down;
      }

    result.push_back(std::move(system));
    }

  std::ranges::sort(result, {}, &territory::system_t::name);
  return result;
  }

auto database_storage_t::load_tick_stats(info::tick_kind_e kind, uint32_t within_days)
  -> expected_ec<info::tick_stats_t>
  {
  using namespace std::chrono;

  auto facts{load_recent_ticks(kind, within_days)};
  if(not facts) [[unlikely]]
    return cxx23::unexpected{facts.error()};

  info::tick_stats_t stats{
    .kind = kind,
    .waves = uint32_t(facts->size()),
    .typical_gap = {},
    .longest_gap = {},
    .multi_system_waves = {},
    .widest_spread = {},
    .typical_window = {}
  };

  std::vector<minutes> widths;
  widths.reserve(facts->size());
  for(auto const & fact: *facts)
    widths.push_back(duration_cast<minutes>(fact.start_end - fact.start_begin));

  if(not widths.empty())
    {
    auto const middle{widths.begin() + std::ptrdiff_t(widths.size() / 2u)};
    std::ranges::nth_element(widths, middle);
    stats.typical_window = *middle;
    }

  std::vector<minutes> gaps;
  gaps.reserve(facts->size());
  for(auto const & fact: *facts)
    {
    if(fact.systems > 1u)
      ++stats.multi_system_waves;

    stats.widest_spread = std::max(stats.widest_spread, duration_cast<minutes>(fact.end_end - fact.start_end));
    }

  // waves run newest first, so the gap separates neighbours in the list
  for(size_t ix{1}; ix < facts->size(); ++ix)
    gaps.push_back(duration_cast<minutes>((*facts)[ix - 1u].start_end - (*facts)[ix].start_end));

  if(not gaps.empty())
    {
    stats.longest_gap = *std::ranges::max_element(gaps);
    auto const middle{gaps.begin() + std::ptrdiff_t(gaps.size() / 2u)};
    std::ranges::nth_element(gaps, middle);
    stats.typical_gap = *middle;
    }

  return stats;
  }

auto database_storage_t::load_station(uint64_t market_id) -> expected_ec<std::optional<info::station_t>>
  {
  auto res{sqlite::select_from<info::station_t>(
    db_->db, sql_iface::tables::station, std::format(" WHERE market_id={}", market_id)
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::station_t>{};

  return std::optional<info::station_t>{std::move((*res)[0])};
  }

auto database_storage_t::note_settlement_owner(
  uint64_t market_id, uint64_t system_address, std::string_view faction, std::chrono::sys_seconds when
) -> expected_ec<void>
  {
  if(market_id == 0u or faction.empty())
    return {};

  auto last{sqlite::select_from<info::settlement_owner_t>(
    db_->db,
    sql_iface::tables::settlement_owner,
    std::format(" WHERE market_id={} ORDER BY last_seen DESC LIMIT 1", market_id)
  )};
  if(not last) [[unlikely]]
    return cxx23::unexpected{last.error()};

  // the same owner as the last seen stretches its row; one seen again later than that row keeps it
  if(not last->empty() and last->front().faction == faction)
    {
    info::settlement_owner_t row{last->front()};
    if(when <= row.last_seen)
      return {};
    row.last_seen = when;
    return sqlite::update_pk(db_->db, "oid"sv, sql_iface::tables::settlement_owner, row, row.oid);
    }
  // a rebuild reads the journals in order, so an older sighting of another owner is a stray - kept out
  if(not last->empty() and when < last->front().last_seen)
    return {};

  return sqlite::insert_into(
    db_->db,
    "oid"sv,
    sql_iface::tables::settlement_owner,
    info::settlement_owner_t{
      .market_id = market_id, .system_address = system_address, .faction = std::string{faction}, .first_seen = when, .last_seen = when
    }
  );
  }

auto database_storage_t::store(info::ground_bond_t const & value) -> expected_ec<void>
  { return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::ground_bond, value); }

auto database_storage_t::store(info::colony_claim_t const & value) -> expected_ec<void>
  {
  auto known{sqlite::select_from<info::colony_claim_t>(
    db_->db, sql_iface::tables::colony_claim, std::format(" WHERE system_address={}", value.system_address)
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(known->empty())
    return sqlite::insert_into<info::colony_claim_t, true>(
      db_->db, "system_address"sv, sql_iface::tables::colony_claim, value
    );
  // a release keeps who claimed it and when; a claim again takes the system back
  info::colony_claim_t row{known->front()};
  if(value.released)
    row.released = true;
  else
    row = value;
  return sqlite::update_pk(db_->db, "system_address"sv, sql_iface::tables::colony_claim, row, row.system_address);
  }

auto database_storage_t::store(info::carrier_movement_t const & value) -> expected_ec<void>
  {
  // the same move read twice - a journal read again - is kept once
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE carrier_id={} AND kind='{}' AND timestamp='{:%Y-%m-%dT%H:%M:%SZ}'",
      sql_iface::tables::carrier_movement,
      value.carrier_id,
      value.kind,
      value.timestamp
    )
  )};
  if(known and *known and **known != 0u)
    return {};
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::carrier_movement, value);
  }

auto database_storage_t::load_carrier_cargo(uint64_t carrier_id) -> expected_ec<std::vector<info::carrier_cargo_t>>
  {
  return sqlite::select_from<info::carrier_cargo_t>(
    db_->db,
    sql_iface::tables::carrier_cargo,
    std::format(" WHERE carrier_id={} AND count>0 ORDER BY count DESC", carrier_id)
  );
  }

auto database_storage_t::load_commodity_categories() -> expected_ec<std::map<std::string, std::string>>
  {
  auto rows{sqlite::select_from<info::commodity_t>(db_->db, sql_iface::tables::commodity, "")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::map<std::string, std::string> categories;
  for(info::commodity_t const & c: *rows)
    {
    // the dictionary writes "Consumer items" - every word capitalised reads as the game shows it
    std::string category{c.category};
    for(size_t i{}; i != category.size(); ++i)
      if((i == 0u or category[i - 1u] == ' ') and category[i] >= 'a' and category[i] <= 'z')
        category[i] = char(category[i] - 'a' + 'A');
    categories[c.key.empty() ? info::commodity_key(c.name) : c.key] = std::move(category);
    }
  return categories;
  }

auto database_storage_t::load_commodity_names() -> expected_ec<std::vector<std::pair<std::string, std::string>>>
  {
  auto rows{sqlite::select_from<info::commodity_t>(db_->db, sql_iface::tables::commodity, " ORDER BY name")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::vector<std::pair<std::string, std::string>> names;
  names.reserve(rows->size());
  for(info::commodity_t & c: *rows)
    names.emplace_back(c.key.empty() ? info::commodity_key(c.name) : std::move(c.key), std::move(c.name));
  return names;
  }

auto database_storage_t::load_carrier_cargo_totals() -> expected_ec<std::map<std::string, int64_t>>
  {
  auto rows{sqlite::select_from<info::carrier_cargo_t>(db_->db, sql_iface::tables::carrier_cargo, " WHERE count>0")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::map<std::string, int64_t> totals;
  for(info::carrier_cargo_t const & row: *rows)
    totals[row.key] += row.count;
  return totals;
  }

auto database_storage_t::change_carrier_cargo(info::carrier_cargo_change_t const & given) -> expected_ec<void>
  {
  if(given.delta == 0)
    return {};
  // the hold names a commodity by its internal name, "steel" or "cmmcomposite" - the market's dictionary
  // has the name the game shows
  info::carrier_cargo_change_t change{given};
  if(std::ranges::none_of(change.commodity, [](char c) { return c >= 'A' and c <= 'Z'; }))
    if(auto names{sqlite::select_from<info::commodity_t>(db_->db, sql_iface::tables::commodity, "")}; names)
      for(info::commodity_t const & c: *names)
        if((c.key.empty() ? info::commodity_key(c.name) : c.key) == change.key)
          {
          change.commodity = c.name;
          break;
          }
  auto known{sqlite::select_from<info::carrier_cargo_t>(
    db_->db,
    sql_iface::tables::carrier_cargo,
    std::format(
      " WHERE carrier_id={} AND key='{}' LIMIT 1", change.carrier_id, sqlite::escape_sql_quotes(change.key)
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::carrier_cargo_change, change)}; not res)
    [[unlikely]]
    return res;
  if(known->empty())
    return sqlite::insert_into(
      db_->db,
      "oid"sv,
      sql_iface::tables::carrier_cargo,
      info::carrier_cargo_t{
        .carrier_id = change.carrier_id, .key = change.key, .commodity = change.commodity,
        .count = std::max<int64_t>(change.delta, 0)
      }
    );
  info::carrier_cargo_t row{known->front()};
  // taken off more than was known to be there: the count was wrong - it stops at nothing
  row.count = std::max<int64_t>(row.count + change.delta, 0);
  if(not change.commodity.empty())
    row.commodity = change.commodity;
  return sqlite::update_pk(db_->db, "oid"sv, sql_iface::tables::carrier_cargo, row, row.oid);
  }

auto database_storage_t::set_carrier_cargo(
  uint64_t carrier_id, std::string_view given_key, std::string_view commodity, int64_t count, std::chrono::sys_seconds when
) -> expected_ec<void>
  {
  std::string const key{given_key};
  if(key.empty())
    return {};
  int64_t now_count{};
  if(auto known{sqlite::select_from<info::carrier_cargo_t>(
       db_->db,
       sql_iface::tables::carrier_cargo,
       std::format(" WHERE carrier_id={} AND key='{}' LIMIT 1", carrier_id, sqlite::escape_sql_quotes(key))
     )};
     known and not known->empty())
    now_count = known->front().count;
  return change_carrier_cargo(
    info::carrier_cargo_change_t{
      .timestamp = when, .carrier_id = carrier_id, .key = key, .commodity = std::string{commodity},
      .delta = std::max<int64_t>(count, 0) - now_count, .source = "edit"
    }
  );
  }

auto database_storage_t::load_carrier_states(std::chrono::sys_seconds now, std::chrono::minutes cooldown)
  -> expected_ec<std::vector<info::carrier_state_t>>
  {
  auto moves{sqlite::select_from<info::carrier_movement_t>(
    db_->db, sql_iface::tables::carrier_movement, " ORDER BY carrier_id, timestamp"
  )};
  if(not moves) [[unlikely]]
    return cxx23::unexpected{moves.error()};

  // a jump takes about a minute - the position comes that long after the departure
  constexpr std::chrono::minutes jump_takes{1};

  struct order_t
    {
    info::carrier_movement_t request;
    std::string from;
    ///\brief the position read after the departure - the arrival - once there is one
    std::optional<std::chrono::sys_seconds> arrived;
    };
  std::map<uint64_t, info::carrier_state_t> states;
  std::map<uint64_t, std::optional<order_t>> orders;
  for(info::carrier_movement_t const & move: *moves)
    {
    info::carrier_state_t & state{states[move.carrier_id]};
    state.carrier_id = move.carrier_id;
    if(not move.carrier_type.empty())
      state.carrier_type = move.carrier_type;
    auto & order{orders[move.carrier_id]};
    if(move.kind == "request")
      order = order_t{.request = move, .from = state.system, .arrived = std::nullopt};
    else if(move.kind == "cancel")
      order.reset();
    else
      {
      // a position read after the departure is the arrival - where the carrier went, and when
      if(order and move.timestamp >= order->request.departure and not order->arrived)
        order->arrived = move.timestamp;
      state.system = move.system;
      state.since = move.timestamp;
      }
    }

  std::vector<info::carrier_state_t> result;
  for(auto & [id, state]: states)
    {
    if(auto const & order{orders[id]}; order)
      {
      auto const arrival{order->arrived.value_or(order->request.departure + jump_takes)};
      if(now < arrival + cooldown)
        {
        state.jumping = true;
        state.from = order->from;
        state.to = order->request.system;
        state.to_body = order->request.body;
        state.departure = order->request.departure;
        state.arrival = arrival;
        state.arrived = order->arrived.has_value();
        }
      else if(not order->arrived)
        {
        // not in the game at the arrival, so no position came - it went where it was sent
        state.system = order->request.system;
        state.since = arrival;
        }
      }
    // the name and callsign come from the carrier's own statistics, when we have seen them
    if(auto row{sqlite::select_from<info::carrier_t>(
         db_->db, sql_iface::tables::carrier, std::format(" WHERE market_id={} LIMIT 1", id)
       )};
       row and not row->empty())
      {
      state.name = row->front().carrier_name;
      state.callsign = row->front().carrier_id;
      }
    result.push_back(std::move(state));
    }
  return result;
  }

auto database_storage_t::store_construction(
  info::construction_depot_t const & depot, std::span<info::construction_need_t const> needs
) -> expected_ec<void>
  {
  auto known{sqlite::select_from<info::construction_depot_t>(
    db_->db, sql_iface::tables::construction_depot, std::format(" WHERE market_id={}", depot.market_id)
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  // an older reading - a journal read again - never overwrites a newer one
  if(not known->empty() and depot.updated < known->front().updated)
    return {};
  info::construction_depot_t row{depot};
  // the event does not name the system; the station row, from the docking, does
  if(row.system_address == 0u and not known->empty())
    row.system_address = known->front().system_address;
  auto stored{
    known->empty()
      ? sqlite::insert_into<info::construction_depot_t, true>(
          db_->db, "market_id"sv, sql_iface::tables::construction_depot, row
        )
      : sqlite::update_pk(db_->db, "market_id"sv, sql_iface::tables::construction_depot, row, row.market_id)
  };
  if(not stored) [[unlikely]]
    return stored;

  // the site writes its state every little while, mostly the same - rows are rewritten only on a change
  auto current{sqlite::select_from<info::construction_need_t>(
    db_->db, sql_iface::tables::construction_need, std::format(" WHERE market_id={} ORDER BY key", depot.market_id)
  )};
  if(not current) [[unlikely]]
    return cxx23::unexpected{current.error()};
  std::vector<info::construction_need_t> incoming{needs.begin(), needs.end()};
  std::ranges::sort(incoming, {}, &info::construction_need_t::key);
  bool const same{std::ranges::equal(
    *current,
    incoming,
    [](info::construction_need_t const & a, info::construction_need_t const & b)
    { return a.key == b.key and a.required == b.required and a.provided == b.provided; }
  )};
  if(same)
    return {};

  if(auto res{sqlite::execute_query_no_result(
       db_->db, std::format("DELETE FROM {} WHERE market_id={}", sql_iface::tables::construction_need, depot.market_id)
     )};
     not res) [[unlikely]]
    return res;
  for(info::construction_need_t const & need: incoming)
    if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::construction_need, need)}; not res)
      [[unlikely]]
      return res;
  return {};
  }

auto database_storage_t::store_delivery(info::construction_delivery_t const & value) -> expected_ec<void>
  {
  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::construction_delivery, value)}; not res)
    [[unlikely]]
    return res;
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "UPDATE {} SET provided = min(required, provided + {}) WHERE market_id={} AND key='{}'",
      sql_iface::tables::construction_need,
      value.amount,
      value.market_id,
      sqlite::escape_sql_quotes(value.key)
    )
  );
  }

auto database_storage_t::mark_construction_abandoned(uint64_t market_id, bool abandoned, std::chrono::sys_seconds when)
  -> expected_ec<void>
  {
  if(not abandoned)
    return sqlite::execute_query_no_result(
      db_->db, std::format("DELETE FROM {} WHERE market_id={}", sql_iface::tables::construction_abandoned, market_id)
    );
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "INSERT OR REPLACE INTO {} (market_id, marked) VALUES ({}, '{:%Y-%m-%dT%H:%M:%SZ}')",
      sql_iface::tables::construction_abandoned,
      market_id,
      when
    )
  );
  }

auto database_storage_t::load_construction_sites(bool with_abandoned)
  -> expected_ec<std::vector<info::construction_site_t>>
  {
  auto depots{sqlite::select_from<info::construction_depot_t>(
    db_->db,
    sql_iface::tables::construction_depot,
    std::format(
      " WHERE complete=0 AND failed=0 AND system_address IN (SELECT system_address FROM {} WHERE released=0)"
      " ORDER BY system_address, market_id",
      sql_iface::tables::colony_claim
    )
  )};
  if(not depots) [[unlikely]]
    return cxx23::unexpected{depots.error()};

  std::vector<info::construction_site_t> sites;
  for(info::construction_depot_t const & depot: *depots)
    {
    auto marked{sqlite::select_signle_from<uint64_t>(
      db_->db,
      std::format(
        "SELECT market_id FROM {} WHERE market_id={}", sql_iface::tables::construction_abandoned, depot.market_id
      )
    )};
    bool const abandoned{marked and *marked};
    if(abandoned and not with_abandoned)
      continue;
    info::construction_site_t site{.depot = depot, .name = {}, .system = {}, .needs = {}, .abandoned = abandoned};
    if(auto station{load_station(depot.market_id)}; station and *station)
      site.name = (*station)->name;
    // the colonisation ship's site goes by a name the game never localised
    if(constexpr std::string_view ship{"$EXT_PANEL_ColonisationShip;"}; site.name.starts_with(ship))
      {
      std::string_view rest{std::string_view{site.name}.substr(ship.size())};
      while(rest.starts_with(' '))
        rest.remove_prefix(1u);
      site.name = std::format("Colonisation Ship: {}", rest);
      }
    // the name alone - the whole system with its bodies is a dozen queries, read here for every site
    if(auto name{sqlite::select_signle_from<std::string>(
         db_->db,
         std::format("SELECT name FROM {} WHERE system_address={}", sql_iface::tables::star_system, depot.system_address)
       )};
       name and *name)
      site.system = **name;
    auto needs{sqlite::select_from<info::construction_need_t>(
      db_->db,
      sql_iface::tables::construction_need,
      std::format(" WHERE market_id={} ORDER BY (required - provided) DESC", depot.market_id)
    )};
    if(needs)
      site.needs = std::move(*needs);
    sites.push_back(std::move(site));
    }
  return sites;
  }

auto database_storage_t::load_war_views(uint64_t system_address) -> expected_ec<std::vector<info::war_view_t>>
  {
  auto conflicts{load_conflicts(system_address)};
  if(not conflicts) [[unlikely]]
    return cxx23::unexpected{conflicts.error()};

  // A war is the rows of one pair of factions since the last row that said it was over - its start is the
  // first row after that. What is under way now is a pair whose newest row is pending or active
  struct run_t
    {
    info::conflict_t newest;
    std::chrono::sys_seconds started;
    };
  std::map<std::pair<std::string, std::string>, run_t> runs;
  for(info::conflict_t const & row: *conflicts)
    {
    auto const key{std::pair{row.faction1, row.faction2}};
    auto it{runs.find(key)};
    bool const fresh{it == runs.end() or it->second.newest.status.empty()};
    if(it == runs.end())
      it = runs.emplace(key, run_t{row, row.timestamp}).first;
    else if(fresh and not row.status.empty())
      it->second.started = row.timestamp;
    it->second.newest = row;
    }

  auto stations{load_stations(system_address)};
  if(not stations) [[unlikely]]
    return cxx23::unexpected{stations.error()};

  auto const highest = [&](uint64_t market_id, std::string_view condition) -> info::cz_intensity_e
  {
    auto res{sqlite::select_signle_from<uint64_t>(
      db_->db,
      std::format(
        "SELECT intensity FROM {} WHERE market_id={} AND {} ORDER BY intensity DESC LIMIT 1",
        sql_iface::tables::ground_bond,
        market_id,
        condition
      )
    )};
    if(not res or not *res)
      return info::cz_intensity_e::unknown;
    return static_cast<info::cz_intensity_e>(std::min<uint64_t>(**res, 3u));
  };

  std::vector<info::war_view_t> views;
  for(auto const & [key, run]: runs)
    {
    info::conflict_t const & war{run.newest};
    if(war.status != "pending" and war.status != "active")
      continue;
    // an election is no fight for settlements
    if(war.war_type == "election")
      continue;

    info::war_view_t view{.conflict = war, .started = run.started, .settlements = {}};
    std::string const start{std::format("{:%Y-%m-%dT%H:%M:%SZ}", run.started)};
    for(info::station_t const & station: *stations)
      {
      if(not info::is_ground_settlement(station))
        continue;

      // the owner when the war began: the last seen before it, else the first seen since, else the
      // one the place is known by now - a war hands a settlement over only when it ends
      std::string owner{station.controlling_faction};
      auto before{sqlite::select_from<info::settlement_owner_t>(
        db_->db,
        sql_iface::tables::settlement_owner,
        std::format(" WHERE market_id={} AND first_seen<='{}' ORDER BY first_seen DESC LIMIT 1", station.market_id, start)
      )};
      auto since{sqlite::select_from<info::settlement_owner_t>(
        db_->db,
        sql_iface::tables::settlement_owner,
        std::format(" WHERE market_id={} AND first_seen>'{}' ORDER BY first_seen LIMIT 1", station.market_id, start)
      )};
      if(before and not before->empty())
        owner = before->front().faction;
      else if(since and not since->empty())
        owner = since->front().faction;

      if(owner != war.faction1 and owner != war.faction2)
        continue;

      view.settlements.push_back(
        info::war_settlement_t{
          .market_id = station.market_id,
          .name = station.name,
          .economy = station.economy,
          .owner_before = owner,
          .before = highest(station.market_id, std::format("timestamp<'{}'", start)),
          .now = highest(station.market_id, std::format("timestamp>='{}'", start))
        }
      );
      }
    std::ranges::sort(
      view.settlements,
      [](info::war_settlement_t const & a, info::war_settlement_t const & b)
      { return std::tie(a.owner_before, a.name) < std::tie(b.owner_before, b.name); }
    );
    views.push_back(std::move(view));
    }
  return views;
  }

auto database_storage_t::load_station(uint64_t system_address, std::string_view name)
  -> expected_ec<std::optional<info::station_t>>
  {
  auto res{sqlite::select_from<info::station_t>(
    db_->db,
    sql_iface::tables::station,
    std::format(" WHERE system_address={} AND name='{}'", system_address, sqlite::escape_sql_quotes(name))
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::station_t>{};

  return std::optional<info::station_t>{std::move((*res)[0])};
  }

auto database_storage_t::store_journal_progress(std::chrono::sys_seconds last_event) -> expected_ec<void>
  {
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "INSERT INTO {0} (id, last_event) VALUES (1, '{1:%Y-%m-%dT%H:%M:%SZ}')"
      " ON CONFLICT(id) DO UPDATE SET last_event = excluded.last_event",
      sql_iface::tables::journal_progress,
      last_event
    )
  );
  }

auto database_storage_t::load_journal_progress() -> expected_ec<std::optional<std::chrono::sys_seconds>>
  {
  return sqlite::select_signle_from<std::chrono::sys_seconds>(
    db_->db, std::format("SELECT last_event FROM {} WHERE id=1", sql_iface::tables::journal_progress)
  );
  }

auto database_storage_t::store_owner(info::db_owner_t const & owner) -> expected_ec<void>
  {
  std::string query{std::format(
    "INSERT INTO {0} (fid, name) VALUES ('{1}', '{2}') ON CONFLICT(fid) DO UPDATE SET name = excluded.name",
    sql_iface::tables::db_owner,
    sqlite::escape_sql_quotes(owner.fid),
    sqlite::escape_sql_quotes(owner.name)
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::load_owner() -> expected_ec<std::optional<info::db_owner_t>>
  {
  auto res{sqlite::select_from<info::db_owner_t>(db_->db, sql_iface::tables::db_owner, {})};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};
  if(res->empty())
    return std::optional<info::db_owner_t>{};
  return std::optional<info::db_owner_t>{std::move((*res)[0])};
  }

auto database_storage_t::load_place_owner(std::string_view system_name, std::string_view place)
  -> expected_ec<std::optional<std::string>>
  {
  // A retreat hands everything over. When a faction's retreat completes at a tick it leaves the
  // system altogether, and every asset it held there - settlements included - passes to whoever
  // controls the system. Nothing announces this per station, so until the next docking corrects the
  // row the stored owner names a faction that is no longer there.
  //
  // Absence alone is not enough to conclude it, though. Engineer bases, megaships and the Pilots'
  // Federation are held by names that never stand in a faction list at all - taking their stations
  // from them would be wrong in 161 places to be right in one. So the owner is overruled only where
  // it once played the background simulation in this very system, by having an influence history
  // here, and has since stopped being among the factions seen at the newest reading.
  return sqlite::select_signle_from<std::string>(
    db_->db,
    std::format(
      "SELECT CASE WHEN fo.oid IS NOT NULL AND sy.controlling_faction <> ''"
      "             AND EXISTS(SELECT 1 FROM {4} i"
      "                        WHERE i.system_address = st.system_address AND i.faction_oid = fo.oid)"
      "             AND NOT EXISTS(SELECT 1 FROM {2} p"
      "                            WHERE p.system_address = st.system_address AND p.faction_oid = fo.oid"
      "                              AND p.last_seen = (SELECT max(last_seen) FROM {2}"
      "                                                 WHERE system_address = st.system_address))"
      "        THEN sy.controlling_faction ELSE st.controlling_faction END"
      " FROM {0} st JOIN {1} sy ON sy.system_address = st.system_address"
      " LEFT JOIN {3} fo ON fo.name = st.controlling_faction"
      " WHERE sy.name='{5}' AND st.name='{6}' LIMIT 1",
      sql_iface::tables::station,
      sql_iface::tables::star_system,
      sql_iface::tables::faction_presence,
      sql_iface::tables::faction_info,
      sql_iface::tables::faction_influence,
      sqlite::escape_sql_quotes(system_name),
      sqlite::escape_sql_quotes(place)
    )
  );
  }

auto database_storage_t::load_stations(uint64_t system_address) -> expected_ec<std::vector<info::station_t>>
  {
  return sqlite::select_from<info::station_t>(
    db_->db, sql_iface::tables::station, std::format(" WHERE system_address={} ORDER BY name", system_address)
  );
  }

auto database_storage_t::load_market_entries(uint64_t market_id) -> expected_ec<std::vector<info::market_entry_t>>
  {
  // the field names of market_entry_t coincide with the columns of both tables, so the join goes
  // through the same query generator as an ordinary read
  return sqlite::select_from<info::market_entry_t>(
    db_->db,
    std::format("{} JOIN {} ON id = commodity_id", sql_iface::tables::market_item, sql_iface::tables::commodity),
    std::format(" WHERE market_id={} ORDER BY category, name", market_id)
  );
  }

auto database_storage_t::replace_market(
  uint64_t market_id,
  std::chrono::sys_seconds updated,
  std::span<info::commodity_t const> commodities,
  std::span<info::market_item_t const> items
) -> expected_ec<void>
  {
  // the commodity dictionary is shared by every market; only the unknown ones are added
  for(info::commodity_t const & value: commodities)
    {
    auto known{sqlite::select_signle_from<uint64_t>(
      db_->db, std::format("SELECT count(*) FROM {} WHERE id={}", sql_iface::tables::commodity, value.id)
    )};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};

    if(*known and **known != 0)
      {
      // a row from before the internal name was kept learns it now
      if(not value.key.empty())
        if(
          auto res{sqlite::execute_query_no_result(
            db_->db,
            std::format(
              "UPDATE {} SET key='{}' WHERE id={} AND key=''",
              sql_iface::tables::commodity,
              sqlite::escape_sql_quotes(value.key),
              value.id
            )
          )};
          not res
        ) [[unlikely]]
          return res;
      continue;
      }

    if(auto res{sqlite::insert_into<info::commodity_t, true>(db_->db, "id"sv, sql_iface::tables::commodity, value)};
       not res) [[unlikely]]
      return res;
    }

  // a market changes all the time, we keep the last reading alone
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db, std::format("DELETE FROM {} WHERE market_id={}", sql_iface::tables::market_item, market_id)
    )};
    not res
  ) [[unlikely]]
    return res;

  for(info::market_item_t const & item: items)
    if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::market_item, item)}; not res) [[unlikely]]
      return res;

  // the time of the reading is kept with the market, not with the station - a station is rebuildable, a reading is not
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db, std::format("DELETE FROM {} WHERE market_id={}", sql_iface::tables::market, market_id)
    )};
    not res
  ) [[unlikely]]
    return res;

  return sqlite::insert_into<info::market_info_t, true>(
    db_->db, "market_id"sv, sql_iface::tables::market, info::market_info_t{.market_id = market_id, .updated = updated}
  );
  }

auto database_storage_t::load_market_info(uint64_t market_id) -> expected_ec<std::optional<info::market_info_t>>
  {
  auto res{sqlite::select_from<info::market_info_t>(
    db_->db, sql_iface::tables::market, std::format(" WHERE market_id={}", market_id)
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::market_info_t>{};

  return std::optional<info::market_info_t>{std::move((*res)[0])};
  }

auto database_storage_t::store(system_signal_t const & value) -> expected_ec<void>
  {
  // the same signal comes back with every fss scan of the system
  std::string query{std::format(
    "SELECT count(*) FROM {} WHERE system_address={} AND name='{}'",
    sql_iface::tables::system_signal,
    value.system_address,
    sqlite::escape_sql_quotes(value.name)
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    {
    // the signal is known already; what counts is when it was last reported
    std::string update{std::format(
      "UPDATE {} SET last_seen='{:%Y-%m-%dT%H:%M:%SZ}' WHERE system_address={} AND name='{}'",
      sql_iface::tables::system_signal,
      value.last_seen,
      value.system_address,
      sqlite::escape_sql_quotes(value.name)
    )};
    return sqlite::execute_query_no_result(db_->db, update);
    }

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::system_signal, value);
  }

auto database_storage_t::load_system_signals(uint64_t system_address)
  -> expected_ec<std::vector<system_signal_t>>
  {
  auto res{sqlite::select_from<system_signal_t>(
    db_->db, sql_iface::tables::system_signal, std::format(" WHERE system_address={} ORDER BY name", system_address)
  )};
  if(not res) [[unlikely]]
    return res;

  // a construction site or a compromised nav beacon disappears from the system, but the row stays behind
  return filter_current_visit(std::move(*res));
  }

auto database_storage_t::store(info::conflict_t const & value) -> expected_ec<void>
  {
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::system_conflict, value);
  }

auto database_storage_t::last_conflict(uint64_t system_address, std::string_view faction1, std::string_view faction2)
  -> expected_ec<std::optional<info::conflict_t>>
  {
  auto res{sqlite::select_from<info::conflict_t>(
    db_->db,
    sql_iface::tables::system_conflict,
    std::format(
      " WHERE system_address={} AND faction1='{}' AND faction2='{}' ORDER BY timestamp DESC LIMIT 1",
      system_address,
      sqlite::escape_sql_quotes(faction1),
      sqlite::escape_sql_quotes(faction2)
    )
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::conflict_t>{};

  return std::optional<info::conflict_t>{std::move((*res)[0])};
  }

auto database_storage_t::load_conflicts(uint64_t system_address) -> expected_ec<std::vector<info::conflict_t>>
  {
  return sqlite::select_from<info::conflict_t>(
    db_->db,
    sql_iface::tables::system_conflict,
    std::format(" WHERE system_address={} ORDER BY timestamp", system_address)
  );
  }

auto database_storage_t::load_influence_history(uint64_t system_address)
  -> expected_ec<std::vector<info::faction_influence_t>>
  {
  return sqlite::select_from<info::faction_influence_t>(
    db_->db,
    sql_iface::tables::faction_influence,
    std::format(" WHERE system_address={} ORDER BY timestamp", system_address)
  );
  }

auto database_storage_t::load_systems_with_influence() -> expected_ec<std::vector<info::system_ref_t>>
  {
  return sqlite::select_from<info::system_ref_t>(
    db_->db,
    sql_iface::tables::star_system,
    std::format(
      " WHERE system_address IN (SELECT DISTINCT system_address FROM {}) ORDER BY name",
      sql_iface::tables::faction_influence
    )
  );
  }

auto database_storage_t::load_factions() -> expected_ec<std::vector<info::faction_info_t>>
  {
  auto res{sqlite::select_from<sql_iface::faction_info_t>(db_->db, sql_iface::tables::faction_info, {})};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  // reputation is personal, so it comes from the main database rather than from the shared knowledge of factions
  auto reputations{sqlite::select_from<info::faction_reputation_t>(db_->db, sql_iface::tables::faction_reputation, {})};
  if(not reputations) [[unlikely]]
    return cxx23::unexpected{reputations.error()};

  std::map<std::string, double, std::less<>> known;
  for(info::faction_reputation_t & entry: *reputations)
    known.emplace(std::move(entry.faction), entry.reputation);

  std::vector<info::faction_info_t> factions;
  factions.reserve(res->size());
  for(sql_iface::faction_info_t & row: *res)
    {
    factions.emplace_back(sql_iface::to_native_fromat(std::move(row)));
    if(auto const it{known.find(factions.back().name)}; it != known.end())
      factions.back().reputation = it->second;
    }
  return factions;
  }

auto database_storage_t::update_faction_info(info::faction_info_t const & faction, bool with_reputation)
  -> expected_ec<void>
  {
  // reputation goes to the personal database, the rest to the shared one - the name joins the two
  if(with_reputation)
    {
    std::string reputation{std::format(
      "INSERT INTO {0} (faction, reputation) VALUES ('{1}', {2})"
      " ON CONFLICT(faction) DO UPDATE SET reputation = excluded.reputation",
      sql_iface::tables::faction_reputation,
      sqlite::escape_sql_quotes(faction.name),
      faction.reputation
    )};
    if(auto res{sqlite::execute_query_no_result(db_->db, reputation)}; not res) [[unlikely]]
      return res;
    }

  auto const row{sql_iface::to_db_fromat(faction)};
  if(faction.oid != -1)
    return sqlite::update_pk(db_->db, "oid"sv, sql_iface::tables::faction_info, row, faction.oid);
  else
    return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::faction_info, row);
  }

auto database_storage_t::load_carrier(std::string_view carrier_id) -> expected_ec<std::optional<info::carrier_t>>
  {
  auto res{sqlite::select_from<info::carrier_t>(
    db_->db, sql_iface::tables::carrier, std::format(" WHERE carrier_id='{}'", sqlite::escape_sql_quotes(carrier_id))
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::carrier_t>{};

  return std::optional<info::carrier_t>{std::move((*res)[0])};
  }

auto database_storage_t::store(info::micro_resource_t const & value) -> expected_ec<void>
  {
  auto known{sqlite::select_from<info::micro_resource_t>(
    db_->db,
    sql_iface::tables::micro_resource,
    std::format(" WHERE name='{}'", sqlite::escape_sql_quotes(value.name))
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(known->empty())
    return sqlite::insert_into<info::micro_resource_t, true>(
      db_->db, "name"sv, sql_iface::tables::micro_resource, value
    );

  // each source knows a different part - the id and the readable name from the bartender, the category from a sale
  info::micro_resource_t merged{std::move((*known)[0])};
  bool changed{};
  if(merged.id == 0 and value.id != 0)
    {
    merged.id = value.id;
    changed = true;
    }
  if(merged.localised.empty() and not value.localised.empty())
    {
    merged.localised = value.localised;
    changed = true;
    }
  if(merged.category.empty() and not value.category.empty())
    {
    merged.category = value.category;
    changed = true;
    }

  if(not changed)
    return {};

  std::string query{std::format(
    "UPDATE {} SET id={}, localised='{}', category='{}' WHERE name='{}'",
    sql_iface::tables::micro_resource,
    merged.id,
    sqlite::escape_sql_quotes(merged.localised),
    sqlite::escape_sql_quotes(merged.category),
    sqlite::escape_sql_quotes(merged.name)
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(info::consumable_use_t const & value) -> expected_ec<void>
  {
  // replaying the journal repeats the uses; time and the place in that second tell them apart
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE timestamp='{:%Y-%m-%dT%H:%M:%SZ}' AND seq={}",
      sql_iface::tables::consumable_use,
      value.timestamp,
      value.seq
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(*known and **known != 0)
    return {};
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::consumable_use, value);
  }

auto database_storage_t::store(info::foot_kill_t const & value) -> expected_ec<void>
  {
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE timestamp='{:%Y-%m-%dT%H:%M:%SZ}' AND seq={}",
      sql_iface::tables::foot_kill,
      value.timestamp,
      value.seq
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(*known and **known != 0)
    return {};
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::foot_kill, value);
  }

auto database_storage_t::load_bartender_summary(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::bartender_summary_t>>
  {
  return sqlite::select_from<info::bartender_summary_t>(
    db_->db,
    std::format(
      "(SELECT i.name AS name, coalesce(r.localised, '') AS localised, coalesce(r.category, '') AS category,"
      " sum(CASE WHEN s.kind = 'sold' THEN i.count ELSE 0 END) AS sold,"
      " sum(CASE WHEN s.kind = 'bought' THEN i.count ELSE 0 END) AS bought,"
      " sum(CASE WHEN s.kind = 'bartered' AND i.received = 0 THEN i.count ELSE 0 END) AS bartered_away,"
      " sum(CASE WHEN s.kind = 'bartered' AND i.received != 0 THEN i.count ELSE 0 END) AS bartered_for"
      " FROM {0} i JOIN {1} s ON s.oid = i.sale_oid LEFT JOIN {2} r ON r.name = i.name"
      " WHERE s.timestamp >= '{3:%Y-%m-%dT%H:%M:%SZ}' GROUP BY i.name)",
      sql_iface::tables::micro_sale_item,
      sql_iface::tables::micro_sale,
      sql_iface::tables::micro_resource,
      since
    ),
    ""
  );
  }

auto database_storage_t::load_bartender_totals(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::bartender_total_t>>
  {
  return sqlite::select_from<info::bartender_total_t>(
    db_->db,
    std::format(
      "(SELECT kind, count(*) AS transactions, sum(price) AS credits FROM {} WHERE timestamp >= "
      "'{:%Y-%m-%dT%H:%M:%SZ}' GROUP BY kind)",
      sql_iface::tables::micro_sale,
      since
    ),
    ""
  );
  }

auto database_storage_t::load_consumable_summary(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::consumable_summary_t>>
  {
  return sqlite::select_from<info::consumable_summary_t>(
    db_->db,
    std::format(
      "(SELECT u.name AS name, coalesce(r.localised, '') AS localised, sum(u.count) AS used"
      " FROM {0} u LEFT JOIN {1} r ON r.name = u.name WHERE u.timestamp >= '{2:%Y-%m-%dT%H:%M:%SZ}'"
      " GROUP BY u.name ORDER BY used DESC)",
      sql_iface::tables::consumable_use,
      sql_iface::tables::micro_resource,
      since
    ),
    ""
  );
  }

auto database_storage_t::load_foot_kills(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::foot_kill_summary_t>>
  {
  return sqlite::select_from<info::foot_kill_summary_t>(
    db_->db,
    std::format(
      "(SELECT kind, grenade, weapon, count(*) AS kills FROM {} WHERE timestamp >= '{:%Y-%m-%dT%H:%M:%SZ}'"
      " GROUP BY kind, grenade, weapon ORDER BY kills DESC)",
      sql_iface::tables::foot_kill,
      since
    ),
    ""
  );
  }

auto database_storage_t::store(info::micro_sale_t const & sale, std::span<info::micro_sale_item_t const> items)
  -> expected_ec<void>
  {
  // replaying the journal repeats the same transactions; time, market and kind tell them apart
  std::string known_query{std::format(
    "SELECT count(*) FROM {} WHERE market_id={} AND timestamp='{:%Y-%m-%dT%H:%M:%SZ}' AND kind='{}'",
    sql_iface::tables::micro_sale,
    sale.market_id,
    sale.timestamp,
    simple_enum::enum_name(sale.kind)
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, known_query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::micro_sale, sale)}; not res) [[unlikely]]
    return res;

  auto sale_oid{sqlite::select_signle_from<int64_t>(
    db_->db, std::format("SELECT max(oid) FROM {}", sql_iface::tables::micro_sale)
  )};
  if(not sale_oid or not *sale_oid) [[unlikely]]
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));

  for(info::micro_sale_item_t const & item: items)
    {
    info::micro_sale_item_t row{item};
    row.sale_oid = **sale_oid;
    if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::micro_sale_item, row)}; not res) [[unlikely]]
      return res;
    }

  return {};
  }

auto database_storage_t::carrier_oid( std::string_view name ) -> expected_ec<std::optional<int64_t>>
  {
  std::string query{
    std::format("SELECT oid FROM {} WHERE carrier_id='{}'", sql_iface::tables::carrier, sqlite::escape_sql_quotes(name))
  };
  return sqlite::select_signle_from<uint64_t>(db_->db, query);
  }

[[nodiscard]]
auto database_storage_t::set_carrier_tracked(std::string_view carrier_id, bool tracked) -> expected_ec<void>
  {
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "UPDATE {} SET tracked={} WHERE carrier_id='{}'",
      sql_iface::tables::carrier,
      tracked ? 1 : 0,
      sqlite::escape_sql_quotes(carrier_id)
    )
  );
  }

auto database_storage_t::update_carrier(info::carrier_t const & carrier) -> expected_ec<void>
  {
  if(not db_->db)
    return cxx23::unexpected(std::make_error_code(std::errc::not_connected));
  
  if(carrier.oid != -1)
    return sqlite::update_pk(db_->db, "oid"sv, sql_iface::tables::carrier, carrier, carrier.oid);
  else
  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::carrier, carrier)};
     not res) [[unlikely]]
    return res;
    
  return {};
  }
  
auto database_storage_t::store(info::fcmaterial_t const & value) -> expected_ec<void>
  {
  std::string query{
    std::format("SELECT count(*) FROM {} WHERE carrier_id={} and material_id={} and timestamp={}",
                sql_iface::tables::carrier_materials, value.carrier_id, value.material_id, value.timestamp)
  };
  auto cntres{sqlite::select_signle_from<uint64_t>(db_->db, query)};
  if(not cntres) [[unlikely]]
    return cxx23::unexpected{cntres.error()};
  auto cnt { *cntres};
  if( *cnt == 0)
    return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::carrier_materials, value);
  return {};
  }

auto database_storage_t::load_system(uint64_t system_address)
  -> cxx23::expected<std::optional<star_system_t>, std::error_code>
  {
  auto res{sqlite::select_from<sql_iface::star_system_t>(
    db_->db, sql_iface::tables::star_system, std::format(" WHERE system_address='{}'", system_address)
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(not res->empty())
    {
    assert(res->size() == 1);
    star_system_t system{to_native_fromat(std::move((*res)[0]))};

    // this character's progress comes separately - galaxy does not know who scanned what
    if(auto progress{sqlite::select_from<info::system_progress_t>(
         db_->db, sql_iface::tables::system_progress, std::format(" WHERE system_address={}", system_address)
       )};
       progress)
      system.fss_complete = not progress->empty() and progress->front().fss_complete;
    else [[unlikely]]
      return cxx23::unexpected{progress.error()};

    auto mapped{sqlite::select_from<info::body_progress_t>(
      db_->db, sql_iface::tables::body_progress, std::format(" WHERE system_address={}", system_address)
    )};
    if(not mapped) [[unlikely]]
      return cxx23::unexpected{mapped.error()};

    auto sampled{sqlite::select_from<info::genus_progress_t>(
      db_->db, sql_iface::tables::genus_progress, std::format(" WHERE system_address={}", system_address)
    )};
    if(not sampled) [[unlikely]]
      return cxx23::unexpected{sampled.error()};

    if(auto signals{load_system_signals(system_address)}; signals)
      system.system_signals = std::move(*signals);
    else [[unlikely]]
      return cxx23::unexpected{signals.error()};

      {
      auto res2{sqlite::select_from<sql_iface::body_t>(
        db_->db, sql_iface::tables::body, std::format(" WHERE ref_system_address='{}'", system_address)
      )};
      if(not res2) [[unlikely]]
        return cxx23::unexpected{res.error()};

      std::vector<sql_iface::body_t> bodies{std::move(*res2)};
      for(sql_iface::body_t & body: bodies)
        {
        system.bodies.emplace_back(sql_iface::to_native_fromat(std::move(body)));
        body_t & out_body{system.bodies.back()};

        if(body_type_e(body.details_type) == body_type_e::planet)
          {
          auto res3{sqlite::select_from<sql_iface::planet_details_t>(
            db_->db, sql_iface::tables::planet_details, std::format(" WHERE ref_body_oid='{}'", body.oid)
          )};
          if(not res3) [[unlikely]]
            return cxx23::unexpected{res.error()};
          assert(res3->size() == 1);
          out_body.details = sql_iface::to_native_fromat((*res3)[0]);
          planet_details_t & details{std::get<planet_details_t>(out_body.details)};

          if(auto it{std::ranges::find(*mapped, out_body.body_id, &info::body_progress_t::body_id)};
             it != mapped->end())
            {
            details.mapped = it->mapped;
            details.footfalled = it->footfalled;
            }

            {
            auto res4{sqlite::select_from<sql_iface::signal_t>(
              db_->db, sql_iface::tables::signal, std::format(" WHERE ref_body_oid='{}'", body.oid)
            )};
            if(not res4) [[unlikely]]
              return cxx23::unexpected{res.error()};
            if(not res4->empty())
              std::ranges::transform(
                *res4,
                std::back_inserter(details.signals_),
                [](sql_iface::signal_t & sig) -> events::signal_t
                { return sql_iface::to_native_fromat(std::move(sig)); }
              );
            }
            {
            auto res4{sqlite::select_from<sql_iface::genus_t>(
              db_->db, sql_iface::tables::genus, std::format(" WHERE ref_body_oid='{}'", body.oid)
            )};
            if(not res4) [[unlikely]]
              return cxx23::unexpected{res.error()};
            if(not res4->empty())
              std::ranges::transform(
                *res4,
                std::back_inserter(details.genuses_),
                [&sampled, id = out_body.body_id](sql_iface::genus_t & sig) -> events::genus_t
                {
                  events::genus_t gen{sql_iface::to_native_fromat(std::move(sig))};
                  auto const it{std::ranges::find_if(
                    *sampled,
                    [&](info::genus_progress_t const & pr)
                    { return pr.body_id == id and pr.genus == gen.Genus_Localised; }
                  )};
                  gen.Sampled = it != sampled->end() and it->sampled;
                  return gen;
                }
              );
            }
          }
        else
          {
          auto res3{sqlite::select_from<sql_iface::star_details_t>(
            db_->db, sql_iface::tables::star_details, std::format(" WHERE ref_body_oid='{}'", body.oid)
          )};
          if(not res3) [[unlikely]]
            return cxx23::unexpected{res.error()};
          assert(res3->size() == 1);
          out_body.details = sql_iface::to_native_fromat((*res3)[0]);
          }
        }
      }
      // barycentres - what a body of a shared orbit (two stars, or two planets of one pair) itself
      // orbits; without this a system reopened after a restart has every such body's position collapse
      // to its own small local wobble, missing the barycentre's own, usually much larger, offset
      {
      auto res5{sqlite::select_from<sql_iface::bary_centre_t>(
        db_->db, sql_iface::tables::bary_centre, std::format(" WHERE ref_system_address='{}'", system_address)
      )};
      if(not res5) [[unlikely]]
        return cxx23::unexpected{res.error()};
      for(sql_iface::bary_centre_t const & bc: *res5)
        system.bary_centre.push_back(sql_iface::to_native_fromat(bc));
      }
      // rings
      {
      auto res4{sqlite::select_from<sql_iface::ring_t>(
        db_->db, sql_iface::tables::ring, std::format(" WHERE ref_system_address='{}'", system_address)
      )};
      if(not res4) [[unlikely]]
        return cxx23::unexpected{res.error()};
      if(not res4->empty())
        {
        for(sql_iface::ring_t & db_ring: *res4)
          {
          ring_t & ring{system.rings.emplace_back(sql_iface::to_native_fromat(std::move(db_ring)))};
          auto res4{sqlite::select_from<sql_iface::signal_t>(
            db_->db, sql_iface::tables::signal, std::format(" WHERE ref_body_oid='{}'", db_ring.oid)
          )};
          if(not res4) [[unlikely]]
            return cxx23::unexpected{res.error()};
          if(not res4->empty())
            std::ranges::transform(
              *res4,
              std::back_inserter(ring.signals_),
              [](sql_iface::signal_t & sig) -> events::signal_t { return sql_iface::to_native_fromat(std::move(sig)); }
            );
          }
        }
      }
    return system;
    }
  return {};
  }

auto database_storage_t::close() -> void
  {
  if(db_->db)
    db_->close();
  }
