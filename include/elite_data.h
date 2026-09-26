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

///\brief konto do ktorego nalezy ta baza osobista
///\detail zapisywane przy imporcie; GUI czyta to i nie dopisuje kariery cudzej postaci, gdyby
/// ktos zalogowal sie na drugie konto z tego samego profilu gry
struct db_owner_t
  {
  std::string fid;
  std::string name;
  };

///\brief co TA postac zrobila w galaktyce - tego nie wolno dzielic miedzy konta
///\detail swiat jest wspolny, ale skan i mapowanie nalezy do konkretnego commandera. pokazanie
/// jednej postaci, ze cos zmapowala, gdy zrobila to druga, prowadzi wprost do zlej decyzji przy
/// planowaniu lotu - dlatego te tabele zostaja w bazie osobistej i kluczuja sie naturalnie,
/// nazwami i numerami z gry, a nie oid-ami, ktore zmieniaja sie przy kazdej przebudowie galaxy
struct system_progress_t
  {
  uint64_t system_address;
  bool fss_complete;
  };

struct body_progress_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  uint32_t body_id;
  bool mapped;
  bool footfalled;
  };

struct genus_progress_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  uint32_t body_id;
  std::string genus;
  bool sampled;
  };

///\brief reputacja jest osobista, a frakcja wspolna - dlatego kluczem jest nazwa, nie oid frakcji
struct faction_reputation_t
  {
  std::string faction;
  double reputation;
  };

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

///\brief populacja skrocona do rzedu wielkosci - 74k, 9.9M, 2.0B
///
/// Przy porownywaniu systemow liczy sie rzad wielkosci, nie pojedyncze osoby: rozstrzyga, czy
/// system jest czterdziestotysieczny czy czterdziestomilionowy, bo to od tego zalezy, ile pracy
/// kosztuje punkt procentowy wplywow. Pelna liczba zabiera miejsce i nic nie wnosi
[[nodiscard]]
auto format_population(uint64_t value) -> std::string;

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
  ///\brief frakcja wladajaca miejscem - to jej wplywy rosna od oddanych tu misji
  std::string controlling_faction;
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
  ///\brief czy stacja ten towar naprawde wytwarza i skupuje - zerowy zapas to moze byc chwilowa pustka
  bool producer;
  bool consumer;
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
  ///\brief czy stacja ten towar naprawde wytwarza i skupuje
  bool producer;
  bool consumer;
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

  ///\brief stan z ostatniego CarrierStats - puste dopoki go nie widzielismy
  ///
  /// Zdarzenie przychodzi przy dokowaniu i przy zarzadzaniu flotowcem, wiec te liczby sa zawsze
  /// z ostatniej takiej chwili, nie z teraz - stad znacznik czasu obok nich
  std::string carrier_type;
  std::string docking_access;
  uint32_t fuel_level;
  double jump_range_curr;
  double jump_range_max;
  uint32_t total_capacity;
  uint32_t free_space;
  uint32_t cargo;
  uint64_t balance;
  uint64_t available_balance;
  std::chrono::sys_seconds stats_seen;
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

///\brief ile wplywu jedna oddana misja dolozyla jednej frakcji w jednym systemie
///
/// gra liczy to plusami, nie procentami - "+++" znaczy tyle, ze dostala trzy razy tyle co "+",
/// ale ile to punktow procentowych zalezy od systemu i od tego co w tej dobie zrobili inni.
/// Dlatego trzymamy surowa liczbe plusow, a przelicznik na procenty wychodzi dopiero z zestawienia
/// z [[faction_influence_t]] po ticku
struct mission_influence_t
  {
  int64_t oid{-1};
  uint64_t mission_id;
  ///\brief kiedy misja zostala oddana - to ta chwila decyduje do ktorej doby BGS wpadnie
  std::chrono::sys_seconds timestamp;
  std::string faction;
  uint64_t system_address;
  ///\brief ze znakiem: dodatnie gdy frakcja rosnie, ujemne gdy ja ta misja spycha w dol
  int32_t pluses;
  };

