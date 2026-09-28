#include <on_foot.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <ranges>

auto on_foot_tracker_t::removed(std::chrono::sys_seconds when, events::backpack_item_t const & item)
  -> std::optional<info::consumable_use_t>
  {
  if(item.Type != "Consumable" or item.Count == 0u)
    return std::nullopt;

  if(item.Name == "amm_grenade_frag")
    last_frag_ = when;

  use_seq_ = when == last_use_ ? use_seq_ + 1u : 0u;
  last_use_ = when;
  return info::consumable_use_t{.timestamp = when, .seq = use_seq_, .name = item.Name, .count = item.Count};
  }

auto on_foot_tracker_t::kill(std::chrono::sys_seconds when, info::foot_kill_e kind, std::string weapon)
  -> info::foot_kill_t
  {
  kill_seq_ = when == last_kill_ ? kill_seq_ + 1u : 0u;
  last_kill_ = when;
  auto const since_frag{when - last_frag_};
  return info::foot_kill_t{
    .timestamp = when,
    .seq = kill_seq_,
    .kind = kind,
    .grenade = last_frag_ != std::chrono::sys_seconds{} and since_frag >= grenade_from and since_frag <= grenade_to,
    .weapon = std::move(weapon)
  };
  }

auto is_foot_target(std::string_view target) -> bool { return target.contains("suitai"); }

auto weapon_log_t::observe(std::chrono::sys_seconds written, std::string weapon) -> void
  {
  // a few fights' worth is enough - a kill is asked about within seconds of being made
  constexpr size_t kept{64u};
  std::scoped_lock const lock{mutex_};
  if(not changes_.empty() and (changes_.back().weapon == weapon or written < changes_.back().since))
    return;
  changes_.push_back(change_t{.since = written, .weapon = std::move(weapon)});
  while(changes_.size() > kept)
    changes_.pop_front();
  }

auto weapon_log_t::at(std::chrono::sys_seconds when) const -> std::string
  {
  std::scoped_lock const lock{mutex_};
  // the last change before the kill's second is what was held going into it - unless it changed within it
  auto const before{std::ranges::find_if(
    changes_ | std::views::reverse, [when](change_t const & change) { return change.since < when; }
  )};
  bool const changed_within{std::ranges::any_of(changes_, [when](change_t const & c) { return c.since == when; })};
  if(changed_within or before == (changes_ | std::views::reverse).end())
    return {};
  return before->weapon;
  }

namespace
  {
  ///\brief the dictionary learns the category and the readable name from every counter
  auto item_of(database_storage_t & db, std::string_view name, std::string const & localised, std::string const & category,
               uint32_t count, bool received) -> info::micro_sale_item_t
    {
    std::string key{micro_resource_key(name)};
    if(auto res{db.store(info::micro_resource_t{.name = key, .id = {}, .localised = localised, .category = category})};
       not res)
      spdlog::error("failed to store micro resource {}", name);
    return info::micro_sale_item_t{.oid = -1, .sale_oid = 0, .name = std::move(key), .count = count, .received = received};
    }
  }  // namespace

auto store_counter_trade(database_storage_t & db, std::chrono::sys_seconds when, events::buy_micro_resources_t const & event)
  -> void
  {
  std::vector<info::micro_sale_item_t> items;
  uint32_t total{};
  // the older form names its one kind in the event itself
  if(event.MicroResources.empty() and not event.Name.empty())
    {
    items.push_back(item_of(db, event.Name, event.Name_Localised, event.Category, event.Count, true));
    total = event.Count;
    }
  for(events::sold_micro_resource_t const & bought: event.MicroResources)
    {
    items.push_back(item_of(db, bought.Name, bought.Name_Localised, bought.Category, bought.Count, true));
    total += bought.Count;
    }
  if(items.empty())
    return;

  info::micro_sale_t const sale{
    .timestamp = when,
    .market_id = event.MarketID,
    .price = event.Price,
    .total_count = total,
    .kind = info::micro_trade_e::bought
  };
  if(auto res{db.store(sale, items)}; not res)
    spdlog::error("failed to store micro resource purchase at {}", event.MarketID);
  }

auto store_counter_trade(database_storage_t & db, std::chrono::sys_seconds when, events::trade_micro_resources_t const & event)
  -> void
  {
  std::vector<info::micro_sale_item_t> items;
  for(events::sold_micro_resource_t const & offered: event.Offered)
    items.push_back(item_of(db, offered.Name, offered.Name_Localised, offered.Category, offered.Count, false));
  if(not event.Received.empty())
    items.push_back(item_of(db, event.Received, event.Received_Localised, event.Category, event.Count, true));
  if(items.empty())
    return;

  info::micro_sale_t const sale{
    .timestamp = when,
    .market_id = event.MarketID,
    .price = 0u,
    .total_count = event.TotalCount,
    .kind = info::micro_trade_e::bartered
  };
  if(auto res{db.store(sale, items)}; not res)
    spdlog::error("failed to store micro resource barter at {}", event.MarketID);
  }
