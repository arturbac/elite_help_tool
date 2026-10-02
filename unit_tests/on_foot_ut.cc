#include <boost/ut.hpp>
#include <on_foot.h>
#include <events/micro_resources.h>
#include <json_io.h>

auto main() -> int
  {
  using namespace boost::ut;
  using std::chrono::seconds;
  using std::chrono::sys_seconds;

  auto const frag{::events::backpack_item_t{.Name = "amm_grenade_frag", .Name_Localised = "Frag Grenade", .Type = "Consumable", .Count = 1u}};

  "grenade window"_test = [&]
  {
    on_foot_tracker_t tracker;
    sys_seconds const thrown{seconds{1000}};
    expect(tracker.removed(thrown, frag).has_value());
    expect(not tracker.kill(thrown + seconds{1}, info::foot_kill_e::murder, {}).grenade);
    expect(tracker.kill(thrown + seconds{2}, info::foot_kill_e::murder, {}).grenade);
    expect(tracker.kill(thrown + seconds{4}, info::foot_kill_e::bounty, {}).grenade);
    expect(not tracker.kill(thrown + seconds{5}, info::foot_kill_e::conflict_zone, {}).grenade);
  };

  "no throw, no grenade"_test = []
  {
    on_foot_tracker_t tracker;
    expect(not tracker.kill(sys_seconds{seconds{3}}, info::foot_kill_e::murder, {}).grenade);
  };

  "two in one second are told apart"_test = [&]
  {
    on_foot_tracker_t tracker;
    sys_seconds const at{seconds{50}};
    expect(tracker.removed(at, frag)->seq == 0_u);
    expect(tracker.removed(at, frag)->seq == 1_u);
    expect(tracker.removed(at + seconds{1}, frag)->seq == 0_u);
    expect(tracker.kill(at, info::foot_kill_e::murder, {}).seq == 0_u);
    expect(tracker.kill(at, info::foot_kill_e::murder, {}).seq == 1_u);
  };

  "only consumables are used up"_test = []
  {
    on_foot_tracker_t tracker;
    auto const goods{::events::backpack_item_t{.Name = "insight", .Name_Localised = {}, .Type = "Item", .Count = 1u}};
    expect(not tracker.removed(sys_seconds{seconds{1}}, goods).has_value());
  };

  "a suit is a kill on foot"_test = []
  {
    expect(is_foot_target("citizensuitai_scientific"));
    expect(is_foot_target("assaultsuitai_class1"));
    expect(not is_foot_target("cobramkiii"));
  };

  "the weapon in hand at a kill"_test = []
  {
    weapon_log_t log;
    sys_seconds const t0{seconds{100}};
    expect(log.at(t0).empty());
    log.observe(t0, "Rifle");
    // the same second is a change that could have come after the shot
    expect(log.at(t0).empty());
    expect(log.at(t0 + seconds{1}) == "Rifle");
    log.observe(t0 + seconds{2}, "Launcher");
    log.observe(t0 + seconds{2}, "Launcher");
    log.observe(t0 + seconds{3}, "Pistol");
    expect(log.at(t0 + seconds{1}) == "Rifle");
    expect(log.at(t0 + seconds{2}).empty());
    expect(log.at(t0 + seconds{3}).empty());
    expect(log.at(t0 + seconds{4}) == "Pistol");
  };
  
  "a locker read off the journal line"_test = []
  {
    std::string const line{
      R"({ "timestamp":"2026-10-02T22:43:34Z", "event":"ShipLocker", "Items":[ { "Name":"virus", "OwnerID":0,)"
      R"( "MissionID":1067513890, "Count":1 } ], "Components":[ ], "Consumables":[ ], "Data":[ { "Name":"surveilleancelogs",)"
      R"( "Name_Localised":"Surveillance Logs", "OwnerID":0, "Count":3 } ] })"
    };
    ::events::ship_locker_t locker{};
    expect(not eht::json::read_lenient(locker, line));
    expect(::events::carries_lists(locker));
    expect(fatal(locker.Items.size() == 1_u));
    expect(locker.Items[0].MissionID == 1067513890_ull);

    std::vector<std::string> categories;
    ::events::for_each_item(locker, [&](::events::locker_item_t const &, std::string_view c) { categories.emplace_back(c); });
    expect(categories == std::vector<std::string>{"Item", "Data"});

    // the bare event that leaves the lists in ShipLocker.json
    ::events::ship_locker_t bare{};
    expect(not eht::json::read_lenient(bare, std::string{R"({ "timestamp":"2026-01-07T17:25:29Z", "event":"ShipLocker" })"}));
    expect(not ::events::carries_lists(bare));
  };

  "the backpack follows its changes"_test = []
  {
    ::events::backpack_t backpack{};
    backpack.Consumables.push_back(::events::locker_item_t{.Name = "healthpack", .Name_Localised = "Medkit", .Count = 2u});

    ::events::backpack_change_t change{};
    change.Added.push_back(::events::backpack_item_t{.Name = "$Virus_Name;", .Name_Localised = "Virus", .Type = "Item", .Count = 1u});
    change.Added.push_back(::events::backpack_item_t{.Name = "virus", .Name_Localised = "Virus", .Type = "Item", .Count = 1u});
    change.Removed.push_back(::events::backpack_item_t{.Name = "healthpack", .Name_Localised = "Medkit", .Type = "Consumable", .Count = 1u});
    apply_backpack_change(backpack, change);
    expect(fatal(backpack.Items.size() == 1_u));
    expect(backpack.Items[0].Count == 2_u);
    expect(fatal(backpack.Consumables.size() == 1_u));
    expect(backpack.Consumables[0].Count == 1_u);

    // uploaded to the data port - the row goes, it does not stay at nought
    ::events::backpack_change_t upload{};
    upload.Removed.push_back(::events::backpack_item_t{.Name = "virus", .Name_Localised = "Virus", .Type = "Item", .Count = 2u});
    apply_backpack_change(backpack, upload);
    expect(backpack.Items.empty());
  };
}
