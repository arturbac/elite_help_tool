#pragma once
#include <string>
#include <memory>
#include <simple_enum/expected.h>
#include <simple_enum/simple_enum.hpp>
#include <elite_events.h>

class QSqlDatabase;

struct sqlite3_handle_t;

struct database_storage_t
  {
  std::string db_path_;
  std::unique_ptr<sqlite3_handle_t> db_;

  explicit database_storage_t(std::string db_path);
  ~database_storage_t();

  [[nodiscard]]
  auto open() -> cxx23::expected<void, std::error_code>;

  [[nodiscard]]
  auto create_database() -> cxx23::expected<void, std::error_code>;

  [[nodiscard]]
  auto store(star_system_t const & value) -> cxx23::expected<void, std::error_code>;

  [[nodiscard]]
  auto store_fss_complete(uint64_t system_address) -> cxx23::expected<void, std::error_code>;
  
  [[nodiscard]]
  auto store(uint64_t system_address, bary_centre_t const & value) -> cxx23::expected<void, std::error_code>;

  [[nodiscard]]
  auto store(uint64_t system_address, body_t const & value) -> cxx23::expected<uint64_t, std::error_code>;

  [[nodiscard]]
  auto store_dss_complete(uint64_t system_address, events::body_id_t body_id) -> cxx23::expected<void, std::error_code>;
  
  [[nodiscard]]
  auto store(uint64_t ref_body_oid, events::signal_t const & value) -> cxx23::expected<void, std::error_code>;

  
  [[nodiscard]]
  auto oid_for_body(uint64_t system_address, events::body_id_t body_id) -> cxx23::expected<uint64_t, std::error_code>;
  
  [[nodiscard]]
  auto store(uint64_t system_address, events::body_id_t body_id, std::span<events::signal_t const> value) -> cxx23::expected<void, std::error_code>;
  
  [[nodiscard]]
  auto store(uint64_t ref_body_oid, events::atmosphere_element_t const & value)
    -> cxx23::expected<void, std::error_code>;
    
    
  [[nodiscard]]
  auto load_system(uint64_t system_address) -> cxx23::expected<std::optional<star_system_t>, std::error_code>;
  auto close() -> void;
  };
