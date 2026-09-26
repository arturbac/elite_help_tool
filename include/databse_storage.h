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
  ///\brief fakty o galaktyce, wspolne dla wszystkich postaci - systemy, ciala, stacje, frakcje
  ///\detail odtwarzalne z journali dowolnej postaci, bo opisuja swiat a nie gracza. dzieki temu
  /// dwa konta moga wskazywac ten sam plik i dzielic wiedze o Bubble, zachowujac wlasne misje,
  /// reputacje i postep skanowania w bazie glownej
  std::string galaxy_db_path_;
  std::unique_ptr<sqlite3_handle_t> db_;

  explicit database_storage_t(std::string_view db_path);
  ~database_storage_t();

  [[nodiscard]]
  auto open(storage_mode_e mode = storage_mode_e::live) -> expected_ec<void>;

  [[nodiscard]]
  auto create_database() -> expected_ec<void>;

  ///\brief dokłada brakujace kolumny do live.sqlite, ktorej nie da sie odtworzyc z journali
  [[nodiscard]]
  auto migrate_live_schema() -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::mission_t const & value) -> expected_ec<void>;
  
  [[nodiscard]]
  auto mission_exists(uint64_t mission_id) ->expected_ec<bool>;
  
  [[nodiscard]]
  auto load_missions() -> expected_ec<std::vector<info::mission_t>>;
  
  [[nodiscard]]
  auto change_mission_status(
    uint64_t mission_id, info::mission_status_e const status, std::chrono::sys_seconds when
  ) -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::mission_cargo_t const & value) -> expected_ec<void>;

  ///\brief ile czego trzeba przywiezc lacznie dla otwartych misji
  [[nodiscard]]
  auto load_cargo_needs() -> expected_ec<std::vector<info::cargo_need_t>>;

  ///\brief rynki, ktore dany towar wytwarzaja, nawet gdy akurat nie maja go na stanie
  [[nodiscard]]
  auto load_producers() -> expected_ec<std::vector<info::supply_option_t>>;

  ///\brief gdzie da sie to kupic w znanych nam rynkach, z zapasem pokrywajacym potrzebe
  [[nodiscard]]
  auto load_supply_options() -> expected_ec<std::vector<info::supply_option_t>>;

  ///\brief kursy handlowe wzgledem tego rynku, liczone po znanych nam innych rynkach
  ///\param bring_here true - kupic gdzie indziej i sprzedac tutaj; false - kupic tutaj i wywiezc
  [[nodiscard]]
  auto load_trade_options(uint64_t market_id, unsigned limit, bool bring_here)
    -> expected_ec<std::vector<info::trade_option_t>>;

  ///\brief MissionAccepted jest dowodem ze misja jest otwarta, nawet gdy wpis juz istnieje
  [[nodiscard]]
  auto reopen_mission(uint64_t mission_id, std::chrono::sys_seconds expiry) -> expected_ec<void>;

  ///\brief zamkniecie misji razem z kwota ktora gra naprawde wyplacila
  [[nodiscard]]
  auto complete_mission(uint64_t mission_id, std::chrono::sys_seconds when, uint64_t reward) -> expected_ec<void>;

  ///\brief zdarzenie Missions wylicza wszystko co gra uwaza za otwarte - reszta juz sie zamknela bez nas
  [[nodiscard]]
  auto expire_missions_outside(std::span<uint64_t const> active, std::chrono::sys_seconds when) -> expected_ec<void>;

  [[nodiscard]]
  auto redirect_mission(uint64_t mission_id, std::string_view system, std::string_view station, std::string_view settlment)-> expected_ec<void>;
  
  [[nodiscard]]
  auto carrier_oid( std::string_view name ) -> expected_ec<std::optional<int64_t>>;
  
  [[nodiscard]]
  auto load_carrier(std::string_view carrier_id) -> expected_ec<std::optional<info::carrier_t>>;

  [[nodiscard]]
  auto update_carrier(info::carrier_t const & value) -> expected_ec<void>;

  ///\brief uzupelnia slownik mikrozasobow - kazde zrodlo wnosi inna czesc wiedzy
  [[nodiscard]]
  auto store(info::micro_resource_t const & value) -> expected_ec<void>;

  ///\brief zapisuje transakcje sprzedazy mikrozasobow, pomijajac juz znane
  [[nodiscard]]
  auto store(info::micro_sale_t const & sale, std::span<info::micro_sale_item_t const> items) -> expected_ec<void>;

  ///\brief zapisuje zdobyty mikrozasob, pomijajac juz znane
  [[nodiscard]]
  auto store(info::micro_acquisition_t const & value) -> expected_ec<void>;

  ///\brief stan polki bartendera z ostatniego odczytu, wraz z jego czasem
  [[nodiscard]]
  auto load_carrier_stock(std::string_view carrier_id) -> expected_ec<std::vector<info::carrier_stock_t>>;

  ///\brief flotowce ktore widzielismy, wlasny pierwszy
  [[nodiscard]]
  auto load_carriers() -> expected_ec<std::vector<info::carrier_t>>;

  ///\brief ukonczone misje per frakcja od podanej chwili
  ///\detail system_address rozne od zera zaweza do misji wzietych w tym systemie
  [[nodiscard]]
  auto load_mission_stats(std::chrono::sys_seconds since, uint64_t system_address)
    -> expected_ec<std::vector<info::mission_stat_t>>;

  ///\brief zdobycze zsumowane per material, z podzialem na sposob pozyskania
  [[nodiscard]]
  auto load_acquisition_summary(std::chrono::sys_seconds since)
    -> expected_ec<std::vector<info::acquisition_summary_t>>;

  ///\brief wplyw oddanej misji na jedna frakcje w jednym systemie, z pominieciem juz znanych
  [[nodiscard]]
  auto store(info::mission_influence_t const & value) -> expected_ec<void>;

  ///\brief kiedy ostatnio patrzylismy na ten system - dowolna frakcja, bo odczyt obejmuje wszystkie
  [[nodiscard]]
  auto last_system_seen(uint64_t system_address) -> expected_ec<std::optional<std::chrono::sys_seconds>>;

  ///\brief slad po ticku, z pominieciem juz znanych okien
  [[nodiscard]]
  auto store(info::tick_observation_t const & value) -> expected_ec<void>;

  ///\brief ostatnie zaobserwowane ticki danego rodzaju, od najswiezszego
  ///\detail okna z jednej doby sa przecinane - kazdy odwiedzony system zawezza wynik
  [[nodiscard]]
  auto load_recent_ticks(info::tick_kind_e kind, uint32_t within_days)
    -> expected_ec<std::vector<info::tick_fact_t>>;

  ///\brief przeliczenia zaobserwowane w jednym systemie, od najswiezszego
  ///\detail sasiednie systemy przelicza sie o roznych porach, wiec to jest ta pora, ktora obowiazuje
  /// przy oddawaniu misji akurat tutaj - globalna fala mowi tylko, w jakim zakresie szukac
  [[nodiscard]]
  auto load_system_ticks(uint64_t system_address, info::tick_kind_e kind, uint32_t within_days)
    -> expected_ec<std::vector<info::tick_observation_t>>;

  ///\brief jak regularnie przeliczenie przychodzi - z tych samych fal, co load_recent_ticks
  [[nodiscard]]
  auto load_tick_stats(info::tick_kind_e kind, uint32_t within_days) -> expected_ec<info::tick_stats_t>;

  ///\brief praca w plusach zestawiona z ruchem wplywow, doba BGS po dobie
  ///\detail doby rozdzielaja wykryte fale przeliczen, nie stala godzina - ta przesuwa sie co kilka
  /// dni i w weekend potrafi nie przyjsc wcale. system_address rozne od zera zaweza do jednego systemu
  [[nodiscard]]
  auto load_bgs_effort(uint32_t within_days, uint64_t system_address)
    -> expected_ec<std::vector<info::bgs_effort_t>>;

  ///\brief ile jeszcze przeliczen wojny do rozstrzygniecia kazdego trwajacego konfliktu
  [[nodiscard]]
  auto load_war_countdown(uint64_t system_address) -> expected_ec<std::vector<info::war_countdown_t>>;

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
  ///\brief tozsamosc frakcji idzie do wspolnej galaxy, reputacja do bazy osobistej
  ///\detail with_reputation=false przy imporcie cudzego journala: swiat bierzemy, reputacje nie
  [[nodiscard]]
  auto update_faction_info(info::faction_info_t const & faction, bool with_reputation = true) -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::faction_influence_t const & value) -> expected_ec<void>;

  /// ostatni zarejestrowany wpis influence dla pary frakcja/system
  [[nodiscard]]
  auto last_influence(int64_t faction_oid, uint64_t system_address)
    -> expected_ec<std::optional<info::faction_influence_t>>;

  ///\brief odnotowuje ze frakcja byla w systemie przy tym odczycie, nawet gdy nic sie nie zmienilo
  [[nodiscard]]
  auto store_faction_seen(int64_t faction_oid, uint64_t system_address, std::chrono::sys_seconds when)
    -> expected_ec<void>;

  ///\brief frakcje obecne przy najswiezszym odczycie systemu - reszta juz z niego wyleciala
  [[nodiscard]]
  auto load_present_factions(uint64_t system_address) -> expected_ec<std::vector<info::faction_ref_t>>;

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

  ///\brief tozsamosc stacji, odtwarzalna z journali
  [[nodiscard]]
  auto store(info::station_t const & value) -> expected_ec<void>;

  ///\brief czas ostatniego odczytu rynku, z bazy zbieranej na zywo
  [[nodiscard]]
  auto load_market_info(uint64_t market_id) -> expected_ec<std::optional<info::market_info_t>>;

  [[nodiscard]]
  auto load_station(uint64_t market_id) -> expected_ec<std::optional<info::station_t>>;

  ///\brief stacja po nazwie widzianej w sygnale systemu
  [[nodiscard]]
  auto load_station(uint64_t system_address, std::string_view name) -> expected_ec<std::optional<info::station_t>>;

  ///\brief konto do ktorego nalezy ta baza osobista
  [[nodiscard]]
  auto store_owner(info::db_owner_t const & owner) -> expected_ec<void>;

  [[nodiscard]]
  auto load_owner() -> expected_ec<std::optional<info::db_owner_t>>;

  ///\brief stacje systemu znane z journali - dokowania, rynkow, celow misji
  ///\detail sygnal skanera potrafi nie wspomniec o osadzie ani o porcie, ktory dopiero stanal,
  /// a stacja w ktorej stanelismy jest swiadectwem mocniejszym niz brak sygnalu
  [[nodiscard]]
  auto load_stations(uint64_t system_address) -> expected_ec<std::vector<info::station_t>>;

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
