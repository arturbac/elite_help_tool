// database_storage_t: factions, influence, ticks, wars and territory
#include "database_storage_impl.h"
#include <territory.h>

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
