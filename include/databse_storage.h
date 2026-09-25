#pragma once
#include <string>
#include <memory>
#include <simple_enum/expected.h>
#include <simple_enum/simple_enum.hpp>
#include <elite_events.h>
#include <elite_data.h>
#include <array>
#include <span>

struct sqlite3_handle_t;

template<typename T>
using expected_ec = cxx23::expected<T, std::error_code>;

///\brief tryb pracy bazy, decyduje o kompromisie trwalosc/szybkosc
enum struct storage_mode_e : uint8_t
  {
  ///\brief praca na zywo - kazdy zapis we wlasnej transakcji, ustawienia domyslne sqlite
  live,
  ///\brief budowanie bazy od zera z calosci logow, przy awarii i tak powtarzamy import
  bulk_import
  };

consteval auto adl_enum_bounds(storage_mode_e)
  {
  using enum storage_mode_e;
  return simple_enum::adl_info{live, bulk_import};
  }

struct database_storage_t
  {
  std::string db_path_;
  ///\brief dane zbierane wylacznie na zywo - rynki stacji i bartender flotowca
  ///\detail ich zrodlem sa pliki Market.json i FCMaterials.json, nadpisywane przez gre, wiec z
  /// journali nie da sie ich odtworzyc; przebudowa bazy glownej ich nie rusza, a journal_tailer
  /// zaklada ten plik tylko gdy go nie ma
  std::string live_db_path_;
  std::unique_ptr<sqlite3_handle_t> db_;

  explicit database_storage_t(std::string_view db_path);
  ~database_storage_t();

  [[nodiscard]]
  auto open(storage_mode_e mode = storage_mode_e::live) -> expected_ec<void>;

  [[nodiscard]]
  auto create_database() -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::mission_t const & value) -> expected_ec<void>;
  
  [[nodiscard]]
  auto mission_exists(uint64_t mission_id) ->expected_ec<bool>;
  
  [[nodiscard]]
  auto load_missions() -> expected_ec<std::vector<info::mission_t>>;
  
  [[nodiscard]]
  auto change_mission_status(uint64_t mission_id, info::mission_status_e const status)-> expected_ec<void>;
  
  [[nodiscard]]
  auto redirect_mission(uint64_t mission_id, std::string_view system, std::string_view station, std::string_view settlment)-> expected_ec<void>;
  
  [[nodiscard]]
  auto carrier_oid( std::string_view name ) -> expected_ec<std::optional<int64_t>>;
  
  [[nodiscard]]
  auto load_carrier(std::string_view carrier_id) -> expected_ec<std::optional<info::carrier_t>>;

  [[nodiscard]]
  auto update_carrier(info::carrier_t const & value) -> expected_ec<void>;

  ///\brief dopisuje nieznany material bartendera do slownika
  [[nodiscard]]
  auto store(info::micro_resource_t const & value) -> expected_ec<void>;
  
  [[nodiscard]]
  auto store(info::fcmaterial_t const & value) -> expected_ec<void>;
  
  [[nodiscard]]
  auto store(star_system_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto store_fss_complete(uint64_t system_address) -> expected_ec<void>;

  [[nodiscard]]
  auto store_system_location(uint64_t system_address, std::array<double, 3> const & loc) -> expected_ec<void>;

  /// opis systemu - ekonomia, rzad, przynaleznosc, bezpieczenstwo, populacja, frakcja kontrolujaca
  [[nodiscard]]
  auto update_system_info(star_system_t const & system) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, bary_centre_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, body_t const & value) -> expected_ec<uint64_t>;

  [[nodiscard]]
  auto store_dss_complete(uint64_t system_address, events::body_id_t body_id) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t ref_body_oid, events::signal_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t ref_body_oid, events::genus_t const & value) -> expected_ec<void>;

  /// gatunek dopisywany do rodzaju znanego z mapowania, po pobraniu probki
  [[nodiscard]]
  auto store_genus_species(
    uint64_t system_address,
    events::body_id_t body_id,
    std::string_view genus,
    std::string_view species,
    bool sampled
  ) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, ring_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto oid_for_body(uint64_t system_address, events::body_id_t body_id) -> expected_ec<std::optional<uint64_t>>;

  [[nodiscard]]
  auto store(uint64_t system_address, events::body_id_t body_id, std::span<events::signal_t const> value)
    -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, events::body_id_t body_id, std::span<events::genus_t const> value)
    -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, std::span<ring_t const> value) -> expected_ec<void>;

  [[nodiscard]]
  auto store_ring_body_id(
    uint64_t system_address,
    events::body_id_t parent_body_id,
    std::string_view ring_name,
    events::body_id_t ring_body_id
  ) -> expected_ec<void>;
  [[nodiscard]]
  auto store(uint64_t ref_body_oid, events::atmosphere_element_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto faction_oid( std::string_view name ) -> expected_ec<std::optional<uint64_t>>;
  
  [[nodiscard]]
  auto load_faction( std::string_view name )-> expected_ec<std::optional<info::faction_info_t>>;
  
  [[nodiscard]]
  auto load_factions() -> expected_ec<std::vector<info::faction_info_t>>;

  [[nodiscard]]
  auto update_faction_info(info::faction_info_t const & faction) -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::faction_influence_t const & value) -> expected_ec<void>;

  /// ostatni zarejestrowany wpis influence dla pary frakcja/system
  [[nodiscard]]
  auto last_influence(int64_t faction_oid, uint64_t system_address)
    -> expected_ec<std::optional<info::faction_influence_t>>;

  /// cala historia influence w systemie, wszystkie frakcje, rosnaco wg czasu
  [[nodiscard]]
  auto load_influence_history(uint64_t system_address) -> expected_ec<std::vector<info::faction_influence_t>>;

  /// systemy dla ktorych mamy zarejestrowana historie influence
  [[nodiscard]]
  auto load_systems_with_influence() -> expected_ec<std::vector<info::system_ref_t>>;

  ///\brief sygnal systemu, pomija powtorzenia tej samej nazwy w tym samym systemie
  [[nodiscard]]
  auto store(system_signal_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto load_system_signals(uint64_t system_address) -> expected_ec<std::vector<system_signal_t>>;

  ///\brief stacja z rynkiem, wpis zakladany przy pierwszym spotkaniu i potem aktualizowany
  [[nodiscard]]
  auto store(info::station_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto load_station(uint64_t market_id) -> expected_ec<std::optional<info::station_t>>;

  ///\brief stacja po nazwie widzianej w sygnale systemu
  [[nodiscard]]
  auto load_station(uint64_t system_address, std::string_view name) -> expected_ec<std::optional<info::station_t>>;

  ///\brief zawartosc rynku zlaczona ze slownikiem towarow
  [[nodiscard]]
  auto load_market_entries(uint64_t market_id) -> expected_ec<std::vector<info::market_entry_t>>;

  ///\brief podmienia cala zawartosc rynku na swiezy odczyt i znaczy czas aktualizacji
  [[nodiscard]]
  auto replace_market(
    uint64_t market_id,
    std::chrono::sys_seconds updated,
    std::span<info::commodity_t const> commodities,
    std::span<info::market_item_t const> items
  ) -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::conflict_t const & value) -> expected_ec<void>;

  /// ostatni zarejestrowany stan konfliktu tych dwoch frakcji w systemie
  [[nodiscard]]
  auto last_conflict(uint64_t system_address, std::string_view faction1, std::string_view faction2)
    -> expected_ec<std::optional<info::conflict_t>>;

  /// konflikty w systemie, rosnaco wg czasu
  [[nodiscard]]
  auto load_conflicts(uint64_t system_address) -> expected_ec<std::vector<info::conflict_t>>;

  [[nodiscard]]
  auto load_system(uint64_t system_address) -> expected_ec<std::optional<star_system_t>>;
  auto close() -> void;
  };
