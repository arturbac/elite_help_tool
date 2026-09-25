#include <overlay_ipc.h>

#include <boost/ut.hpp>

#include <unistd.h>

#include <chrono>
#include <format>
#include <thread>

using namespace boost::ut;
using namespace std::chrono_literals;

namespace
  {
///\brief sciezka gniazda unixowego miesci sie w 108 bajtach, wiec trzyma sie krotko
[[nodiscard]]
auto scratch_socket(std::string_view tag) -> std::string
  { return std::format("/tmp/eht_ovl_{}_{}.sock", tag, ::getpid()); }

///\brief czeka az warunek bedzie spelniony, ale nie dluzej niz limit - testy nie moga wisiec
template<typename predicate_t>
[[nodiscard]]
auto wait_until(predicate_t predicate, std::chrono::milliseconds limit = 3000ms) -> bool
  {
  auto const deadline{std::chrono::steady_clock::now() + limit};
  while(std::chrono::steady_clock::now() < deadline)
    {
    if(predicate())
      return true;
    std::this_thread::sleep_for(10ms);
    }
  return predicate();
  }

[[nodiscard]]
auto sample_frame(uint64_t sequence) -> overlay::frame_t
  {
  return overlay::frame_t{
    .seq = sequence,
    .blocks = {overlay::block_t{
      .corner = overlay::corner_e::top_right,
      .ttl_ms = 2500u,
      .lines = {
        overlay::line_t{.text = "Bleia Eohn QT-O d7-43", .color = 0x3cb371u},
        overlay::line_t{.text = "Camorra of Purui 6.3%", .color = 0xd9534fu}
      }
    }}
  };
  }
  }  // namespace

auto main() -> int
  {
  "ramka dociera do klienta w calosci"_test = []
  {
    auto const path{scratch_socket("roundtrip")};
    overlay::server_t server{path};
    expect(server.listening());

    overlay::client_t client{path};
    expect(wait_until([&] { return server.clients() == 1u; })) << "klient sie nie podlaczyl";

    expect(wait_until(
      [&]
      {
        server.publish(sample_frame(7u));
        return client.snapshot() != nullptr;
      }
    )) << "ramka nie dotarla";

    auto const received{client.snapshot()};
    expect(received != nullptr);
    if(received != nullptr)
      {
      expect(received->frame.seq == 7_ul);
      expect(received->frame.blocks.size() == 1_ul);
      expect(received->frame.blocks.front().corner == overlay::corner_e::top_right);
      expect(received->frame.blocks.front().ttl_ms == 2500_u);
      expect(received->frame.blocks.front().lines.size() == 2_ul);
      expect(received->frame.blocks.front().lines.front().text == std::string{"Bleia Eohn QT-O d7-43"});
      expect(received->frame.blocks.front().lines.back().color == 0xd9534fu);
      }
  };

  // typowa kolejnosc: narzedzie chodzi od dawna, gra wstaje pozniej
  "klient podlaczony po publikacji dostaje ostatni obraz"_test = []
  {
    auto const path{scratch_socket("retained")};
    overlay::server_t server{path};
    expect(server.listening());

    server.publish(sample_frame(42u));

    overlay::client_t client{path};
    expect(wait_until([&] { return client.snapshot() != nullptr; })) << "zapamietana ramka nie dotarla";

    auto const received{client.snapshot()};
    expect(received != nullptr);
    if(received != nullptr)
      expect(received->frame.seq == 42_ul);
  };

  // gra potrafi wystartowac przed narzedziem - brak serwera nie moze niczego zepsuc
  "klient bez serwera zyje i nic nie zwraca"_test = []
  {
    overlay::client_t client{scratch_socket("noserver")};
    std::this_thread::sleep_for(200ms);
    expect(not client.connected());
    expect(client.snapshot() == nullptr);
    expect(client.received() == 0_ul);
  };

  // narzedzie mozna zrestartowac w trakcie gry, klient ma sam wrocic
  "klient wraca po restarcie serwera"_test = []
  {
    auto const path{scratch_socket("restart")};
    overlay::client_t client{path};

      {
      overlay::server_t first{path};
      expect(first.listening());
      expect(wait_until([&] { return first.clients() == 1u; })) << "pierwsze polaczenie nie doszlo";
      expect(wait_until(
        [&]
        {
          first.publish(sample_frame(1u));
          return client.received() >= 1u;
        }
      ));
      }

    expect(wait_until([&] { return not client.connected(); })) << "klient nie zauwazyl zniknięcia serwera";

    overlay::server_t second{path};
    expect(second.listening());
    expect(wait_until([&] { return second.clients() == 1u; }, 5000ms)) << "klient nie wrocil";

    auto const before{client.received()};
    expect(wait_until(
      [&]
      {
        second.publish(sample_frame(2u));
        return client.received() > before;
      }
    )) << "po powrocie nie przychodza ramki";
  };

  // serwer moze dzialac bez zadnej gry, publish nie ma prawa na tym polec
  "serwer bez klientow przyjmuje publikacje"_test = []
  {
    overlay::server_t server{scratch_socket("noclient")};
    expect(server.listening());
    for(uint64_t sequence{}; sequence != 100u; ++sequence)
      server.publish(sample_frame(sequence));
    expect(server.clients() == 0_u);
  };
  }
