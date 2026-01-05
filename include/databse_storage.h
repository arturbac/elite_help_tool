#pragma once
#include <string>
#include <memory>
#include <simple_enum/expected.h>
#include <simple_enum/simple_enum.hpp>
#include <elite_events.h>
#include <elite_data.h>
#include <array>

struct sqlite3_handle_t;

template<typename T>
using expected_ec = cxx23::expected<T, std::error_code>;

struct database_storage_t
  {
  std::string db_path_;
  std::unique_ptr<sqlite3_handle_t> db_;

  explicit database_storage_t(std::string db_path);
  ~database_storage_t();

  [[nodiscard]]
  auto open() -> expected_ec<void>;

  [[nodiscard]]
  auto create_database() -> expected_ec<void>;

  [[nodiscard]]
  auto store(star_system_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto store_fss_complete(uint64_t system_address) -> expected_ec<void>;

  [[nodiscard]]
  auto store_system_location(uint64_t system_address, std::array<double, 3> const & loc) -> expected_ec<void>;

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
  auto faction_oid( std::string_view name ) -> expected_ec<std::optional<uint32_t>>;
  
  [[nodiscard]]
  auto load_faction( std::string_view name )-> expected_ec<std::optional<info::faction_info_t>>;
  
  [[nodiscard]]
  auto update_faction_info(info::faction_info_t const & faction) -> expected_ec<void>;

  [[nodiscard]]
  auto load_system(uint64_t system_address) -> expected_ec<std::optional<star_system_t>>;
  auto close() -> void;
  };
