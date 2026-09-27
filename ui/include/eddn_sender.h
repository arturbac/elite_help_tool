#pragma once

#include <eddn_publisher.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

namespace eddn
  {
///\brief takes the messages to the gateway, on a thread of its own, the way EDMC does
///\detail Gzipped compact JSON posted over HTTPS, ten seconds allowed. Accepted - or refused for good,
/// with a 400, a 413 or an unknown schema - a message is done with; any other failure keeps it for
/// another try five minutes later. Messages go at most one every 400 ms. What is queued lives in memory
/// only: the scans are safe on disk until the sale anyway, and a bar stock is worth nothing an hour on.
class sender_t final
  {
public:
  sender_t();
  ~sender_t();
  sender_t(sender_t const &) = delete;
  auto operator=(sender_t const &) -> sender_t & = delete;

  auto enqueue(message_t && message) -> void;

private:
  std::mutex mutex_;
  std::condition_variable_any wake_;
  std::deque<message_t> queue_;
  std::jthread worker_;

  auto run(std::stop_token stop) -> void;
  };
  }  // namespace eddn
