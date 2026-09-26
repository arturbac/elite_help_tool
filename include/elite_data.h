#pragma once
#include <elite_events.h>

namespace info
  {
enum struct government_e : uint8_t
  {
  unknown,
  anarchy,
  communism,
  confederacy,
  cooperative,
  corporate,
  democracy,
  dictatorship,
  feudal,
  patronage,
  prison_colony,
  theocracy,
  engineer,
  private_ownership
  };

consteval auto adl_enum_bounds(government_e)
  {
  using enum government_e;
  return simple_enum::adl_info{unknown, private_ownership};
  }
enum struct allegiance_e : uint8_t
  {
  unknown,
  independent,
  alliance,
  empire,
  federation,
  thargoid,
  guardian
  };

consteval auto adl_enum_bounds(allegiance_e)
  {
  using enum allegiance_e;
  return simple_enum::adl_info{unknown, guardian};
  }
enum struct happiness_e
  {
  unknown,
  elated,
  happy,
  discontented,
  unhappy,
  despondent
  };

consteval auto adl_enum_bounds(happiness_e)
  {
  using enum happiness_e;
  return simple_enum::adl_info{unknown, despondent};
  }

struct faction_info_t
  {
  std::string name;
  int64_t oid{-1};
  double reputation;
  government_e government;
  allegiance_e allegiance;
  happiness_e happiness;

  // assuming same name and skips oid verification
  [[nodiscard]]
  auto operator==(faction_info_t const &) const noexcept -> bool;
  };

[[nodiscard]]
auto to_native(events::faction_info_t && faction) -> faction_info_t;

/// influence jest wartoscia per system, rejestrowana w czasie wg daty eventu
struct faction_influence_t
  {
  int64_t oid{-1};
  int64_t faction_oid{-1};
  uint64_t system_address;
  std::chrono::sys_seconds timestamp;
  double influence;
  std::string faction_state;
  std::string pending_states;
  std::string active_states;
  std::string recovering_states;
  };

///\brief konflikt w systemie zarejestrowany w czasie wg daty eventu
struct conflict_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  std::chrono::sys_seconds timestamp;
  std::string war_type;
  std::string status;
  std::string faction1;
  std::string stake1;
  uint32_t won_days1;
  std::string faction2;
  std::string stake2;
  uint32_t won_days2;

  ///\brief bez oid i czasu, do wykrycia czy stan konfliktu sie zmienil
  [[nodiscard]]
  auto operator==(conflict_t const &) const noexcept -> bool;
  };

[[nodiscard]]
auto to_conflict(uint64_t system_address, std::chrono::sys_seconds timestamp, events::conflict_t const & conflict)
  -> conflict_t;

///\brief nazwy stanow sklejone przecinkiem, do zapisu i pokazania w tabeli
[[nodiscard]]
auto join_states(std::span<events::faction_state_entry_t const> states) -> std::string;

///\brief tozsamosc stacji, jeden wpis na MarketID
///\detail odtwarzalna z journali - zdarzenia Docked i Market - wiec mieszka w bazie glownej
struct station_t
  {
  uint64_t market_id;
  uint64_t system_address;
  std::string name;
  std::string station_type;
  ///\brief ekonomia i rzad miejsca - to one mowia czego tam szukac, nie nazwa osady
  std::string economy;
  std::string government;
  };

///\brief kiedy ostatnio odczytalismy rynek tej stacji
///\detail sama zawartosc pochodzi z Market.json, ktorego nie da sie odtworzyc, wiec i czas
/// odczytu nalezy do bazy zbieranej na zywo
struct market_info_t
  {
  uint64_t market_id;
  std::chrono::sys_seconds updated;
  };

///\brief slownik towarow, mean_price to srednia galaktyczna czyli stala towaru
struct commodity_t
  {
  uint64_t id;
  std::string name;
  std::string category;
  uint32_t mean_price;
  };

///\brief najswiezszy odczyt rynku, jeden wiersz na towar
struct market_item_t
  {
  int64_t oid{-1};
  uint64_t market_id;
  uint64_t commodity_id;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t stock;
  uint32_t demand;
  };

