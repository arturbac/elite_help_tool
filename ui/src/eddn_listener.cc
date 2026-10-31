#include <eddn_listener.h>

#include <carrier_owner.h>
#include <data/carrier.h>
#include <data/micro_resources.h>
#include <data/progress.h>
#include <databse_storage.h>
#include <eddn_listen.h>
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
#include <set>

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
    session_t(database_storage_t & db, std::function<void()> const & stored, std::stop_token stop) :
        db_{db},
        stored_{stored},
        stop_{std::move(stop)}
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

    ///\brief the fleet carriers of the commander of this tool, by market id - read again every few minutes
    auto reload_carriers() -> void
      {
      loaded_ = std::chrono::steady_clock::now();
      ours_.clear();
      std::string fid;
      if(auto owner{db_.load_owner()}; owner and *owner)
        fid = (*owner)->fid;
      if(auto carriers{db_.load_carriers()}; carriers)
        for(info::carrier_t const & c: *carriers)
          if(c.carrier_type != "SquadronCarrier" and carrier_owner::ours(c, fid))
            ours_.insert(c.market_id);
      known_names_.clear();
      if(auto names{db_.load_micro_resources()}; names)
        for(info::micro_resource_t const & item: *names)
          known_names_.insert(item.name);
      }

    auto take(std::string const & packed) -> void
      {
      if(std::chrono::steady_clock::now() - loaded_ > 5min)
        reload_carriers();
      if(ours_.empty())
        return;
      auto text{inflate_message(packed)};
      if(not text)
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
      if(auto last{db_.last_carrier_micro_change(shelf->MarketID)}; last and (not *last or **last < shelf->timestamp))
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
    std::function<void()> const & stored_;
    std::stop_token stop_;
    std::set<uint64_t> ours_;
    std::set<std::string> known_names_;
    std::chrono::steady_clock::time_point loaded_{};
    };
  }  // namespace

listener_t::listener_t(std::string db_path, std::function<void()> stored) :
    worker_{[db_path = std::move(db_path), stored = std::move(stored)](std::stop_token stop)
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
                    if(session_t{db, stored, stop}.run())
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