///\brief praca wlozona w jedna frakcje w jednym systemie przez jedna dobe BGS, zestawiona z tym
/// co ta doba faktycznie dala
struct bgs_effort_t
  {
  uint64_t system_address;
  std::string system_name;
  ///\brief populacja systemu - bez niej liczba plusow nic nie znaczy
  ///
  /// gra dzieli wplyw misji przez wielkosc systemu, wiec te same 10 plusow daje w systemie
  /// czterdziestomilionowym ulamek tego, co w czterdziestotysiecznym. Przelicznik ma sens wylacznie
  /// w obrebie jednego systemu i nigdy nie wolno go usredniac miedzy systemami
  uint64_t population;
  std::string faction;
  ///\brief fala, ktora te dobe zamknela - plusy oddane przed nia licza sie wlasnie do niej.
  /// Granica jest wykryta, nie wyliczona z godziny, bo tick przesuwa sie co kilka dni
  std::chrono::sys_seconds closed_by;
  ///\brief plusy pchajace frakcje w gore i te spychajace ja w dol, osobno - to dwie rozne dzwignie
  int32_t pushed_up;
  int32_t pushed_down;
  int32_t missions;
  ///\brief wplyw z ostatniej probki przed zamykajaca fala i z pierwszej po niej, w procentach.
  /// Puste gdy nie bylo nas w systemie po jednej ze stron - wtedy tej doby nie da sie rozliczyc
  /// i nie wolno jej dopowiadac
  std::optional<double> influence_before;
  std::optional<double> influence_after;

  ///\brief stan frakcji z odczytu sprzed zamykajacej fali, czyli ten obowiazujacy w tej dobie
  ///
  /// Bez niego przelicznik bywa nieczytelny, bo stan zmienia obie strony rownania. Najmocniej
  /// **Retreat**: misje dla frakcji w odwrocie sa znacznie skuteczniejsze, a przy tym traci ona
  /// okolo dwoch punktow procentowych na dobe - zmierzony przyrost jest wiec tym, co zostalo po
  /// odjeciu tego odplywu, a prawdziwa skutecznosc pracy byla jeszcze wyzsza
  std::string faction_state;

  ///\brief cala praca w gore wlozona tej doby w ten system, po wszystkich frakcjach razem
  ///
  /// Wplyw jest udzialem procentowym, wiec frakcje pchane tego samego dnia dziela miedzy siebie
  /// jeden przyrost, a nie dostaja dwoch niezaleznych. Liczenie przelicznika osobno dla kazdej
  /// zawyza go tym bardziej, im wiecej frakcji robiono naraz - dlatego koszt punktu jest
  /// wielkoscia systemu, nie frakcji.
  ///
  /// **Podzial pracy nie jest podzialem przyrostu.** Te same piec punktow podnosi frakcje lezaca
  /// na dnie znacznie mocniej niz taka, ktora ma juz dziewiecdziesiat procent - bo procenty licza
  /// sie wzgledem sumy, a ta u gory jest juz prawie cala jej wlasna. Udzial w pracy mowi wiec, ile
  /// wysilku gdzie poszlo, a nie ile punktow procentowych z tego wyjdzie; o tym decyduje jeszcze
  /// to, gdzie frakcja stoi w stawce - dlatego przy kazdym wierszu widac jej wplyw sprzed fali
  int32_t system_pushed_up;
  ///\brief laczny przyrost wplywow frakcji pchanych tej doby w gore, w punktach procentowych.
  /// Puste takze wtedy, gdy ktorejkolwiek z nich brakuje odczytu - podzial musi obejmowac calosc
  /// albo nie ma go wcale.
  ///
  /// Koszt punktu policzony z tego jest srednia po tym, kogo akurat tej doby pchano: dzien pracy
  /// dla frakcji z dolu stawki wyjdzie taniej niz ten sam wysilek wlozony w lidera systemu
  std::optional<double> system_gain;
  };

