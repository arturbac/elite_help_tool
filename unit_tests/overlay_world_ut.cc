#include <world_follow.h>

#include <boost/ut.hpp>

#include <cmath>
#include <memory>

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
