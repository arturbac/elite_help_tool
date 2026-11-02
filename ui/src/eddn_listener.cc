#include <eddn_listener.h>

#include <carrier_owner.h>
#include <data/carrier.h>
#include <data/market.h>
#include <data/micro_resources.h>
#include <data/progress.h>
#include <databse_storage.h>
#include <eddn_listen.h>
#include <holdings.h>
#include <eht_settings.h>
#include <event_guard.h>
#include <events/micro_resources.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <zlib.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <vector>

namespace eddn
  {
namespace
  {
  namespace asio = boost::asio;
  using tcp = asio::ip::tcp;
  using namespace std::chrono_literals;

  constexpr char const * relay_host{"eddn.edcd.io"};
  constexpr char const * relay_port{"9500"};

  ///\brief a message of the relay inflated - zlib, not gzip; nullopt when it does not inflate or grows past a sane size
  [[nodiscard]]
  auto inflate_message(std::string const & packed) -> std::optional<std::string>
    {
    constexpr size_t most{16u << 20u};
    z_stream stream{};
    if(inflateInit(&stream) != Z_OK)
      return std::nullopt;
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(packed.data()));
    stream.avail_in = uInt(packed.size());
    std::string out;
    std::array<char, 1u << 16u> chunk{};
    int result{Z_OK};
    while(result == Z_OK)
      {
      stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
      stream.avail_out = uInt(chunk.size());
      result = inflate(&stream, Z_NO_FLUSH);
      if(result != Z_OK and result != Z_STREAM_END)
        break;
      out.append(chunk.data(), chunk.size() - stream.avail_out);
      if(out.size() > most)
        {
        result = Z_DATA_ERROR;
        break;
        }
      }
    inflateEnd(&stream);
    if(result != Z_STREAM_END)
      return std::nullopt;
    return out;
    }

  ///\brief one connection to the relay, read until it breaks, the setting goes off or the thread is to stop
  class session_t
    {
  public:
    session_t(
      database_storage_t & db,
      std::function<std::string()> const & account,
      std::function<void()> const & stored,
      std::function<std::optional<std::array<double, 3>>()> const & here,
      std::stop_token stop
    ) :
        db_{db},
        account_{account},
        stored_{stored},
        stop_{std::move(stop)},
        here_{here}
      {
      }

    ///\brief true when the relay was reached and spoken to - the wait before the next try starts over then
    auto run() -> bool
      {
      tcp::socket socket{context_};
        {
        tcp::resolver resolver{context_};
        boost::system::error_code error;
        auto const endpoints{resolver.resolve(relay_host, relay_port, error)};
        if(error)
          return note_failure(std::format("the relay's name did not resolve: {}", error.message()));
        bool done{};
        asio::async_connect(
          socket, endpoints, [&](boost::system::error_code const & e, tcp::endpoint const &) { error = e, done = true; }
        );
        context_.run_for(15s);
        context_.restart();
        if(not done)
          return note_failure("the relay did not answer within 15 s");
        if(error)
          return note_failure(std::format("the relay refused: {}", error.message()));
        }

      boost::system::error_code error;
      auto const greeting{zmtp::greeting()};
      asio::write(socket, asio::buffer(greeting), error);
      std::string peer(64u, '\0');
      if(not error)
        read_exactly(socket, peer, error);
      if(error or not zmtp::greeting_ok(peer))
        return note_failure("the relay's greeting is not ZMTP 3 with NULL security");
      std::string const hello{zmtp::ready_sub() + zmtp::subscribe_all()};
      asio::write(socket, asio::buffer(hello), error);
      if(error)
        return note_failure(std::format("the subscription was not sent: {}", error.message()));

      spdlog::info("eddn listen: connected to the relay");
      reload_carriers();
      zmtp::reader_t reader;
      std::array<char, 1u << 16u> chunk{};
      auto last_data{std::chrono::steady_clock::now()};
      while(not stop_.stop_requested() and eht::settings()->eddn.listen)
        {
        size_t got{};
        bool done{};
        socket.async_read_some(
          asio::buffer(chunk), [&](boost::system::error_code const & e, size_t n) { error = e, got = n, done = true; }
        );
        context_.run_for(1s);
        if(not done)
          {
          // nothing for a second - a quiet moment; the read is let go and asked for again
          socket.cancel();
          context_.run();
          }
        context_.restart();
        if(error and error != asio::error::operation_aborted)
          {
          spdlog::warn("eddn listen: the relay's connection broke: {}", error.message());
          return true;
          }
        // the relay is never silent for long: a minute without a word is a connection that died quietly
        if(got == 0u)
          {
          if(std::chrono::steady_clock::now() - last_data > 1min)
            {
            spdlog::warn("eddn listen: nothing from the relay for a minute, connecting again");
            return true;
            }
          continue;
          }
        last_data = std::chrono::steady_clock::now();
        reader.feed(std::string_view{chunk.data(), got});
        while(auto frame{reader.next()})
          if(not frame->command)
            take(frame->body);
        if(reader.broken())
          {
          spdlog::warn("eddn listen: a frame from the relay is not ZMTP, connecting again");
          return true;
          }
        }
      return true;
      }

  private:
    auto read_exactly(tcp::socket & socket, std::string & into, boost::system::error_code & error) -> void
      {
      bool done{};
      asio::async_read(
        socket, asio::buffer(into), [&](boost::system::error_code const & e, size_t) { error = e, done = true; }
      );
      context_.run_for(15s);
      if(not done)
        {
        socket.cancel();
        context_.run();
        error = asio::error::timed_out;
        }
      context_.restart();
      }

    auto note_failure(std::string const & why) -> bool
      {
      spdlog::warn("eddn listen: {}", why);
      return false;
      }

    ///\brief the fleet carriers of the account of this tool, by market id - read again every few minutes
    auto reload_carriers() -> void
      {
      loaded_ = std::chrono::steady_clock::now();
      ours_.clear();
      std::string const fid{account_ ? account_() : std::string{}};
      if(auto carriers{db_.load_carriers()}; carriers)
        for(info::carrier_t const & c: *carriers)
          if(carrier_owner::own(c, fid))
            ours_.insert(c.market_id);
      if(auto claims{db_.load_claim_positions()}; claims)
        claims_ = std::move(*claims);
      if(auto ids{db_.load_commodity_ids()}; ids)
        commodity_ids_ = std::move(*ids);
      unplaced_.clear();
      known_names_.clear();
      if(auto names{db_.load_micro_resources()}; names)
        for(info::micro_resource_t const & item: *names)
          known_names_.insert(item.name);
      }

    ///\brief where a system lies - from the relay's own messages, or from the systems known here
    [[nodiscard]]
    auto position_of(std::string const & system) -> std::optional<std::array<double, 3>>
      {
      if(auto const it{positions_.find(system)}; it != positions_.end())
        return it->second;
      if(unplaced_.contains(system))
        return std::nullopt;
      std::array<std::string, 1> const names{system};
      if(auto known{db_.load_system_positions(names)}; known and not known->empty())
        return positions_[system] = known->begin()->second;
      unplaced_.insert(system);
      return std::nullopt;
      }

    [[nodiscard]]
    auto within(std::array<double, 3> const & position, double radius) -> bool
      {
      if(eddn_listen::in_reach(position, claims_, radius))
        return true;
      auto const where{here_ ? here_() : std::nullopt};
      return where and eddn_listen::in_reach(position, std::span{&*where, 1u}, radius);
      }

    ///\brief the markets and places around our systems, as other commanders' tools send them
    auto keep_markets(std::string const & text) -> void
      {
      double const radius{eht::settings()->eddn.market_radius_ly};
      if(radius <= 0.0)
        return;
      if(auto event{eddn_listen::journal(text)}; event)
        {
        // a bounded memory: the relay names some thousands of systems an hour
        if(positions_.size() > 50'000u)
          positions_.clear();
        positions_[event->StarSystem] = *event->StarPos;
        unplaced_.erase(event->StarSystem);
        if(event->event != "Docked" or event->MarketID == 0u or not within(*event->StarPos, radius))
          return;
        info::eddn_place_t const place{
          .market_id = event->MarketID,
          .system_address = event->SystemAddress,
          .system = event->StarSystem,
          .station = event->StationName,
          .station_type = event->StationType,
          .dist_from_star_ls = event->DistFromStarLS,
          .pads_small = events::pads_of(event->LandingPads, &events::landing_pads_t::Small),
          .pads_medium = events::pads_of(event->LandingPads, &events::landing_pads_t::Medium),
          .pads_large = events::pads_of(event->LandingPads, &events::landing_pads_t::Large),
          .loc_x = (*event->StarPos)[0],
          .loc_y = (*event->StarPos)[1],
          .loc_z = (*event->StarPos)[2],
          .controlling_faction = event->StationFaction.Name,
          .seen = event->timestamp
        };
        if(auto res{db_.store(place)}; not res)
          spdlog::error("eddn listen: failed to store the place {}", event->MarketID);
        else
          ++heard_.places;
        return;
        }

      auto market{eddn_listen::market(text)};
      if(not market)
        return;
      ++heard_.markets;
      auto const position{position_of(market->systemName)};
      if(not position)
        {
        ++heard_.unplaced;
        return;
        }
      if(not within(*position, radius))
        return;
      ++heard_.in_reach;
      std::vector<info::market_item_t> items;
      for(eddn_listen::commodity_t const & c: market->commodities)
        {
        auto const id{commodity_ids_.find(info::commodity_key(c.name))};
        if(id == commodity_ids_.end())
          {
          ++heard_.unknown_commodities;
          // a rare kind sells at one place only, so it is never in a dictionary filled from one's own markets
          if(unknown_names_.insert(c.name).second)
            spdlog::info(
              "eddn listen: commodity '{}' of {} | {} is not in the dictionary, skipped",
              c.name,
              market->systemName,
              market->stationName
            );
          continue;
          }
        auto const count = [](int64_t v) { return uint32_t(std::clamp<int64_t>(v, 0, std::numeric_limits<uint32_t>::max())); };
        items.push_back(
          info::market_item_t{
            .market_id = market->marketId,
            .commodity_id = id->second,
            .buy_price = count(c.buyPrice),
            .sell_price = count(c.sellPrice),
            .stock = count(c.stock),
            .demand = count(c.demand),
            // the relay does not say what a station produces; what it has to sell stands for it
            .producer = c.stock > 0,
            .consumer = c.demand > 0
          }
        );
        }
      if(auto stored{db_.store_eddn_market(market->marketId, market->timestamp, items)}; not stored)
        spdlog::error("eddn listen: failed to store the market {}", market->marketId);
      else if(*stored)
        ++heard_.stored;
      }

    ///\brief one line on the markets heard since the last one
    auto report_markets() -> void
      {
      reported_ = std::chrono::steady_clock::now();
      if(eht::settings()->eddn.market_radius_ly <= 0.0)
        return;
      spdlog::info(
        "eddn listen: in 10 min {} markets heard, {} in reach, {} kept as newer; {} in systems of no known position; "
        "{} commodities not in the dictionary; {} places kept",
        heard_.markets,
        heard_.in_reach,
        heard_.stored,
        heard_.unplaced,
        heard_.unknown_commodities,
        heard_.places
      );
      heard_ = {};
      }

    auto take(std::string const & packed) -> void
      {
      if(std::chrono::steady_clock::now() - loaded_ > 5min)
        reload_carriers();
      auto text{inflate_message(packed)};
      if(not text)
        return;
      keep_markets(*text);
      if(std::chrono::steady_clock::now() - reported_ > 10min)
        report_markets();
      if(ours_.empty())
        return;
      auto shelf{eddn_listen::fcmaterials(*text)};
      if(not shelf or not ours_.contains(shelf->MarketID))
        return;
      auto oid{db_.carrier_oid(shelf->CarrierID)};
      if(not oid or not *oid)
        {
        spdlog::error("eddn listen: carrier {} not in the database", shelf->CarrierID);
        return;
        }
      for(events::fcmaterial_t const & item: shelf->Items)
        {
        // a kind the bartender was never read for at home gets its internal name; a readable one comes with the
        // next reading in the game
        std::string key{micro_resource_key(item.Name)};
        if(not known_names_.contains(key))
          {
          if(auto res{db_.store(info::micro_resource_t{.name = key, .id = item.id, .localised = {}})}; not res)
            spdlog::error("eddn listen: failed to store micro resource {}", key);
          known_names_.insert(std::move(key));
          }
        if(
          auto res{db_.store(
            info::fcmaterial_t{
              .carrier_id = int64_t(**oid),
              .timestamp = shelf->timestamp.time_since_epoch().count(),
              .material_id = item.id,
              .price = item.Price,
              .stock = item.Stock,
              .demand = item.Demand
            }
          )};
          not res
        )
          spdlog::error("eddn listen: failed to store a shelf row of {}", shelf->CarrierID);
        }
      // the bar sells out of the carrier's store, so the reading is the store's count of the kinds on sale - unless
      // the store changed since, as when one's own reading comes back here after the visit's balance was booked
      if(auto last{db_.last_carrier_micro_change(shelf->MarketID)}; last and holdings::reading_applies(*last, shelf->timestamp))
        for(events::fcmaterial_t const & item: shelf->Items)
          if(auto res{db_.set_carrier_micro(
               shelf->MarketID, micro_resource_key(item.Name), item.Stock, shelf->timestamp, "eddn"
             )};
             not res)
            spdlog::error("eddn listen: failed to set the store of {}", shelf->CarrierID);
      spdlog::info(
        "eddn listen: the bar of {} as of {} UTC, {} kinds",
        shelf->CarrierID,
        std::format("{:%Y-%m-%d %H:%M:%S}", shelf->timestamp),
        shelf->Items.size()
      );
      if(stored_)
        stored_();
      }

    asio::io_context context_;
    database_storage_t & db_;
    std::function<std::string()> const & account_;
    std::function<void()> const & stored_;
    std::stop_token stop_;
    std::set<uint64_t> ours_;
    std::set<std::string> known_names_;
    std::chrono::steady_clock::time_point loaded_{};
    std::function<std::optional<std::array<double, 3>>()> const & here_;
    std::vector<std::array<double, 3>> claims_;
    std::map<std::string, uint64_t> commodity_ids_;
    std::map<std::string, std::array<double, 3>> positions_;
    std::set<std::string> unplaced_;
    ///\brief the commodities not in the dictionary already named in the log
    std::set<std::string> unknown_names_;
    struct heard_t
      {
      uint64_t markets{};
      uint64_t in_reach{};
      uint64_t stored{};
      uint64_t unplaced{};
      uint64_t unknown_commodities{};
      uint64_t places{};
      } heard_;
    std::chrono::steady_clock::time_point reported_{std::chrono::steady_clock::now()};
    };
  }  // namespace

listener_t::listener_t(
  std::string db_path,
  std::function<std::string()> account,
  std::function<void()> stored,
  std::function<std::optional<std::array<double, 3>>()> here
) :
    worker_{[db_path = std::move(db_path),
             account = std::move(account),
             stored = std::move(stored),
             here = std::move(here)](std::stop_token stop)
            {
              eht::event_guard(
                "eddn listen",
                [&]
                {
                  database_storage_t db{db_path};
                  if(auto res{db.open()}; not res)
                    {
                    spdlog::error("eddn listen: failed to open {}", db_path);
                    return;
                    }
                  auto wait{30s};
                  bool listening{};
                  while(not stop.stop_requested())
                    {
                    if(not eht::settings()->eddn.listen)
                      {
                      if(listening)
                        spdlog::info("eddn listen: off");
                      listening = false;
                      std::this_thread::sleep_for(1s);
                      continue;
                      }
                    listening = true;
                    if(session_t{db, account, stored, here, stop}.run())
                      wait = 30s;
                    // the next try waits, longer each time the relay cannot be reached, a second at a time so
                    // that the tool can close
                    for(auto left{wait}; left > 0s and not stop.stop_requested(); left -= 1s)
                      std::this_thread::sleep_for(1s);
                    wait = std::min<std::chrono::seconds>(wait * 2, 300s);
                    }
                }
              );
            }}
  {
  }
  }  // namespace eddn