///\brief jak pozno po zapowiedzi wojna naprawde ruszyla
///
/// Panel wsparcia frakcji pokazuje przejscie w stan wojny od razu, ale do journala nie trafia nic -
/// jedynym sladem jest status konfliktu przy kolejnym odczycie systemu. Dlatego oba znaczniki sa
/// ograniczeniami, nie chwilami: wojna ruszyla gdzies miedzy nimi, a osady wchodza w stan wojny
/// jeszcze pozniej. Rozrzut z wielu wojen mowi, na kiedy planowac wyprawe
struct war_onset_t
  {
  uint64_t system_address;
  std::string system_name;
  std::string war_type;
  std::string faction1;
  std::string faction2;
  ///\brief ostatni odczyt, w ktorym wojna byla jeszcze tylko zapowiedziana
  std::chrono::sys_seconds pending_last;
  ///\brief pierwszy, w ktorym juz trwala
  std::chrono::sys_seconds active_first;
  ///\brief wynik z ostatniego odczytu tej wojny - dni wygrane przez kazda ze stron
  uint32_t won_days1;
  uint32_t won_days2;
  ///\brief status z ostatniego odczytu; pusty znaczy, ze wojna sie juz skonczyla
  std::string status;
  };

///\brief ile jeszcze zostalo trwajacemu konfliktowi
///
/// Konflikt rozstrzyga sie, gdy jedna ze stron uzbiera cztery wygrane dni - w danych Artura konczy
/// tak 53 z 90 zamknietych konfliktow, reszta to ostatnie odczyty sprzed zniknienia z systemu.
/// Dzieki temu koniec wojny daje sie odliczyc z samego won_days, bez znajomosci pory przeliczenia;
/// pora mowi juz tylko, o ktorej tego dnia
struct war_countdown_t
  {
  uint64_t system_address;
  std::string war_type;
  std::string faction1;
  std::string faction2;
  uint32_t won_days1;
  uint32_t won_days2;
  ///\brief zero znaczy, ze konflikt rozstrzyga sie najblizszym przeliczeniem wojen - wtedy warto
  /// miec bondy na reku, bo po wygranej ida z premia
  uint32_t ticks_left;
  ///\brief czy konflikt juz trwa - zapowiedziany dopiero sie zacznie i ma pelne cztery dni przed soba
  bool active;
  };

///\brief ktory z dziennych przeliczen gry - to sa dwa osobne zegary
///\detail zwykle chodza razem, ale nie zawsze: 4 sierpnia 2026 wplywy przeliczyly sie o 16:30,
/// a wojny o 14:30, siodmego wplywy o 16:30 a wojny o 11:30
enum struct tick_kind_e : uint8_t
  {
  ///\brief przeliczenie wplywow frakcji
  influence,
  ///\brief przeliczenie dni wygranych w konfliktach
  war
  };

consteval auto adl_enum_bounds(tick_kind_e)
  {
  using enum tick_kind_e;
  return simple_enum::adl_info{influence, war};
  }

///\brief slad po jednym ticku: przedzial miedzy ostatnim odczytem ze stara wartoscia a pierwszym
/// z nowa
///
/// Gra nie oglasza ticku. Jedyne co widac to ze miedzy dwoma spojrzeniami na system wartosc sie
/// zmienila - a to znaczy tyle, ze tick wypadl gdzies w tym przedziale. Im wiecej systemow
/// odwiedzonych blisko siebie w czasie, tym ciasniej przedzialy sie przecinaja
struct tick_observation_t
  {
  int64_t oid{-1};
  tick_kind_e kind;
  uint64_t system_address;
  ///\brief ostatni odczyt, ktory pokazywal jeszcze stara wartosc
  std::chrono::sys_seconds window_begin;
  ///\brief pierwszy odczyt z nowa wartoscia
  std::chrono::sys_seconds window_end;
  };

