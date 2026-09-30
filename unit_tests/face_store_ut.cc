#include <face_store.h>

#include <boost/ut.hpp>

#include <unistd.h>

using namespace boost::ut;

int main()
  {
  "a view comes back as it was kept, and a better one takes its place"_test = []
  {
    // a directory of this run's own, so a run never meets the rows of another
    std::filesystem::path const dir{std::filesystem::temp_directory_path() / std::format("eht_face_store_ut_{}", getpid())};
    std::filesystem::create_directories(dir);
    std::filesystem::path const live{dir / "live.sqlite"};

    expect(not face_store::stamp(live, "A 1").has_value());

    planet_face::image_t image{.width = 16u, .height = 8u, .rgb = {}};
    for(size_t at{}; at != 16u * 8u * 3u; ++at)
      image.rgb.push_back(uint8_t(at * 7u));
    expect(face_store::save(live, "A 1", 0.5f, image));

    auto const kept{face_store::load(live, "A 1")};
    expect(fatal(kept.has_value()));
    expect(kept->score == 0.5_f);
    expect(kept->image.width == 16u and kept->image.height == 8u);
    expect(kept->image.rgb == image.rgb);

    image.rgb.front() = 1u;
    expect(face_store::save(live, "A 1", 0.9f, image));
    auto const stamp{face_store::stamp(live, "A 1")};
    expect(fatal(stamp.has_value()));
    expect(stamp->first == 0.9_f);
    expect(face_store::load(live, "A 1")->image.rgb.front() == 1_u);
    expect(not face_store::stamp(live, "A 2").has_value());

    expect(not face_store::save(live, "A 3", 1.f, planet_face::image_t{}));
  };
  }
