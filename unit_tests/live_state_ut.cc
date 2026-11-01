#include <carrier_owner.h>
#include <live_state.h>

#include <boost/ut.hpp>
#include <sstream>

#include <vector>

using namespace boost::ut;

namespace
  {
[[nodiscard]]
auto module(std::string item) -> ::events::module_t
  { return ::events::module_t{.Slot = "Slot", .Item = std::move(item), .On = true, .Priority = 0u, .Health = 1.f}; }
  }  // namespace

// the lock goes at a jump, at supercruise, at a dock, on foot, at death and at a change of ship
static_assert(live::drops_target_lock<::events::supercruise_entry_t>);
static_assert(live::drops_target_lock<::events::start_jump_t>);
static_assert(live::drops_target_lock<::events::fsd_jump_t>);
static_assert(live::drops_target_lock<::events::docked_t>);
static_assert(live::drops_target_lock<::events::disembark_t>);
static_assert(live::drops_target_lock<::events::died_t>);
static_assert(live::drops_target_lock<::events::resurrect_t>);
static_assert(live::drops_target_lock<::events::shipyard_swap_t>);
static_assert(live::drops_target_lock<::events::location_t>);
// a new lock is the target's own event, and near the ground a lock may be held
static_assert(not live::drops_target_lock<::events::ship_targeted_t>);
static_assert(not live::drops_target_lock<::events::supercruise_exit_t>);

// the market and the shipyard name the port; out of an escape pod the Location does not
static_assert(live::names_the_port<::events::market_t>);
static_assert(live::names_the_port<::events::shipyard_swap_t>);
static_assert(not live::names_the_port<::events::location_t>);

auto main() -> int
  {
  "a ship with a fighter hangar has its crew aboard"_test = []
  {
    std::vector const modules{module("int_fighterbay_size5_class1"), module("int_hyperdrive_size5_class5")};
    expect(live::has_fighter_bay(modules));
  };

  "the hangar's name in any case"_test = []
  {
    std::vector const modules{module("Int_FighterBay_Size6_Class1")};
    expect(live::has_fighter_bay(modules));
  };

  "a ship without one has no crew aboard"_test = []
  {
    std::vector const modules{module("int_cargorack_size6_class1"), module("int_hyperdrive_size5_class5")};
    expect(not live::has_fighter_bay(modules));
  };

  "a fleet carrier is its owner's, the squadron's carrier everyone's"_test = []
  {
    info::carrier_t maria{};
    maria.carrier_type = "FleetCarrier";
    maria.owner_fid = "F1";
    expect(carrier_owner::ours(maria, "F1"));
    expect(not carrier_owner::ours(maria, "F2"));
    expect(not carrier_owner::ours(maria, ""));
    info::carrier_t squadron{};
    squadron.carrier_type = "SquadronCarrier";
    expect(carrier_owner::ours(squadron, "F1") and carrier_owner::ours(squadron, "F2"));
    expect(not carrier_owner::own(squadron, "F1"));
    // never managed yet: nobody's, whatever the old shared mark says - one account's mark is not the other's
    info::carrier_t unknown{};
    unknown.carrier_type = "FleetCarrier";
    unknown.tracked = true;
    expect(not carrier_owner::ours(unknown, "F1") and not carrier_owner::ours(unknown, "F2"));
  };

  "a journal names the owner of each fleet carrier managed in it, and never of the squadron's"_test = []
  {
    std::istringstream journal{
      R"({ "timestamp":"2026-10-31T12:00:00Z", "event":"Commander", "FID":"F2", "Name":"DUNKAN" })" "\n"
      R"({ "timestamp":"2026-10-31T12:59:06Z", "event":"CarrierStats", "CarrierID":43, "CarrierType":"FleetCarrier", "Callsign":"N1M-L8V" })" "\n"
      R"({ "timestamp":"2026-10-31T13:00:00Z", "event":"CarrierStats", "CarrierID":44, "CarrierType":"SquadronCarrier", "Callsign":"PRNH" })" "\n"
      R"({ "timestamp":"2026-10-31T14:00:00Z", "event":"Commander", "FID":"F1", "Name":"SJONA" })" "\n"
      R"({ "timestamp":"2026-10-31T14:01:00Z", "event":"CarrierStats", "CarrierID":42, "CarrierType":"FleetCarrier", "Callsign":"W1V-NXM" })" "\n"};
    carrier_owner::owners_t const owners{carrier_owner::owners_in_journal(journal)};
    expect(owners.size() == 2u);
    expect(owners.contains(43u) and owners.at(43u) == "F2");
    expect(owners.contains(42u) and owners.at(42u) == "F1");
    // a CarrierStats before any Commander has no account to name
    std::istringstream headless{R"({ "event":"CarrierStats", "CarrierID":43, "CarrierType":"FleetCarrier" })" "\n"};
    expect(carrier_owner::owners_in_journal(headless).empty());
  };

  "a fleet carrier may be marked by an account only while nobody else owns it"_test = []
  {
    info::carrier_t ignacy{};
    ignacy.carrier_type = "FleetCarrier";
    expect(carrier_owner::may_mark(ignacy, "F2"));
    expect(not carrier_owner::may_mark(ignacy, ""));
    ignacy.owner_fid = "F2";
    expect(carrier_owner::may_mark(ignacy, "F2"));
    expect(not carrier_owner::may_mark(ignacy, "F1"));
    info::carrier_t squadron{};
    squadron.carrier_type = "SquadronCarrier";
    expect(not carrier_owner::may_mark(squadron, "F1"));
  };

  "no Loadout yet hides nothing"_test = [] { expect(live::has_fighter_bay(std::vector<::events::module_t>{})); };
  }