///\brief jedna fala przeliczenia, zlozona z obserwacji po kolejnych systemach
///
/// Tick nie jest chwila. Galaktyka przelicza sie systemami, sasiednie potrafia sie rozjechac
/// o godziny, wiec przecinanie okien z roznych systemow dawaloby zbior pusty - a nie daje, bo
/// kazdy system ma wlasny moment. Dlatego zamiast jednej godziny trzymamy oba konce fali:
///
/// - **poczatek** jest tym, co liczy sie dla wplywow: misje trzeba oddac przed nim, bo po nim
///   plusy ida juz na nastepna dobe,
/// - **koniec** jest tym, co liczy sie dla wojen: dopiero po nim bondy sprzedaja sie po nowemu.
///
/// Fale nie chodza co 24h - w weekendy potrafi nie byc ticku przez prawie dwie doby, a po
/// aktualizacji gry serwery gubia go zupelnie. Dlatego zadne pole nie jest prognoza
struct tick_fact_t
  {
  tick_kind_e kind;
  ///\brief okno, w ktorym przeliczyl sie pierwszy system - tu fala sie zaczela
  std::chrono::sys_seconds start_begin;
  std::chrono::sys_seconds start_end;
  ///\brief okno, w ktorym przeliczyl sie ostatni - tu fala doszla do konca
  std::chrono::sys_seconds end_begin;
  std::chrono::sys_seconds end_end;
  ///\brief ile obserwacji i ilu roznych systemow zlozylo sie na te fale
  uint32_t samples;
  uint32_t systems;
  };

///\brief jak regularnie przeliczenie w ogole przychodzi
///
/// Odpowiada na pytanie "czy tick dzisiaj byl" inaczej niz przez doliczanie doby: pokazuje typowa
/// i najdluzsza zaobserwowana przerwe, wiec od razu widac, ze w weekend potrafi nie przyjsc
struct tick_stats_t
  {
  tick_kind_e kind;
  uint32_t waves;
  ///\brief mediana i najdluzsza przerwa miedzy poczatkami kolejnych fal
  std::chrono::minutes typical_gap;
  std::chrono::minutes longest_gap;
  ///\brief ile fal widzialo wiecej niz jeden system - tylko one mowia cos o szerokosci propagacji
  uint32_t multi_system_waves;
  ///\brief najszersza zaobserwowana propagacja, od poczatku fali do jej konca
  std::chrono::minutes widest_spread;
  ///\brief mediana szerokosci okna, czyli jak dokladnie to w ogole zmierzono
  ///
  /// Okno to odstep miedzy odczytem ze stara i z nowa wartoscia, wiec rzadsze wizyty w systemie
  /// rozszerzaja je. Tick nie przesuwa sie przez to na wykresie - po prostu wiadomo o nim mniej,
  /// i wlasnie ta liczba o tym mowi
  std::chrono::minutes typical_window;
  };

///\brief przystanek zapisanej trasy neutronowej
///
/// Trasa wyznaczona na zewnatrz (spansh) i wczytana z pliku - gra nie zapisuje jej nigdzie, a
/// NavRoute nadpisuje przy kazdym wyznaczeniu kursu, wiec skoki neutronowe wyznaczane pojedynczo
/// kasowalyby ja bez przerwy. Trasa, ktora lata sie regularnie, ma zostac zapamietana, a ze nie da
/// sie jej odtworzyc z journali, mieszka w bazie zbieranej na zywo
struct neutron_waypoint_t
  {
  int64_t oid{-1};
  ///\brief nazwa trasy, ta sama we wszystkich jej przystankach - zapamietana jest jedna, ta latana
  /// regularnie; trasy jednorazowe zyja tylko do zamkniecia okna i nigdzie nie trafiaja
  std::string route_name;
  ///\brief kolejnosc lotu - juz po ewentualnym odwroceniu, wiec zero to pierwszy skok
  uint32_t position;
  std::string system;
  uint64_t system_address;
  double loc_x;
  double loc_y;
  double loc_z;
  ///\brief czy to gwiazda neutronowa, czyli przystanek na doladowanie
  bool neutron;
  ///\brief odleglosc od poprzedniego przystanku w latach swietlnych
  double distance;
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