///\brief pozycja rynku juz zlaczona ze slownikiem towarow, do pokazania w oknie
struct market_entry_t
  {
  std::string name;
  std::string category;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t mean_price;
  uint32_t stock;
  uint32_t demand;
  };

///\brief lekka projekcja star_system do list wyboru, nazwy pol musza zgadzac sie z kolumnami
struct system_ref_t
  {
  uint64_t system_address;
  std::string name;
  };

[[nodiscard]]
auto to_influence(
  int64_t faction_oid,
  uint64_t system_address,
  std::chrono::sys_seconds timestamp,
  events::faction_info_t const & faction
) -> faction_influence_t;

enum struct mission_status_e : uint8_t
  {
  accepted,
  redirected,  // done but not delivered and completed
  completed,
  failed,
  abandoned,
  ///\brief gra przestala ja wykazywac jako otwarta, a my nie widzielismy jak sie zamknela
  expired
  };

consteval auto adl_enum_bounds(mission_status_e)
  {
  using enum mission_status_e;
  return simple_enum::adl_info{accepted, expired};
  }

struct mission_t
  {
  uint64_t mission_id;
  mission_status_e status;
  std::chrono::sys_seconds expiry;
  std::string faction;
  std::string type;
  std::string description;
  uint64_t reward;
  ///\brief stacja w ktorej misja zostala wzieta, zero gdy nieznana
  uint64_t market_id;
  ///\brief kiedy misja sie zamknela - bez tego nie da sie liczyc statystyk tygodniowych
  std::chrono::sys_seconds closed;

  std::string target;
  std::string target_type;
  std::string target_faction;

  std::string destination_system;   //": "Anana",
  std::string destination_station;  //": "Yamazaki Base",
  std::string destination_settlement;

  std::string redirected_system;   //": "Anana",
  std::string redirected_station;  //": "Yamazaki Base",
  std::string redirected_settlement;

  uint32_t count;
  uint16_t kill_count;
  uint16_t passenger_count;

  [[nodiscard]]
  auto mission_count() const noexcept
    {
    return std::max<uint32_t>(std::max<uint32_t>(count, kill_count), passenger_count);
    }
  };

using space_location_t = std::array<double, 3>;

struct route_item_t
  {
  std::string system;
  uint64_t system_address;
  /// star position in light years
  space_location_t star_location;
  std::string star_class;
  double distance;
  bool visited;
  };


struct fcmaterial_t
{
  int64_t oid;
  int64_t carrier_id;
  int64_t timestamp;
  uint64_t material_id;
  uint32_t price;
  uint32_t stock;
  uint32_t demand;
};

struct carrier_t
{
  int64_t oid;
  uint64_t market_id;
  std::string carrier_name;
  std::string carrier_id;
  ///\brief flotowiec ktory mnie interesuje - bartendera obcych tez widzimy, ale to tylko tlo
  bool tracked;
};

///\brief slownik mikrozasobow, sklejany z dwoch zrodel o roznej wiedzy
///\detail FCMaterials.json podaje numeryczne id i nazwe czytelna, SellMicroResources kategorie,
/// a wspolnym kluczem jest nazwa wewnetrzna - "$weaponschematic_name;" i "weaponschematic" to ten
/// sam material
struct micro_resource_t
{
  std::string name;
  uint64_t id;
  std::string localised;
  std::string category;
};

///\brief sprzedaz mikrozasobow - zrzut u bartendera na stacji albo dostawa na flotowiec gracza
///\detail zdarzenie journala, wiec odtwarzalne wstecz; o ktory przypadek chodzi mowi typ stacji
struct micro_sale_t
{
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t market_id;
  uint64_t price;
  uint32_t total_count;
};

