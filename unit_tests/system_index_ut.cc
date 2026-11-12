#include <diagram_svg.h>
#include <galaxy_region.h>
#include <system_index.h>

#include <boost/ut.hpp>

#include <array>
#include <string>

using namespace boost::ut;
using namespace std::string_view_literals;

suite<"system_index"> system_index_suite = []
{
  "a generated name is in its sector"_test = []
  {
    expect(system_index::sector_of("Bleia Eohn QT-O d7-43") == "Bleia Eohn"sv);
    expect(system_index::sector_of("Col 359 Sector TU-F d11-89") == "Col 359 Sector"sv);
    expect(system_index::sector_of("Spoihee QO-H c26-0") == "Spoihee"sv);
    expect(system_index::sector_of("Leamue CF-A e3088") == "Leamue"sv);
  };

  "a name of its own has no sector"_test = []
  {
    expect(system_index::sector_of("Sol") == system_index::named_systems);
    expect(system_index::sector_of("Kacobog") == system_index::named_systems);
    expect(system_index::sector_of("Shinrarta Dezhra") == system_index::named_systems);
    expect(system_index::sector_of("HIP 12345 A") == system_index::named_systems);
  };

  "a body is counted under its letter"_test = []
  {
    expect(system_index::body_letter("Sudarsky class III gas giant") == "G"sv);
    expect(system_index::body_letter("Water giant") == "G"sv);
    expect(system_index::body_letter("Gas giant with ammonia based life") == "G"sv);
    expect(system_index::body_letter("High metal content body") == "H"sv);
    expect(system_index::body_letter("Metal rich body") == "M"sv);
    expect(system_index::body_letter("Rocky ice body") == "Ri"sv);
    expect(system_index::body_letter("Rocky body") == "R"sv);
    expect(system_index::body_letter("Icy body") == "I"sv);
    expect(system_index::body_letter("Earthlike body") == "E"sv);
    expect(system_index::body_letter("").empty()) << "a belt cluster";
  };

  "the counts keep their order"_test = []
  {
    system_index::body_counts_t const counts{{"R", 14u}, {"S", 1u}, {"G", 3u}, {"Ri", 2u}};
    expect(system_index::counts_text(counts) == "[S:1, G:3, R:14, Ri:2]"sv);
    expect(system_index::counts_text({}).empty());
  };

  "a system stands in the region its address names, as the game's codex entries say"_test = []
  {
    // the regions Sjona's codex entries gave; Flyua Dryoae FW-T d4-6 and Skaude KM-C d128 stand by a border,
    // where the star's own position would give the region next door
    expect(galaxy_region::of_address(2140781169011u) == "Inner Orion Spur"sv) << "12 Aquarii";
    expect(galaxy_region::of_address(12274907287851u) == "Inner Scutum-Centaurus Arm"sv) << "Asura";
    expect(galaxy_region::of_address(102050133615322u) == "Odin's Hold"sv) << "Eoch Pruae DX-G c25-371";
    expect(galaxy_region::of_address(1470071844u) == "Sanguineous Rim"sv) << "Fedgau OS-T e3-0";
    expect(galaxy_region::of_address(363562668634u) == "Outer Orion Spur"sv) << "NGC 2546 Sector JL-W c18-1";
    expect(galaxy_region::of_address(217122310443u) == "Inner Orion Spur"sv) << "Flyua Dryoae FW-T d4-6";
    expect(galaxy_region::of_address(4407617867011u) == "Inner Orion Spur"sv) << "Skaude KM-C d128";
    expect(galaxy_region::of_address(58156541301184u) == "Inner Orion Spur"sv) << "Aucoks, a name of its own";
    expect(galaxy_region::of_address(0u).empty()) << "the galaxy's corner is in no region";
    expect(galaxy_region::at(1e6, 0.0).empty());
    expect(system_index::region_of(0u) == system_index::unknown_region);
  };

  "the index folds the sectors under their regions, the unknown one last"_test = []
  {
    std::array const rows{
      system_index::row_t{.address = 0u, .name = "Leamue BX-B d13-1906", .counts = "[S:1]", .href = "systems/a.html"},
      system_index::row_t{.address = 102050133615322u, .name = "Eoch Pruae DX-G c25-371", .counts = "[S:1, G:3]", .href = "systems/b.html", .picture = true, .life = true, .first = true, .star_type = "K"}
    };
    std::string const page{system_index::index_page(rows)};
    expect(page.contains("2 systems known, 1 with pictures, 1 with life, 1 with a first discovery"));
    expect(page.find("Odin&#39;s Hold") == std::string::npos);
    expect(page.find("Odin's Hold") < page.find(system_index::unknown_region));
    expect(page.contains(
      "<span class=\"star\" style=\"color:#ffcf7a\" title=\"K\">&#9679;</span> <a href=\"systems/b.html\">Eoch Pruae DX-G c25-371</a> &#128247; &#127807; &#11088;"
    )) << "the arrival star's dot in its class' colour";
    expect(page.contains("<span class=\"star\" style=\"color:#cccccc\">&#9679;</span> <a href=\"systems/a.html\">"))
      << "a grey dot with no star known";
    expect(page.contains("[S:1, G:3]"));
  };

  "the picture draws a ring's far half before the ball and its near half after"_test = []
  {
    overlay::diagram_t diagram{.width = 40.f, .height = 20.f};
    diagram.discs.push_back(
      overlay::disc_t{
        .x = 10.f,
        .y = 10.f,
        .radius = 5.f,
        .color = 0x4a88e0u,
        .sphere = true,
        .rings = {overlay::ball_ring_t{.radius = 8.f, .color = 0x9fd4ffu}},
        .body = 7u
      }
    );
    diagram.labels.push_back(overlay::label_t{.x = 10.f, .y = 2.f, .text = "A & B", .align = 0.5f});
    std::string const svg{overlay::diagram_svg(diagram)};
    size_t const ball{svg.find("<a href=\"#b7\">")};
    expect(ball != std::string::npos);
    expect(svg.find("#9fd4ff") < ball);
    expect(svg.rfind("#9fd4ff") > svg.find("</a>"));
    expect(svg.contains("A &amp; B"));
  };

  "a face is the ball's picture, clipped to it"_test = []
  {
    overlay::diagram_t diagram{.width = 40.f, .height = 20.f};
    diagram.discs.push_back(
      overlay::disc_t{.x = 10.f, .y = 10.f, .radius = 5.f, .sphere = true, .face = "../faces/a.png"}
    );
    std::string const svg{overlay::diagram_svg(diagram)};
    expect(svg.contains("<image href=\"../faces/a.png\""));
    expect(svg.contains("clip-path=\"url(#c1)\""));
  };
};

auto main() -> int {}
