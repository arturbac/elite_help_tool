#pragma once

#include <elite_events.h>
#include <databse_storage.h>
#include <vector>

struct database_import_state_t : public generic_state_t
  {
  struct buffered_signal_t
    {
    events::body_id_t body_id;
    std::vector<events::signal_t> signals_;
    std::vector<events::genus_t> genuses_;
    };

  struct state_t
    {
    star_system_t system{};
    ///\brief osada w ktorej jestesmy - zdobyte mikrozasoby dostaja jej market_id
    uint64_t settlement_market_id{};
    std::vector<buffered_signal_t> buffered_signals;
    database_storage_t db_;

    ///\brief konto do ktorego nalezy ta baza - puste znaczy "bierz wszystko"
    ///\detail katalog journali potrafi zawierac zapisy kilku postaci, bo prefix bywa kopiowany.
    /// swiat z nich bierzemy w calosci, bo galaktyka jest wspolna, ale misje, zdobycze, reputacja
    /// i postep skanowania naleza do jednej postaci i zmieszane nie daja sie juz rozdzielic
    std::string owner_fid;
    ///\brief czy biezacy journal nalezy do wlasciciela bazy
    bool personal{true};

    explicit state_t(std::string_view db_path) : db_{db_path} {}
    };

  state_t * state;

  explicit database_import_state_t(std::string_view journal_dir) : generic_state_t{journal_dir} {}

  void handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) override;
  };