///\brief zdobyty mikrozasob wraz z miejscem, z ktorego pochodzi
///\detail market_id wskazuje osade, a przez nia jej ekonomie; zero gdy zdobyte poza osada
///\brief skad wzial sie mikrozasob
enum struct acquisition_source_e : uint8_t
{
  ///\brief podniesione w osadzie, porcie danych albo z pojemnika
  collected,
  ///\brief nagroda za misje, trafia wprost do lockera z pominieciem plecaka
  mission_reward
};

consteval auto adl_enum_bounds(acquisition_source_e)
{
  using enum acquisition_source_e;
  return simple_enum::adl_info{collected, mission_reward};
}

struct micro_acquisition_t
{
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t market_id;
  std::string name;
  uint32_t count;
  acquisition_source_e source;
};

///\brief slad "frakcja byla obecna przy tym odczycie systemu"
///
/// influence zapisujemy tylko gdy sie zmienilo, wiec data ostatniego wpisu mowi o ostatniej zmianie,
/// nie o ostatnim widzeniu. Bez osobnego sladu frakcja, ktora wyleciala z systemu, zostaje na liscie
struct faction_presence_t
  {
  int64_t oid{-1};
  int64_t faction_oid;
  uint64_t system_address;
  std::chrono::sys_seconds last_seen;
  };

///\brief lekka projekcja do listy obecnych
struct faction_ref_t
  { int64_t faction_oid; };

///\brief towar wymagany przez misje - osobna tabela, zeby nie ruszac schematu misji
struct mission_cargo_t
  {
  uint64_t mission_id;
  ///\brief nazwa czytelna, taka sama jak w slowniku towarow
  std::string commodity;
  uint32_t count;
  };

///\brief ile czego trzeba przywiezc lacznie, po zsumowaniu otwartych misji
struct cargo_need_t
  {
  std::string commodity;
  uint32_t count;
  };

///\brief miejsce w ktorym da sie kupic to, czego wymaga misja
struct supply_option_t
  {
  uint64_t market_id;
  std::string station;
  std::string station_type;
  std::string system;
  std::string commodity;
  uint32_t needed;
  uint32_t stock;
  uint32_t buy_price;
  };

///\brief czy towaru nie da sie kupic, a jedynie wykopac
///
/// stacja potrafi placic za taki surowiec bardzo dobrze, ale dla handlarza to slepy zaulek -
/// nikt mu go nie sprzeda. Lista jest stala wiedza o grze, wiec siedzi w kodzie, nie w bazie
[[nodiscard]]
auto is_mining_only(std::string_view commodity) noexcept -> bool;

///\brief kurs handlowy: kupic tam, sprzedac tutaj
struct trade_option_t
  {
  uint64_t market_id;
  std::string station;
  std::string system;
  std::string commodity;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t stock;
  uint32_t demand;
  };

///\brief ile i jakich misji zrobilem dla frakcji w danym okresie
struct mission_stat_t
  {
  std::string faction;
  uint32_t missions;
  uint64_t rewards;
  ///\brief najczestszy rodzaj, np Mission_Massacre
  std::string top_type;
  };

///\brief pozycja polki bartendera po zlaczeniu ze slownikiem
struct carrier_stock_t
{
  std::string name;
  std::string localised;
  std::string category;
  uint32_t price;
  uint32_t stock;
  uint32_t demand;
  std::chrono::sys_seconds timestamp;
};

///\brief zdobycze jednego materialu, z podzialem na sposob i miejsce
struct acquisition_summary_t
{
  std::string name;
  std::string localised;
  std::string category;
  uint32_t collected;
  uint32_t from_missions;
  std::string top_economy;
  std::chrono::sys_seconds last_seen;
};

///\brief pozycja transakcji, laczy sie ze slownikiem przez nazwe wewnetrzna
struct micro_sale_item_t
{
  int64_t oid{-1};
  int64_t sale_oid;
  std::string name;
  uint32_t count;
};

constexpr double light_speed_mps = 299'792'458.0;

///\returns distance in Ly
[[nodiscard]]
auto distance(space_location_t const & loc1, space_location_t const & loc2) -> double;

[[nodiscard]]
auto transform_mission_name(std::string_view input) -> std::string;
  }  // namespace info
