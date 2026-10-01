#include <boost/ut.hpp>
#include <data/colonisation.h>
#include <data/market.h>
#include <data/ships.h>
#include <commodity_facts.h>

#include <set>

auto main() -> int
  {
  using namespace boost::ut;

  "the escape pod returns to ports only"_test = []
  {
    expect(info::is_escape_pod_port("Coriolis", "Arkush City"));
    expect(info::is_escape_pod_port("CraterOutpost", "Brewer Site"));
    expect(info::is_escape_pod_port("SurfaceStation", "Malzberg City"));
    expect(not info::is_escape_pod_port("OnFootSettlement", "Zhadan Command Complex"));
    expect(not info::is_escape_pod_port("FleetCarrier", "W1V-NXM"));
    expect(not info::is_escape_pod_port("PlanetaryConstructionDepot", "Planetary Construction Site: Zhadan"));
    expect(not info::is_escape_pod_port("SpaceConstructionDepot", "Orbital Construction Site: Graeff Terminal"));
    // the colonisation ship passes for a surface station, only its name gives it away
    expect(not info::is_escape_pod_port("SurfaceStation", "$EXT_PANEL_ColonisationShip; Bertin Territories"));
    expect(not info::is_escape_pod_port("", "Somewhere"));
  };

  "a site's list is ordered by name whatever the case"_test = []
  {
    expect(info::names_before("Ceramic Composites", "CMM Composite"));
    expect(not info::names_before("CMM Composite", "Ceramic Composites"));
    expect(info::needs_before("Industrial Materials", "Steel", "Machinery", "Aluminium")) << "the type goes first";
    expect(info::needs_before("Industrial Materials", "Ceramic Composites", "Industrial Materials", "CMM Composite"));
  };

  "a site is shown without the game's prefix"_test = []
  {
    info::construction_site_t site{};
    site.depot.market_id = 4393068035u;
    site.name = "Planetary Construction Site: Zhadan Command Complex";
    expect(info::shown_name(site) == std::string{"Zhadan Command Complex"});
    site.name = "Orbital Construction Site: Graeff Terminal";
    expect(info::shown_name(site) == std::string{"Graeff Terminal"});
    site.name = "Horwood Military Camp";
    expect(info::shown_name(site) == std::string{"Horwood Military Camp"});
    site.name.clear();
    expect(info::shown_name(site) == std::string{"site 4393068035"});
  };

  "the hold's names and the facts share one key"_test = []
  {
    expect(info::commodity_key("$terrainenrichmentsystems_name;") == std::string{"terrainenrichmentsystems"});
    expect(info::commodity_key("steel") == std::string{"steel"});
    expect(commodity_facts::find(info::commodity_key("$CMMComposite_name;")).has_value());
    expect(commodity_facts::find(info::commodity_key("$terrainenrichmentsystems_name;")).has_value());
  };

  "every fact is whole and said once"_test = []
  {
    std::set<std::string_view> keys;
    for(commodity_facts::fact_t const & fact: commodity_facts::facts)
      {
      expect(keys.insert(fact.key).second) << "twice:" << fact.key;
      expect(not fact.category.empty()) << fact.key;
      expect(uint8_t(fact.produced_by) != 0u) << fact.key;
      expect(not(fact.surface and fact.orbital)) << fact.key;
      }
    auto const membrane{commodity_facts::find("insulatingmembrane")};
    expect(membrane.has_value() and membrane->orbital and not membrane->surface);
    auto const cmm{commodity_facts::find("cmmcomposite")};
    expect(cmm.has_value() and cmm->surface);
    expect(commodity_facts::category_of("terrainenrichmentsystems") == std::optional<std::string_view>{"Technology"});
    expect(not commodity_facts::find("landenrichmentsystems").has_value()) << "the shown name is not the key";
  };
  }
