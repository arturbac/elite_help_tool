#include <eddn_sender.h>
#include <eht_settings.h>
#include <event_guard.h>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>
#include <zlib.h>

#include <chrono>
#include <exception>
#include <format>
#include <optional>

namespace eddn
  {
namespace
  {
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

constexpr std::chrono::seconds request_timeout{10};
constexpr std::chrono::milliseconds between_messages{400};
constexpr std::chrono::minutes after_failure{5};

struct url_t
  {
  std::string host;
  std::string port;
  std::string target;
  };

///\brief https://host[:port]/path - the one shape the gateway's address takes
[[nodiscard]]
auto parse_url(std::string_view url) -> std::optional<url_t>
  {
  constexpr std::string_view scheme{"https://"};
  if(not url.starts_with(scheme))
    return std::nullopt;
  url.remove_prefix(scheme.size());
  auto const slash{url.find('/')};
  std::string_view authority{url.substr(0, slash)};
  std::string_view const target{slash == std::string_view::npos ? std::string_view{"/"} : url.substr(slash)};
  std::string_view port{"443"};
  if(auto const colon{authority.find(':')}; colon != std::string_view::npos)
    {
    port = authority.substr(colon + 1u);
    authority = authority.substr(0, colon);
    }
  if(authority.empty())
    return std::nullopt;
  return url_t{.host = std::string{authority}, .port = std::string{port}, .target = std::string{target}};
  }

[[nodiscard]]
auto gzip(std::string const & text) -> std::optional<std::string>
  {
  z_stream stream{};
  // 15 bits of window, and 16 more for the gzip wrapper rather than the bare zlib one
  if(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    return std::nullopt;
  std::string out(deflateBound(&stream, uLong(text.size())), '\0');
  stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(text.data()));
  stream.avail_in = uInt(text.size());
  stream.next_out = reinterpret_cast<Bytef *>(out.data());
  stream.avail_out = uInt(out.size());
  int const result{deflate(&stream, Z_FINISH)};
  out.resize(stream.total_out);
  deflateEnd(&stream);
  if(result != Z_STREAM_END)
    return std::nullopt;
  return out;
  }

struct reply_t
  {
  unsigned status{};
  std::string body;
  std::string error;
  };

[[nodiscard]]
auto post(url_t const & url, std::string const & body) -> reply_t
  {
  try
    {
    asio::io_context io;
    asio::ssl::context tls{asio::ssl::context::tls_client};
    tls.set_default_verify_paths();
    tls.set_verify_mode(asio::ssl::verify_peer);
    tls.set_verify_callback(asio::ssl::host_name_verification(url.host));

    beast::ssl_stream<beast::tcp_stream> stream{io, tls};
    // the name the certificate is asked for - without it the gateway's server answers for nobody
    if(not SSL_set_tlsext_host_name(stream.native_handle(), url.host.c_str()))
      return reply_t{.status = 0u, .body = {}, .error = "the server name could not be set"};

    asio::ip::tcp::resolver resolver{io};
    reply_t reply{};
    bool done{};
    // every step asynchronous: the timeouts of tcp_stream bind only those, and a server that stops
    // answering would otherwise hold the sender - and the closing of the tool, which waits for it - for ever
    asio::co_spawn(
      io,
      [&]() -> asio::awaitable<void>
      {
        auto const endpoints{co_await resolver.async_resolve(url.host, url.port, asio::use_awaitable)};
        beast::get_lowest_layer(stream).expires_after(request_timeout);
        co_await beast::get_lowest_layer(stream).async_connect(endpoints, asio::use_awaitable);
        beast::get_lowest_layer(stream).expires_after(request_timeout);
        co_await stream.async_handshake(asio::ssl::stream_base::client, asio::use_awaitable);

        http::request<http::string_body> request{http::verb::post, url.target, 11};
        // on a port other than the usual one the server knows its site only by name and port together
        request.set(http::field::host, url.port == "443" ? url.host : std::format("{}:{}", url.host, url.port));
        request.set(http::field::user_agent, std::format("{}/{}", software_name, software_version));
        request.set(http::field::content_type, "application/json");
        request.set(http::field::content_encoding, "gzip");
        request.body() = body;
        request.prepare_payload();

        beast::get_lowest_layer(stream).expires_after(request_timeout);
        co_await http::async_write(stream, request, asio::use_awaitable);

        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        beast::get_lowest_layer(stream).expires_after(request_timeout);
        co_await http::async_read(stream, buffer, response, asio::use_awaitable);

        // the answer is in; a server that closes without the TLS goodbye changes nothing
        beast::get_lowest_layer(stream).expires_after(request_timeout);
        [[maybe_unused]]
        auto const closed{co_await stream.async_shutdown(asio::as_tuple(asio::use_awaitable))};
        reply = reply_t{.status = response.result_int(), .body = std::move(response.body()), .error = {}};
      },
      [&](std::exception_ptr const & error)
      {
        done = true;
        if(error)
          try
            {
            std::rethrow_exception(error);
            }
          catch(std::exception const & e)
            {
            reply.error = e.what();
            }
          catch(...)
            {
            reply.error = "an unknown exception";
            }
      }
    );
    // the name lookup has no timeout of its own, so the whole exchange has one as well
    io.run_for(4 * request_timeout);
    if(not done)
      {
      // what is still pending is cancelled and let finish, so that nothing outlives the objects it uses
      resolver.cancel();
      beast::get_lowest_layer(stream).close();
      io.restart();
      io.run_for(std::chrono::seconds{1});
      return reply_t{.status = 0u, .body = {}, .error = "no answer in time"};
      }
    return reply;
    }
  catch(std::exception const & e)
    {
    return reply_t{.status = 0u, .body = {}, .error = e.what()};
    }
  }

///\brief a refusal that another try would not change
[[nodiscard]]
auto refused_for_good(reply_t const & reply) -> bool
  {
  return reply.status == 400u or reply.status == 413u or reply.body.contains("is unknown, unable to validate");
  }
  }  // namespace

sender_t::sender_t() :
    worker_{[this](std::stop_token stop) { eht::event_guard("eddn sender", [&] { run(std::move(stop)); }); }}
  {
  }

sender_t::~sender_t()
  {
  worker_.request_stop();
  wake_.notify_all();
  }

auto sender_t::enqueue(message_t && message) -> void
  {
  {
  std::lock_guard const lock{mutex_};
  queue_.push_back(std::move(message));
  }
  wake_.notify_all();
  }

auto sender_t::run(std::stop_token stop) -> void
  {
  while(not stop.stop_requested())
    {
    message_t message;
    {
    std::unique_lock lock{mutex_};
    if(not wake_.wait(lock, stop, [this] { return not queue_.empty(); }))
      return;
    message = queue_.front();
    }

    std::optional<url_t> const url{parse_url(eht::settings()->eddn.upload_url)};
    auto const body{gzip(message.envelope)};
    if(not url or not body)
      {
      spdlog::error("eddn: the gateway's address or the message is unusable, {} dropped", message.schema);
      std::lock_guard const lock{mutex_};
      queue_.pop_front();
      continue;
      }

    reply_t const reply{post(*url, *body)};
    if(reply.status == 200u or refused_for_good(reply))
      {
      if(reply.status == 200u)
        spdlog::info("eddn: {} sent", message.schema);
      else
        spdlog::warn("eddn: {} refused ({}): {}", message.schema, reply.status, reply.body);
      {
      std::lock_guard const lock{mutex_};
      queue_.pop_front();
      }
      std::unique_lock lock{mutex_};
      wake_.wait_for(lock, stop, between_messages, [] { return false; });
      continue;
      }

    std::unique_lock lock{mutex_};
    spdlog::warn(
      "eddn: {} not sent ({} {}), {} waiting, next try in {} min",
      message.schema,
      reply.status,
      reply.error.empty() ? reply.body : reply.error,
      queue_.size(),
      after_failure.count()
    );
    wake_.wait_for(lock, stop, after_failure, [] { return false; });
    }
  }
  }  // namespace eddn
