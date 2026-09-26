// #define SPDLOG_USE_STD_FORMAT
#include <databse_storage.h>
#include <set>
#include <sqlite3.h>
#include <filesystem>
#include <glaze/glaze.hpp>
#include <elite_events.h>
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
    .sub_class = v.sub_class
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
    .sub_class = v.sub_class
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

///\brief Sampled zostaje puste - probka nalezy do postaci i przychodzi z genus_progress
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
    .details_type = uint8_t(v.body_type())
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
    .was_discovered = v.was_discovered
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
    .mean_anomaly = bc.mean_anomaly
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
    .population = system.population
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
    .population = system.population
  };
  }

///\brief frakcja bez reputacji - ta jest osobista i siedzi w bazie glownej
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

///\brief reputacja zostaje zerowa - dopelnia ja odczyt z faction_reputation
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
  // fakty o galaktyce - te same dla kazdej postaci, wiec moga byc wspolne dla dwoch kont
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
  // tick jest wlasnoscia gry, nie postaci - i odtwarza sie z journali razem z reszta galaktyki
  inline constexpr std::string_view tick_observation{"galaxy.tick_observation"};
  // to czego nie da sie odtworzyc siedzi w osobnym pliku podpietym jako schemat live
  inline constexpr std::string_view market{"live.market"};
  inline constexpr std::string_view commodity{"live.commodity"};
  inline constexpr std::string_view market_item{"live.market_item"};
  // co zrobila TA postac - zostaje w bazie osobistej, klucze naturalne zeby przezyly przebudowe galaxy
  inline constexpr std::string_view db_owner{"db_owner"};
  inline constexpr std::string_view system_progress{"system_progress"};
  inline constexpr std::string_view body_progress{"body_progress"};
  inline constexpr std::string_view genus_progress{"genus_progress"};
  inline constexpr std::string_view faction_reputation{"faction_reputation"};
  inline constexpr std::string_view mission{"mission"};
  inline constexpr std::string_view mission_cargo{"mission_cargo"};
  inline constexpr std::string_view mission_influence{"mission_influence"};
  // jedno i drugie odtwarza sie z journali, wiec miejsce jest w bazie osobistej, nie w live
  inline constexpr std::string_view ship_transfer{"ship_transfer"};
  inline constexpr std::string_view port_visit{"port_visit"};
  inline constexpr std::string_view micro_resource{"live.micro_resource"};
  // sprzedaz mikrozasobow to zdarzenie journala, wiec odtwarzalna
  inline constexpr std::string_view micro_sale{"micro_sale"};
  inline constexpr std::string_view micro_sale_item{"micro_sale_item"};
  inline constexpr std::string_view micro_acquisition{"micro_acquisition"};
  // trasy wyznaczonej na zewnatrz nie ma w zadnym journalu, wiec przebudowa by ja skasowala
  inline constexpr std::string_view neutron_route{"live.neutron_route"};
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

///\brief sqlite nie zawsze ustawia opis bledu, formatowanie nullptr jako {} konczy sie strlen(nullptr)
[[nodiscard]]
auto sql_error_text(char const * err_msg) noexcept -> char const *
  {
  return err_msg != nullptr ? err_msg : "brak opisu bledu";
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

///\brief nazwy kolumn w jednej linii, do komunikatu bledu
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
  // PRAGMA table_info zwraca kolumny opisu, nazwa stoi na drugiej pozycji
  if(argc > 1 and argv[1] != nullptr)
    static_cast<std::vector<std::string> *>(d)->emplace_back(argv[1]);
  return 0;
  }

///\brief nazwy kolumn istniejacej tabeli, dziala tez dla nazw z przedrostkiem schematu
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

///\brief sprawdza czy istniejaca tabela ma ksztalt jakiego oczekuje kod
///\detail CREATE TABLE IF NOT EXISTS milczy gdy tabela istnieje w innym ksztalcie, a baza zbierana
/// na zywo nigdy nie jest kasowana - bez tej kontroli kazdy zapis sypalby sie osobno w trakcie pracy
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
      value = deserialize<T>(fields[ix]);
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

  // agregat po pustym zbiorze (max, min, sum) oddaje wiersz z NULL, a sqlite podaje go jako nullptr.
  // string_view zbudowany z nullptr to UB, wiec brak wartosci musi zostac brakiem wartosci
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

