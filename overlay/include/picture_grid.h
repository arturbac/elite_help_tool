#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

///\brief how a block's pictures are laid out in a grid - plain arithmetic, apart from ImGui so it can be tested
namespace overlay::pictures
  {
struct grid_t
  {
  uint32_t columns;
  ///\brief the width of one cell in pixels; a picture is as tall as its aspect makes it
  float cell;
  };

///\brief the grid that makes count pictures largest within width x height pixels
///\param most the most columns allowed (the block's picture_columns, held to 1..8)
///\param height 0 sets no limit: then the grid takes the most columns across the width, as it always did
///\param aspect a picture's height over its width - the tallest one, so every row fits
///\detail with a height every column count from 1 to most is tried and the one with the largest cell wins;
/// a tie goes to fewer columns, the taller and narrower grid
[[nodiscard]]
constexpr auto grid(size_t count, uint32_t most, float width, float height, float gap, float aspect) noexcept -> grid_t
  {
  uint32_t const widest{std::clamp(most, 1u, 8u)};
  auto const cell_across = [&](uint32_t columns) { return (width - gap * float(columns - 1u)) / float(columns); };
  if(height <= 0.f or count == 0u or aspect <= 0.f)
    return grid_t{.columns = widest, .cell = cell_across(widest)};

  grid_t best{.columns = 1u, .cell = 0.f};
  for(uint32_t columns{1u}; columns <= widest; ++columns)
    {
    auto const rows{float((count + columns - 1u) / columns)};
    float const cell_down{(height - gap * (rows - 1.f)) / rows / aspect};
    float const cell{std::min(cell_across(columns), cell_down)};
    if(cell > best.cell)
      best = grid_t{.columns = columns, .cell = cell};
    }
  return best;
  }
  }  // namespace overlay::pictures
