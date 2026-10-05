#include <world_follow.h>
#include <world_target.h>

#include <boost/ut.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace boost::ut;

namespace
  {
constexpr int64_t now_ms{1'800'000'000'000};

[[nodiscard]]
auto near(float a, float b) -> bool
  { return std::fabs(a - b) < 1e-5f; }

///\brief a record as edworld writes it: magic, an even sequence, the frame's panels
[[nodiscard]]
auto make_record() -> std::unique_ptr<edworld::share_t>
  {
  auto record{std::make_unique<edworld::share_t>()};
  record->magic = edworld::share_magic;
  record->version = edworld::share_version;
  record->size = sizeof(edworld::share_t);
  record->sequence = 2u;
  record->unix_ms = now_ms;
  return record;
  }

auto add_panel(edworld::share_t & record, uint32_t width, uint32_t height, float x, float y, float w) -> void
  {
  edworld::panel_t & panel{record.panels[record.panel_count++]};
  panel.surface_width = width;
  panel.surface_height = height;
  panel.anchor_clip[0] = x * w;
  panel.anchor_clip[1] = y * w;
  panel.anchor_clip[2] = 0.f;
  panel.anchor_clip[3] = w;
  }
  }  // namespace

int main()
  {
  "a panel followed by its surface's size moves the patch by as much as its origin moved"_test = []
  {
    auto record{make_record()};
    add_panel(*record, 1024u, 512u, 0.10f, -0.20f, 2.f);
    add_panel(*record, 640u, 480u, 0.90f, 0.90f, 2.f);
    auto const shift{overlay::world::panel_shift(*record, {1024u, 512u, 0.05f, -0.25f}, now_ms)};
    expect(fatal(shift.has_value()));
    expect(near(shift->x, 0.05f)) << shift->x;
    expect(near(shift->y, 0.05f)) << shift->y;
  };

  "of two panels of one size, the one nearest its rest place"_test = []
  {
    auto record{make_record()};
    add_panel(*record, 1024u, 512u, -0.60f, 0.10f, 1.f);
    add_panel(*record, 1024u, 512u, 0.02f, 0.01f, 1.f);
    auto const shift{overlay::world::panel_shift(*record, {1024u, 512u, 0.f, 0.f}, now_ms)};
    expect(fatal(shift.has_value()));
    expect(near(shift->x, 0.02f) and near(shift->y, 0.01f));
  };

  "nothing to follow: not asked, not drawn, behind the eye or stale"_test = []
  {
    auto record{make_record()};
    add_panel(*record, 1024u, 512u, 0.1f, 0.1f, 1.f);
    expect(not overlay::world::panel_shift(*record, {0u, 0u, 0.f, 0.f}, now_ms).has_value());
    expect(not overlay::world::panel_shift(*record, {800u, 600u, 0.f, 0.f}, now_ms).has_value());
    expect(not overlay::world::panel_shift(*record, {1024u, 512u, 0.f, 0.f}, now_ms + overlay::world::fresh_ms + 1)
                 .has_value());
    auto behind{make_record()};
    add_panel(*behind, 1024u, 512u, 0.1f, 0.1f, -1.f);
    expect(not overlay::world::panel_shift(*behind, {1024u, 512u, 0.f, 0.f}, now_ms).has_value());
  };

  "the destination told to edworld: the database's allegiance, or not known"_test = []
  {
    auto const fed{overlay::world::make_target(1487912553027ull, true, "Federation", "Bleia Eohn QT-O d7-43", now_ms)};
    expect(fed.magic == edworld::target_magic and fed.version == edworld::target_version);
    expect(fed.size == sizeof(edworld::target_t) and fed.system_address == 1487912553027ull);
    expect(fed.known == 1u and fed.allegiance == 1u);
    expect(std::string_view{fed.name} == "Bleia Eohn QT-O d7-43");
    expect(overlay::world::allegiance_code("Empire") == 2u and overlay::world::allegiance_code("Alliance") == 3u);
    expect(overlay::world::allegiance_code("Independent") == 4u and overlay::world::allegiance_code("Thargoid") == 5u);
    auto const unknown{overlay::world::make_target(7ull, false, "Empire", std::string(100, 'x'), now_ms)};
    expect(unknown.known == 0u and unknown.allegiance == 0u) << "an unknown system names no allegiance";
    expect(std::string_view{unknown.name}.size() == sizeof unknown.name - 1u) << "a long name is cut, NUL kept";
  };

  "the destination's factions go with it, only when the tool has the system and its readings"_test = []
  {
    std::vector<overlay::world::faction_entry_t> const factions{
      {.name = "Bleia Eohn Gold Federal Industry",
       .states = "Boom  EP +3",
       .influence = 0.412,
       .allegiance = 1u,
       .trend = 1u,
       .controlling = true},
      {.name = std::string_view{"a name far longer than the sixty-three bytes the field has room for, cut"},
       .states = {},
       .influence = 0.2,
       .allegiance = 9u,
       .trend = 7u,
       .controlling = false}
    };
    auto const t{overlay::world::make_target(1ull, true, "Federation", "Bleia Eohn", now_ms, true, factions)};
    expect(t.size == sizeof(edworld::target_t) and t.size > edworld::target_size_first);
    expect(t.factions_known == 1u and t.faction_count == 2u);
    expect(std::string_view{t.factions[0].name} == "Bleia Eohn Gold Federal Industry");
    expect(std::string_view{t.factions[0].states} == "Boom  EP +3");
    expect(t.factions[0].allegiance == 1u and t.factions[0].trend == 1u and t.factions[0].controlling == 1u);
    expect(t.factions[0].influence > 0.411f and t.factions[0].influence < 0.413f);
    expect(std::string_view{t.factions[1].name}.size() == sizeof t.factions[1].name - 1u)
      << "a long name is cut, NUL kept";
    expect(t.factions[1].allegiance == 5u and t.factions[1].trend == 0u)
      << "codes out of range fall to other / unknown";
    auto const not_known{overlay::world::make_target(1ull, false, "", "X", now_ms, true, factions)};
    expect(not_known.factions_known == 0u and not_known.faction_count == 0u) << "no list for a system the tool lacks";
    auto const no_readings{overlay::world::make_target(1ull, true, "Federation", "X", now_ms, false, factions)};
    expect(no_readings.factions_known == 0u and no_readings.faction_count == 0u);
    std::vector<overlay::world::faction_entry_t> const many(20u, factions[0]);
    expect(
      overlay::world::make_target(1ull, true, "", "X", now_ms, true, many).faction_count == edworld::max_target_factions
    );
  };

  "the target goes into the shared record under its seqlock, even and whole"_test = []
  {
    auto shared{std::make_unique<edworld::target_t>()};
    shared->sequence = 4u;
    overlay::world::write_target(*shared, overlay::world::make_target(42ull, true, "Alliance", "A", now_ms));
    expect(shared->sequence == 6u) << shared->sequence;
    expect(shared->system_address == 42ull and shared->allegiance == 3u and shared->magic == edworld::target_magic);
    overlay::world::write_target(*shared, overlay::world::make_target(43ull, false, "", "B", now_ms));
    expect(shared->sequence == 8u and shared->system_address == 43ull and shared->known == 0u);
  };

  "a record is read only when consistent and edworld's"_test = []
  {
    auto record{make_record()};
    add_panel(*record, 1024u, 512u, 0.1f, 0.1f, 1.f);
    auto copy{std::make_unique<edworld::share_t>()};
    expect(overlay::world::read_record(*record, *copy));
    expect(copy->panel_count == 1u and copy->panels[0].surface_width == 1024u);
    record->sequence = 3u;  // the writer inside
    expect(not overlay::world::read_record(*record, *copy));
    record->sequence = 4u;
    record->magic = 0u;
    expect(not overlay::world::read_record(*record, *copy));
  };
  }