///\brief indeks na tabeli, ktora moze siedziec w podpietym schemacie
///\detail w CREATE INDEX schemat stoi przy nazwie indeksu, a nie przy tabeli - podanie
/// "galaxy.body" w obu miejscach to blad skladni, wiec nazwa rozchodzi sie tutaj
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
  // pliki poboczne leza obok bazy glownej i kazdy zyje wlasnym zyciem
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

  // czytanie z gui i zapis z watku sledzacego to osobne polaczenia, czekamy zamiast dostac SQLITE_BUSY
  sqlite3_busy_timeout(db_->db, 3000);

  // dane zbierane na zywo i wiedza o galaktyce w osobnych plikach - ATTACH zaklada je gdy nie istnieja
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
    // kazdy insert to osobna transakcja, a przy imporcie calosci logow jest ich setki tysiecy
    // - bez fsync na wiersz i z dziennikiem w pamieci import idzie wielokrotnie szybciej.
    // Awaria konczy sie uszkodzona baza, ale import i tak buduje ja od zera.
    //
    // przedrostek schematu nie jest ozdobnikiem: niekwalifikowane journal_mode i synchronous siegaja
    // WSZYSTKICH podpietych baz, wiec zdjelyby te zabezpieczenia takze z live.sqlite - a tego pliku
    // nie da sie odtworzyc i bywa wspoldzielony z druga, dzialajaca instancja. main i galaxy import
    // buduje od zera, wiec tam skroty sa na miejscu - i konieczne, bo wiekszosc wierszy idzie
    // wlasnie do galaxy; zostawienie jej z fsync na wiersz spowalnia caly import kilkunastokrotnie.
    // temp_store dotyczy polaczenia, nie bazy, wiec zostaje bez przedrostka
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
    // gui, okna narzedziowe i overlay czytaja z osobnych polaczen w trakcie zapisu z watku journala.
    // w dzienniku rollback taki czytelnik czeka na pisarza i po busy_timeout dostaje "database is
    // locked"; w WAL nie czeka wcale, bo czyta ostatni spojny obraz obok trwajacego zapisu
    for(std::string_view pragma:
        {"PRAGMA journal_mode = WAL;"sv, "PRAGMA live.journal_mode = WAL;"sv, "PRAGMA galaxy.journal_mode = WAL;"sv})
      if(auto res{sqlite::execute_query_no_result(db_->db, pragma)}; not res) [[unlikely]]
        return res;
    }

  // najpierw migracja, bo create_database sprawdza ksztalt tabel i na starej odmowilby otwarcia
  if(auto res{migrate_live_schema()}; not res) [[unlikely]]
    return res;

  // wszystkie CREATE sa IF NOT EXISTS, wiec istniejaca baza dostaje brakujace tabele i indeksy
  return create_database();
  }

