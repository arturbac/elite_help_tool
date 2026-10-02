#include <boost/ut.hpp>
#include <graphics_profile.h>

#include <chrono>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;
  using graphics_profile::place_e;
  using graphics_profile::profile_t;

  profile_t const planet{.name = "FSR Balanced + SMAA", .upscaling = 2u, .supersampling = 0.59, .anti_aliasing = 4u};
  profile_t const space{
    .name = "FSR Ultra Quality + SMAA", .upscaling = 2u, .supersampling = 0.77, .anti_aliasing = 4u
  };

  "the three values are read from the game's file"_test = []
  {
    // as the game wrote it on 3 Oct 2026, shortened
    auto const setting{graphics_profile::parse(
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<Root PresetName=\"Custom\" MajorVersion=\"4\" MinorVersion=\"4\">\n"
      "\t<UpscalingQuality>2</UpscalingQuality>\n"
      "\t<TextureQualityEx>2</TextureQualityEx>\n"
      "\t<AAMode>4</AAMode>\n"
      "\t<HMDRenderTargetMultiplier>1.000000</HMDRenderTargetMultiplier>\n"
      "\t<SSAAMultiplier>0.590000</SSAAMultiplier>\n"
      "</Root>\n"
    )};
    expect(fatal(setting.has_value()));
    expect(setting->upscaling == 2u);
    expect(setting->anti_aliasing == 4u);
    expect(setting->supersampling > 0.589 and setting->supersampling < 0.591);
  };

  "a file without one of them gives nothing"_test = []
  {
    expect(not graphics_profile::parse("<Root><UpscalingQuality>2</UpscalingQuality><AAMode>4</AAMode></Root>"));
    expect(not graphics_profile::parse(
      "<Root><UpscalingQuality>x</UpscalingQuality><AAMode>4</AAMode>"
      "<SSAAMultiplier>0.5</SSAAMultiplier></Root>"
    ));
  };

  "near a planet from orbital cruise down, and on foot on it"_test = []
  {
    expect(graphics_profile::place_of(1u << 21u, 0u) == place_e::planet);
    expect(graphics_profile::place_of(0u, 1u << 4u) == place_e::planet);
    // supercruise, docked at a station, on foot in a station
    expect(graphics_profile::place_of(1u << 4u, 0u) == place_e::space);
    expect(graphics_profile::place_of(1u << 0u, 0u) == place_e::space);
    expect(graphics_profile::place_of(0u, (1u << 0u) | (1u << 3u)) == place_e::space);
  };

  "a place counts once it has lasted, the first one at once"_test = []
  {
    graphics_profile::watch_t watch;
    std::chrono::steady_clock::time_point const t0{};
    expect(graphics_profile::settle(watch, place_e::space, t0, 3s) == place_e::space);
    expect(graphics_profile::settle(watch, place_e::planet, t0 + 1s, 3s) == place_e::space);
    // a flicker back starts the wait again
    expect(graphics_profile::settle(watch, place_e::space, t0 + 2s, 3s) == place_e::space);
    expect(graphics_profile::settle(watch, place_e::planet, t0 + 3s, 3s) == place_e::space);
    expect(graphics_profile::settle(watch, place_e::planet, t0 + 5s, 3s) == place_e::space);
    expect(graphics_profile::settle(watch, place_e::planet, t0 + 6s, 3s) == place_e::planet);
  };

  "the line asks for the place's set until the file holds it"_test = [&]
  {
    graphics_profile::setting_t const balanced{.upscaling = 2u, .supersampling = 0.59, .anti_aliasing = 4u};
    graphics_profile::setting_t const other{.upscaling = 0u, .supersampling = 0.75, .anti_aliasing = 0u};
    expect(not graphics_profile::hint(place_e::planet, balanced, planet, space));
    expect(
      graphics_profile::hint(place_e::space, balanced, planet, space)
      == std::optional<std::string>{"graphics in space: set FSR Ultra Quality + SMAA - now FSR Balanced + SMAA"}
    );
    expect(
      graphics_profile::hint(place_e::planet, other, planet, space)
      == std::optional<std::string>{
        "graphics near the planet: set FSR Balanced + SMAA - now upscaling 0, supersampling 0.75, AA mode 0"
      }
    );
  };

  "the graphics options lie in the same Wine user's home as the journals"_test = []
  {
    expect(
      graphics_profile::graphics_dir_of(
        "/pfx/drive_c/users/steamuser/Saved Games/Frontier Developments/Elite Dangerous"
      )
      == std::filesystem::path{
        "/pfx/drive_c/users/steamuser/AppData/Local/Frontier Developments/Elite Dangerous/Options/Graphics"
      }
    );
    expect(graphics_profile::graphics_dir_of("/home/someone/journals").empty());
  };
  }
