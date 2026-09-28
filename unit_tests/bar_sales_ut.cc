#include <boost/ut.hpp>
#include <bar_sales.h>

auto main() -> int
  {
  using namespace boost::ut;
  using std::chrono::minutes;
  using std::chrono::sys_seconds;

  auto const reading = [](std::string name, int at_minutes, uint32_t price, uint32_t stock) -> info::carrier_stock_t
  {
    return info::carrier_stock_t{
      .name = std::move(name),
      .localised = {},
      .category = "Data",
      .price = price,
      .stock = stock,
      .demand = 0u,
      .timestamp = sys_seconds{minutes{at_minutes}}
    };
  };

  "a fall is a sale at the earlier price, a rise is the owner's"_test = [&]
  {
    std::vector<info::carrier_stock_t> const history{
      reading("gas", 0, 100u, 25u),
      reading("gas", 5, 100u, 66u),   // restocked on the spot
      reading("gas", 600, 120u, 66u), // back after an absence, nothing sold
      reading("schematic", 0, 1000u, 55u),
      reading("schematic", 120, 1000u, 46u),
      reading("schematic", 125, 2000u, 50u),
      reading("schematic", 600, 2000u, 4u),
    };
    auto const sold{bar::sales(history)};
    expect(sold.size() == 2_u);
    expect(sold[0].sold == 0_u and sold[0].absences_listed == 1_u and sold[0].absences_sold == 0_u);
    expect(sold[0].price == 120_u);
    expect(sold[1].sold == 55_u);
    expect(sold[1].revenue == 9u * 1000u + 46u * 2000u);
    expect(sold[1].absences_listed == 2_u and sold[1].absences_sold == 2_u);
    expect(bar::absences(history) == 2_u);
  };

  "an empty reading is skipped"_test = [&]
  {
    std::vector<info::carrier_stock_t> const history{
      reading("gas", 0, 100u, 25u),
      reading("gas", 600, 100u, 0u),
      reading("gas", 607, 100u, 25u),
    };
    auto const sold{bar::sales(history)};
    expect(sold[0].sold == 0_u);
    expect(bar::absences(history) == 1_u);
  };

  "port prices come out of the sales"_test = []
  {
    std::vector<bar::port_sale_row_t> const rows{
      {.sale_oid = 1, .price = 3000u, .name = "a", .count = 3u},
      {.sale_oid = 2, .price = 5000u, .name = "a", .count = 2u},
      {.sale_oid = 2, .price = 5000u, .name = "b", .count = 1u},
      // does not add up - no price for c
      {.sale_oid = 3, .price = 100u, .name = "b", .count = 1u},
      {.sale_oid = 3, .price = 100u, .name = "c", .count = 1u},
    };
    auto const prices{bar::port_prices(rows)};
    expect(prices.at("a") == 1000.0_d);
    expect(prices.at("b") == 3000.0_d);
    expect(not prices.contains("c"));
  };
  }