auto database_storage_t::migrate_live_schema() -> expected_ec<void>
  {
  // kolumna dolozona w miejscu, zamiast przebudowy calej bazy. dla live.sqlite to jedyna droga,
  // bo tego pliku nie da sie odtworzyc z journali; dla galaxy to uprzejmosc - narzedzie rusza od
  // razu, a przebudowa i tak wypelni kolumne wstecz, kiedy bedzie po drodze
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
       // stan flotowca z CarrierStats - dokladany w miejscu, bo live.sqlite nie powstaje od nowa
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
       addition_t{sql_iface::tables::carrier, "stats_seen"sv, "TEXT DEFAULT ''"sv}})
    {
    auto known{sqlite::table_columns(db_->db, add.table)};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};

    // pusta lista znaczy ze tabeli jeszcze nie ma - powstanie od razu w docelowym ksztalcie
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

  if(auto res{sqlite::create_table<info::micro_resource_t>(db_->db, "name"sv, sql_iface::tables::micro_resource)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_sale_t>(db_->db, "oid"sv, sql_iface::tables::micro_sale)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_sale_item_t>(db_->db, "oid"sv, sql_iface::tables::micro_sale_item)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_acquisition_t>(db_->db, "oid"sv, sql_iface::tables::micro_acquisition)};
     not res) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::neutron_waypoint_t>(db_->db, "oid"sv, sql_iface::tables::neutron_route)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::carrier_t>(db_->db, "oid"sv, sql_iface::tables::carrier)}; not res)
    [[unlikely]]
    return res;
    
  if(auto res{sqlite::create_table<info::fcmaterial_t>(db_->db, "oid"sv, sql_iface::tables::carrier_materials)}; not res)
    [[unlikely]]
    return res;

  // wyszukiwanie frakcji po nazwie i ostatniego wpisu influence idzie przy kazdym odwiedzonym systemie
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

  // klucz musi byc unikalny, bo upsert obecnosci opiera sie na ON CONFLICT
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

  // ten sam sygnal wraca przy kazdym skanie fss, wiec kazdy trafia najpierw w sprawdzenie
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::system_signal, "system_address, name")}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::market_item, "market_id")}; not res) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::carrier_materials, "carrier_id, material_id, timestamp"
    )};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::db_owner_t>(db_->db, "fid"sv, sql_iface::tables::db_owner)}; not res)
    [[unlikely]]
    return res;

  // postep tej postaci - osobny od wiedzy o galaktyce, wiec w bazie glownej
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

  // upsert postepu opiera sie na ON CONFLICT, wiec klucze musza byc unikalne
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

  // kazda zdobycz sprawdza czy juz ja znamy, a jest ich sto kilkadziesiat tysiecy
  if(
    auto res{sqlite::create_index(db_->db, sql_iface::tables::micro_acquisition, "timestamp, market_id, name")};
    not res
  ) [[unlikely]]
    return res;

  // misja rusza kilka frakcji naraz, wiec wpisow jest wielokrotnie wiecej niz misji. Ten sam klucz
  // sluzy odsiewaniu powtorek przy przebudowie i wyszukiwaniu po systemie
  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::mission_influence, "mission_id, faction, system_address", "key", true
    )};
    not res
  ) [[unlikely]]
    return res;

  // to samo okno wpada raz na system, a przebudowa powtarza je od poczatku
  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::tick_observation, "kind, system_address, window_begin, window_end", "key", true
    )};
    not res
  ) [[unlikely]]
    return res;

  // wyszukiwanie ostatnich ticków idzie po koncu okna
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::tick_observation, "kind, window_end", "recent")};
     not res) [[unlikely]]
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
    // redirected znaczy zrobiona i czekajaca na oddanie - po terminie jest zamknieta tak samo
    // jak nieoddana, bo albo przepadla albo fakt oddania nie trafil do journala
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
  // ponowne odtwarzanie journala trafia na te sama misje, wiec wpis ma byc jeden
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
  // to samo co load_supply_options, ale bez warunku na zapas - interesuje nas sam fakt,
  // ze rynek tym handluje, zeby odroznic chwilowa pustke od braku jakiegokolwiek zrodla
  return sqlite::select_from<info::supply_option_t>(
    db_->db,
    std::format(
      "(SELECT i.market_id AS market_id,"
      " coalesce(st.name,'') AS station,"
      " coalesce(st.station_type,'') AS station_type,"
      " coalesce(ss.name,'') AS system,"
      " c.name AS commodity,"
      " n.needed AS needed,"
      " i.stock AS stock,"
      " i.buy_price AS buy_price"
      " FROM (SELECT mc.commodity AS commodity, sum(mc.count) AS needed"
      "       FROM {0} mc JOIN {1} m ON m.mission_id = mc.mission_id"
      "       WHERE m.status IN ('{2}','{3}') AND mc.commodity <> ''"
      "       GROUP BY mc.commodity) n"
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
  // slownik towarow i zapasy siedza w bazie live, misje w glownej - dlatego jedno zapytanie przez oba
  return sqlite::select_from<info::supply_option_t>(
    db_->db,
    std::format(
      "(SELECT i.market_id AS market_id,"
      " coalesce(st.name,'') AS station,"
      " coalesce(st.station_type,'') AS station_type,"
      " coalesce(ss.name,'') AS system,"
      " c.name AS commodity,"
      " n.needed AS needed,"
      " i.stock AS stock,"
      " i.buy_price AS buy_price"
      " FROM (SELECT mc.commodity AS commodity, sum(mc.count) AS needed"
      "       FROM {0} mc JOIN {1} m ON m.mission_id = mc.mission_id"
      "       WHERE m.status IN ('{2}','{3}') AND mc.commodity <> ''"
      "       GROUP BY mc.commodity) n"
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
  // jeden wiersz na pare frakcja/system, przesuwany do przodu przy kazdym odczycie systemu
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
  // obecne sa te, ktore widzielismy przy najswiezszym odczycie tego systemu
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

auto database_storage_t::load_trade_options(uint64_t market_id, unsigned limit, bool bring_here)
  -> expected_ec<std::vector<info::trade_option_t>>
  {
  // rynek "here" to ten w ktorym stoimy, "other" to dowolny inny ktory kiedys widzielismy.
  // kierunek decyduje tylko o tym, ktora strona kupuje a ktora sprzedaje
  std::string_view const buy_side{bring_here ? "other" : "here"};
  std::string_view const sell_side{bring_here ? "here" : "other"};
  // kurs na jedna tone nie jest kursem - ponizej tego progu podpowiedz tylko zasmieca ekran
  constexpr unsigned minimum_quantity{50u};

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
      "   AND {7}.demand >= {8} AND {7}.sell_price > {6}.buy_price"
      // ladownia ma skonczona pojemnosc, wiec o zarobku decyduje marza na tonie, nie procent
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
      minimum_quantity
    ),
    ""
  );
  }

auto database_storage_t::reopen_mission(uint64_t mission_id, std::chrono::sys_seconds expiry) -> expected_ec<void>
  {
  // zdarzenie Missions to zdjecie z chwili startu gry; odtworzone pozniej zamyka misje wziete po nim,
  // a MissionAccepted jest swiadectwem mocniejszym - mowi wprost, ze w tej chwili misja byla otwarta
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
  // kwota z MissionAccepted to oferta, dopiero MissionCompleted mowi ile faktycznie wplynelo
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

  // pusta lista tez niesie informacje - znaczy ze gra nie ma juz zadnej otwartej misji
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
  // skan nalezy do postaci, nie do systemu - dlatego osobna tabela w bazie glownej
  std::string query{std::format(
    "INSERT INTO {0} (system_address, fss_complete) VALUES ({1}, 1)"
    " ON CONFLICT(system_address) DO UPDATE SET fss_complete = 1",
    sql_iface::tables::system_progress,
    system_address
  )};
  return sqlite::execute_query_no_result(db_->db, query);
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

    // stan z pamieci moze juz nosic slad mapowania - ten nalezy do postaci, nie do ciala
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
  // klucz to system i numer ciala z gry, a nie oid - ten zmienia sie przy kazdej przebudowie galaxy
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
  uint64_t system_address, events::body_id_t body_id, std::string_view genus, std::string_view species, bool sampled
) -> expected_ec<void>
  {
  auto body_oid{oid_for_body(system_address, body_id)};
  if(not body_oid) [[unlikely]]
    return cxx23::unexpected{body_oid.error()};

  // probka moze przyjsc dla ciala, ktorego jeszcze nie zmapowalismy
  if(not *body_oid)
    return {};

  // gatunek rosnie tam niezaleznie od tego, kto go probkowal
  std::string query{std::format(
    "UPDATE {} SET species='{}' WHERE ref_body_oid={} AND genus='{}'",
    sql_iface::tables::genus,
    sqlite::escape_sql_quotes(species),
    **body_oid,
    sqlite::escape_sql_quotes(genus)
  )};
  if(auto res{sqlite::execute_query_no_result(db_->db, query)}; not res) [[unlikely]]
    return res;

  if(not sampled)
    return {};

  // znacznika probki nigdy nie zdejmujemy - kolejny Log tego samego rodzaju nie cofa pobrania
  std::string progress{std::format(
    "INSERT INTO {0} (system_address, body_id, genus, sampled) VALUES ({1}, {2}, '{3}', 1)"
    " ON CONFLICT(system_address, body_id, genus) DO UPDATE SET sampled = 1",
    sql_iface::tables::genus_progress,
    system_address,
    body_id,
    sqlite::escape_sql_quotes(genus)
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
  if(boid_oid)
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

  // zrodla opisuja miejsce roznie - Docked zna typ stacji, ApproachSettlement ekonomie osady.
  // puste pole nie kasuje tego co juz wiemy, ale niepuste nadpisuje: ukonczona konstrukcja
  // zmienia nazwe i journal jest jedynym zrodlem prawdy o tym, jak nazywa sie teraz
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

auto database_storage_t::load_carrier_stock(std::string_view carrier_id)
  -> expected_ec<std::vector<info::carrier_stock_t>>
  {
  // liczy sie ostatni odczyt, wczesniejsze sa historia sprzedazy
  std::string const where{std::format(
    " WHERE c.carrier_id = '{}' AND m.timestamp = (SELECT max(timestamp) FROM {} WHERE carrier_id = c.oid)"
    " ORDER BY r.category, r.localised",
    sqlite::escape_sql_quotes(carrier_id),
    sql_iface::tables::carrier_materials
  )};

  return sqlite::select_from<info::carrier_stock_t>(
    db_->db,
    std::format(
      "{} m JOIN {} c ON c.oid = m.carrier_id LEFT JOIN {} r ON r.id = m.material_id",
      sql_iface::tables::carrier_materials,
      sql_iface::tables::carrier,
      sql_iface::tables::micro_resource
    ),
    where
  );
  }

auto database_storage_t::load_mission_stats(std::chrono::sys_seconds since, uint64_t system_address)
  -> expected_ec<std::vector<info::mission_stat_t>>
  {
  // zawezenie do systemu idzie przez stacje w ktorej misja zostala wzieta,
  // kazde zrodlo ma wlasny alias stacji wiec warunek budujemy osobno dla kazdego
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
  // ekonomia miejsca przychodzi ze stacji, wiec pojedyncze zdobycze zyskuja ja wstecz.
  // liczona jest raz, jednym przebiegiem po historii - to samo per wiersz kosztowalo sekunde
  // "Mostly from" idzie za wybranym okresem - inaczej mowiloby o miejscu sprzed pol roku
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
  // odtwarzanie journala powtarza zdobycze, rozroznia je czas, miejsce i material
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
  // przebudowa z journali powtarza kazda misje - rozroznia je misja, frakcja i system
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
  // odczyt systemu obejmuje wszystkie obecne frakcje naraz, wiec najswiezszy wpis dowolnej z nich
  // mowi kiedy ostatnio na ten system patrzylismy
  // bez agregatu - przy nieodwiedzonym systemie ma nie byc zadnego wiersza, a nie wiersz z NULL
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
///\brief najdluzsza przerwa miedzy zmianami, ktora jeszcze uchodzi za te sama fale przeliczenia
///
/// Galaktyka przelicza sie systemami i rozjazd miedzy nimi siega godzin, wiec fala musi miec
/// luz. Z drugiej strony kolejna przychodzi zwykle po dobie, a w weekend po dwoch, wiec prog
/// w okolicach polowy doby rozdziela je pewnie
constexpr std::chrono::hours same_wave_gap{8};

///\brief o tyle przesuwamy poczatek fali wstecz wzgledem tego, co zobaczylismy
///
/// Odczyt ze stara wartoscia nie dowodzi, ze serwer jeszcze nie przeliczyl - dowodzi tylko, ze do
/// nas jeszcze nie doszlo. Widac to w danych: okna potrafia zaczynac sie rowno o pelnej albo
/// polowie godziny, czyli tam, gdzie tick najpewniej naprawde wypadl. Przy oddawaniu misji blad
/// w te strone jest bezpieczny - lepiej uznac, ze doba zamknela sie wczesniej, niz oddac za pozno
constexpr std::chrono::minutes client_lag{5};

///\brief najblizszy czwartek 07:00 UTC po podanej chwili, czyli tygodniowe przeliczenie gry
///
/// %w liczy dni od niedzieli, wiec czwartek to 4. Gdy juz jest czwartek, ale po godzinie, wlasciwy
/// jest dopiero nastepny tydzien
/// modulo zapisane jest pojedynczym znakiem procenta - w std::format nie jest on specjalny,
/// wiec podwojenie trafiloby wprost do SQL i wywrocilo zapytanie
constexpr std::string_view next_weekly_tick{
  "strftime('%Y-%m-%dT%H:%M:%SZ', datetime(date({0}, '+' || ("
  "  CASE WHEN ((4 - CAST(strftime('%w', {0}) AS INTEGER) + 7) % 7) = 0 AND time({0}) >= '07:00:00'"
  "       THEN 7 ELSE ((4 - CAST(strftime('%w', {0}) AS INTEGER) + 7) % 7) END"
  ") || ' days'), '+7 hours'))"
};

///\brief czas po kolonizacji, w ktorym wplywy chodza wlasnym rytmem
///
/// Swiezo skolonizowany system ma wplywy ustawione z gory i do pierwszego **tygodniowego**
/// przeliczenia - czwartek 07:00 UTC, czyli 09:00 czasu lokalnego - albo stoja, albo skacza
/// o ulamek punktu na frakcji glownej. Ani jedno, ani drugie nie jest sladem dobowego ticku
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
      // okno szersze niz kilka godzin powstaje po dluzszej nieobecnosci w systemie i nie mowi nic
      // o porze przeliczenia, a rozbija fale na osobne pozycje - do klastrowania nie wchodzi
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

  // Fale rozdziela przerwa, nie doba kalendarzowa i nie wspolna czesc okien. Okna z roznych
  // systemow nie musza sie przecinac, bo kazdy system przelicza sie u siebie - probowanie ich
  // przeciecia dawalo zbior pusty i gubilo wiekszosc dni
  std::vector<info::tick_fact_t> facts;
  for(auto it{rows->begin()}; it != rows->end();)
    {
    auto const wave_begin{it};
    auto last_end{it->window_end};
    for(; it != rows->end() and it->window_end - last_end <= same_wave_gap; ++it)
      last_end = it->window_end;

    // posortowane po koncu okna, wiec pierwszy element fali przeliczyl sie najwczesniej,
    // a ostatni najpozniej
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
      .start_begin = first.window_begin - client_lag,
      .start_end = first.window_end,
      .end_begin = last.window_begin,
      .end_end = last.window_end,
      .samples = uint32_t(std::distance(wave_begin, it)),
      .systems = uint32_t(touched.size() - size_t(std::ranges::distance(unique_systems)))
    });
    }

  // najswiezsza fala na poczatku - to ona odpowiada na pytanie "kiedy byl ostatni"
  std::ranges::reverse(facts);
  return facts;
  }

auto database_storage_t::last_local_tick(uint64_t system_address, info::tick_kind_e kind)
  -> expected_ec<std::optional<std::chrono::sys_seconds>>
  {
  // wplywy zmieniaja sie przy ticku wplywow, dni wygrane przy ticku wojen - kazdy zegar ma wiec
  // wlasna tabele i wlasna ostatnia zmiane
  auto res{sqlite::select_signle_from<std::chrono::sys_seconds>(
    db_->db,
    std::format(
      "SELECT timestamp FROM {} WHERE system_address={} ORDER BY timestamp DESC LIMIT 1",
      kind == info::tick_kind_e::influence ? sql_iface::tables::faction_influence
                                           : sql_iface::tables::system_conflict,
      system_address
    )
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  return *res;
  }

namespace
  {
///\brief wplyw frakcji wedlug ostatniej probki nie pozniejszej niz podana chwila, w procentach
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

///\brief stan frakcji wedlug tej samej probki, z ktorej czytamy wplyw sprzed fali
[[nodiscard]]
auto state_at(sqlite3 * db, uint64_t system_address, std::string_view faction, std::chrono::sys_seconds when)
  -> std::string
  {
  auto res{sqlite::select_signle_from<std::string>(
    db,
    std::format(
      "SELECT fi.faction_state FROM {0} fi JOIN {1} f ON f.oid = fi.faction_oid"
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

  // "None" to brak stanu, a nie stan o nazwie None - w tabeli ma zostac pusto
  return **res == "None" ? std::string{} : **res;
  }

///\brief pierwsza zmiana wplywow w systemie po podanej chwili, dowolnej frakcji
///
/// Sluzy za dowod, ze po fali naprawde tam bylismy. Brak takiej zmiany znaczy albo ze nie bylismy,
/// albo ze nic sie nie ruszylo - w obu wypadkach doby nie wolno rozliczyc
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
///\brief surowy plus z misji razem z opisem systemu - nazwy pol musza zgadzac sie z aliasami
/// zapytania, a sama struktura potrzebuje wiazania zewnetrznego, bo refleksja glaze nie siega
/// do przestrzeni anonimowej
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

  // fale przychodza od najswiezszej, a granice dob wygodniej szukac rosnaco. Pusta lista nie jest
  // bledem - na bazie jeszcze nieprzebudowanej zadna fala nie zostala wykryta, a praca mimo to
  // zostala wykonana i ma sie pokazac, tyle ze w calosci jako doba jeszcze nierozliczona
  std::vector<info::tick_fact_t> ordered{*waves};
  std::ranges::reverse(ordered);

  // Okres liczony od ostatniej zapisanej pracy, nie od zegara - baza bywa starsza niz dzis, a pusty
  // raport nie powiedzialby, czy pracy nie bylo, czy tylko jest sprzed tygodnia
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

  // Doba BGS nie konczy sie na granicy okresu, wiec ciecie rowno w niej zabraloby czesc plusow
  // najstarszej doby i zaniziloby jej przelicznik - ta sama doba wygladalaby inaczej przy wyborze
  // 7 i 14 dni. Zamiast tego cofamy sie do przeliczenia, ktore te dobe otworzylo
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
    // misja oddana przed fala liczy sie do doby, ktora ta fala zamyka. Wpadniecie w samo okno fali
    // jest nierozstrzygalne, wiec idzie do doby zamykanej - tam misja jeszcze najpewniej zdazyla
    auto const closing{std::ranges::find_if(ordered, [&](info::tick_fact_t const & w)
                                            { return w.start_end >= row.timestamp; })};

    // po ostatniej fali siedzi doba jeszcze nierozliczona - zerowy znacznik mowi "trwa"
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

    // doby nierozliczonej nie ma czym zamknac, a reszcie dokladamy wplyw z obu stron fali
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

        // wartosc po fali wolno pokazac tylko gdy mamy dowod, ze po niej tam bylismy
        if(auto seen{first_change_after(db_->db, bucket.effort.system_address, closing->end_end)}; seen)
          bucket.effort.influence_after
            = influence_at(db_->db, bucket.effort.system_address, bucket.effort.faction, *seen);
        }
      }

    result.push_back(std::move(bucket.effort));
    }

  // Przyrost wplywow dzieli sie miedzy frakcje pchane tej samej doby w tym samym systemie, bo
  // procenty sumuja sie do stu. Koszt punktu jest wiec wielkoscia systemu i doby, a nie frakcji;
  // udzial pojedynczej frakcji wynika z tego, jaka czesc calej pracy w gore na nia poszla
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

    // bez odczytu po obu stronach fali nie wiadomo, ile ta frakcja wziela z puli, a wtedy nie da
    // sie uczciwie rozdzielic reszty - caly podzial tej doby przepada
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

  // Najswiezsze doby na gorze, a w obrebie doby najwieksza praca pierwsza. Doba jeszcze
  // nierozliczona jest najswiezsza z mozliwych, a jej znacznikiem jest zero - bez podmiany
  // ladowalaby na samym koncu, czyli najdalej od tego, co robimy teraz
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
  // jedna zapamietana trasa - wczytanie nowej zastepuje poprzednia w calosci
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
  // przebudowa z journali powtarza kazde zamowienie - rozroznia je statek i chwila zamowienia
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

auto database_storage_t::store(info::port_visit_t const & value) -> expected_ec<void>
  {
  // Jeden wiersz na port, znacznik przesuwany przy kazdym kolejnym postoju - a postoje sie
  // powtarzaja, wiec zwykly INSERT wywalalby sie na kluczu i zostawial date pierwszej wizyty
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
    db_->db, sql_iface::tables::port_visit, " ORDER BY visited DESC LIMIT 1"
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::port_visit_t>{};

  return std::optional<info::port_visit_t>{std::move((*res)[0])};
  }

auto database_storage_t::load_bgs_systems() -> expected_ec<std::vector<info::system_ref_t>>
  {
  // BGS robi sie tam, gdzie sie oddaje misje - lista bierze sie z samej pracy, bez osobnego ustawienia
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
  // oba znaczniki sa ograniczeniami z jednej strony: wojna ruszyla po ostatnim "pending"
  // i nie pozniej niz pierwszy "active", a ile z tego to opoznienie gry, a ile nasza nieobecnosc,
  // widac dopiero po szerokosci tego przedzialu
  return sqlite::select_from<info::war_onset_t>(
    db_->db,
    std::format(
      // te same frakcje bija sie ze soba wiecej niz raz, wiec kazda zapowiedz szuka najblizszego
      // po niej przejscia w stan wojny, a nie najwczesniejszego w calej historii tej pary
      // wynik dopinany na koncu, z najswiezszego odczytu tej samej wojny - dopiero on mowi,
      // kto ja wygral i jakim stosunkiem dni
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

  // baza trzyma cala historie, a odliczac mozna tylko z najswiezszego stanu kazdej pary
  std::map<std::pair<std::string, std::string>, info::conflict_t const *> latest;
  for(info::conflict_t const & conflict: *conflicts)
    {
    auto & slot{latest[{conflict.faction1, conflict.faction2}]};
    if(slot == nullptr or slot->timestamp < conflict.timestamp)
      slot = &conflict;
    }

  ///\brief tyle wygranych dni rozstrzyga konflikt
  constexpr uint32_t days_to_win{4};

  std::vector<info::war_countdown_t> result;
  for(auto const & [pair, entry]: latest)
    {
    info::conflict_t const & conflict{*entry};

    // pusty status znaczy, ze konflikt juz sie zamknal - nie ma czego odliczac
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
      // zapowiedziany potrzebuje jeszcze jednego przeliczenia, zeby w ogole ruszyc
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

  // fale ida od najswiezszej, wiec przerwa dzieli sasiadow na liscie
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

auto database_storage_t::load_stations(uint64_t system_address) -> expected_ec<std::vector<info::station_t>>
  {
  return sqlite::select_from<info::station_t>(
    db_->db, sql_iface::tables::station, std::format(" WHERE system_address={} ORDER BY name", system_address)
  );
  }

auto database_storage_t::load_market_entries(uint64_t market_id) -> expected_ec<std::vector<info::market_entry_t>>
  {
  // nazwy pol market_entry_t pokrywaja sie z kolumnami obu tabel, wiec zlaczenie idzie
  // przez ten sam generator zapytan co zwykly odczyt
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
  // slownik towarow jest wspolny dla wszystkich rynkow, dopisujemy tylko nieznane
  for(info::commodity_t const & value: commodities)
    {
    auto known{sqlite::select_signle_from<uint64_t>(
      db_->db, std::format("SELECT count(*) FROM {} WHERE id={}", sql_iface::tables::commodity, value.id)
    )};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};

    if(*known and **known != 0)
      continue;

    if(auto res{sqlite::insert_into<info::commodity_t, true>(db_->db, "id"sv, sql_iface::tables::commodity, value)};
       not res) [[unlikely]]
      return res;
    }

  // rynek zmienia sie ciagle, trzymamy tylko ostatni odczyt
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

  // czas odczytu trzymamy przy rynku, nie przy stacji - stacja jest odtwarzalna, odczyt nie
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
  // ten sam sygnal wraca przy kazdym skanie fss systemu
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
    // sygnal juz znamy, liczy sie kiedy ostatnio zostal zgloszony
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

  // plac budowy czy compromised nav beacon znikaja z systemu, ale wpis po nich zostaje
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

  // reputacja jest osobista, wiec dochodzi z bazy glownej, a nie ze wspolnej wiedzy o frakcjach
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
  // reputacja idzie do bazy osobistej, reszta do wspolnej - nazwa laczy jedno z drugim
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

  // kazde zrodlo zna inna czesc - id i nazwe czytelna bartender, kategorie sprzedaz
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

auto database_storage_t::store(info::micro_sale_t const & sale, std::span<info::micro_sale_item_t const> items)
  -> expected_ec<void>
  {
  // odtwarzanie journala powtarza te same transakcje, para czas i rynek je rozroznia
  std::string known_query{std::format(
    "SELECT count(*) FROM {} WHERE market_id={} AND timestamp='{:%Y-%m-%dT%H:%M:%SZ}'",
    sql_iface::tables::micro_sale,
    sale.market_id,
    sale.timestamp
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

    // postep tej postaci dochodzi osobno - galaxy nie wie, kto co zeskanowal
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
