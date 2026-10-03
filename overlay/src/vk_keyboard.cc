#include "vk_keyboard.h"
#include "vk_dispatch.h"
#include "vk_service.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <dlfcn.h>
#include <unistd.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace eht_overlay
  {
namespace
  {
  ///\brief libxcb taken from the game's own process - Wine has it loaded already, so this costs nothing
  ///\detail xcb rather than Xlib: Xlib reports errors to one handler for the whole process, and in the
  /// game that handler is Wine's. xcb hands every error back with the reply it belongs to
  struct xcb_t
    {
#define EHT_XCB(name) \
  decltype(&::name) name {}
    EHT_XCB(xcb_connect);
    EHT_XCB(xcb_connection_has_error);
    EHT_XCB(xcb_disconnect);
    EHT_XCB(xcb_get_setup);
    EHT_XCB(xcb_setup_roots_iterator);
    EHT_XCB(xcb_screen_next);
    EHT_XCB(xcb_query_keymap);
    EHT_XCB(xcb_query_keymap_reply);
    EHT_XCB(xcb_get_keyboard_mapping);
    EHT_XCB(xcb_get_keyboard_mapping_reply);
    EHT_XCB(xcb_get_keyboard_mapping_keysyms);
    EHT_XCB(xcb_get_keyboard_mapping_keysyms_length);
    EHT_XCB(xcb_intern_atom);
    EHT_XCB(xcb_intern_atom_reply);
    EHT_XCB(xcb_get_property);
    EHT_XCB(xcb_get_property_reply);
    EHT_XCB(xcb_get_property_value);
    EHT_XCB(xcb_get_property_value_length);
#undef EHT_XCB

    void * library{};

    xcb_t() = default;
    xcb_t(xcb_t const &) = delete;
    auto operator=(xcb_t const &) -> xcb_t & = delete;

    ~xcb_t()
      {
      if(library != nullptr)
        ::dlclose(library);
      }

    [[nodiscard]]
    auto load() -> bool
      {
      library = ::dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
      if(library == nullptr)
        return false;
      bool complete{true};
      auto const take = [&]<typename function_t>(function_t & target, char const * name)
      {
        target = reinterpret_cast<function_t>(::dlsym(library, name));
        complete = complete and target != nullptr;
      };
#define EHT_XCB(name) take(name, #name)
      EHT_XCB(xcb_connect);
      EHT_XCB(xcb_connection_has_error);
      EHT_XCB(xcb_disconnect);
      EHT_XCB(xcb_get_setup);
      EHT_XCB(xcb_setup_roots_iterator);
      EHT_XCB(xcb_screen_next);
      EHT_XCB(xcb_query_keymap);
      EHT_XCB(xcb_query_keymap_reply);
      EHT_XCB(xcb_get_keyboard_mapping);
      EHT_XCB(xcb_get_keyboard_mapping_reply);
      EHT_XCB(xcb_get_keyboard_mapping_keysyms);
      EHT_XCB(xcb_get_keyboard_mapping_keysyms_length);
      EHT_XCB(xcb_intern_atom);
      EHT_XCB(xcb_intern_atom_reply);
      EHT_XCB(xcb_get_property);
      EHT_XCB(xcb_get_property_reply);
      EHT_XCB(xcb_get_property_value);
      EHT_XCB(xcb_get_property_value_length);
#undef EHT_XCB
      return complete;
      }
    };

  ///\brief a reply from xcb, freed with free() as xcb wants
  template<typename reply_t>
  struct reply_ptr_t
    {
    reply_t * reply{};

    explicit reply_ptr_t(reply_t * r) noexcept : reply{r} {}

    reply_ptr_t(reply_ptr_t const &) = delete;
    auto operator=(reply_ptr_t const &) -> reply_ptr_t & = delete;

    ~reply_ptr_t() { std::free(reply); }

    auto operator->() const noexcept -> reply_t * { return reply; }

    explicit operator bool() const noexcept { return reply != nullptr; }
    };

  ///\brief the keysym of a key named the way xev and xmodmap name it
  ///\detail not the whole of keysymdef.h - the keys one would take a screenshot with: the function keys,
  /// the few beside them, a letter or a digit, and any other as a number, 0xffc8 say
  [[nodiscard]]
  auto keysym_of(std::string_view name) noexcept -> xcb_keysym_t
    {
    if(name.size() == 1u)
      {
      char const c{name.front()};
      // letters are lower case in the keyboard map; the shifted ones are the second column
      if(c >= 'A' and c <= 'Z')
        return xcb_keysym_t(c - 'A' + 'a');
      if((c >= 'a' and c <= 'z') or (c >= '0' and c <= '9'))
        return xcb_keysym_t(c);
      return 0u;
      }
    if(name.starts_with("0x"))
      {
      xcb_keysym_t value{};
      auto const [end, error]{std::from_chars(name.data() + 2, name.data() + name.size(), value, 16)};
      return error == std::errc{} and end == name.data() + name.size() ? value : 0u;
      }
    if(name.size() > 1u and name.front() == 'F')
      {
      unsigned number{};
      auto const [end, error]{std::from_chars(name.data() + 1, name.data() + name.size(), number)};
      if(error == std::errc{} and end == name.data() + name.size() and number >= 1u and number <= 35u)
        return xcb_keysym_t(0xffbeu + number - 1u);
      return 0u;
      }

    struct named_t
      {
      std::string_view name;
      xcb_keysym_t keysym;
      };

    static constexpr std::array known{
      named_t{"Print", 0xff61u},
      named_t{"Pause", 0xff13u},
      named_t{"Scroll_Lock", 0xff14u},
      named_t{"Insert", 0xff63u},
      named_t{"Delete", 0xffffu},
      named_t{"Home", 0xff50u},
      named_t{"End", 0xff57u},
      named_t{"Prior", 0xff55u},
      named_t{"Page_Up", 0xff55u},
      named_t{"Next", 0xff56u},
      named_t{"Page_Down", 0xff56u},
      named_t{"KP_Multiply", 0xffaau},
      named_t{"KP_Add", 0xffabu},
      named_t{"KP_Subtract", 0xffadu},
      named_t{"KP_Divide", 0xffafu},
      named_t{"KP_Enter", 0xff8du},
    };
    for(named_t const & entry: known)
      if(entry.name == name)
        return entry.keysym;
    return 0u;
    }

  ///\brief the value of one variable in a process' environment, empty when it cannot be read
  [[nodiscard]]
  auto environment_of(uint32_t pid, std::string_view variable) -> std::string
    {
    std::ifstream file{std::format("/proc/{}/environ", pid), std::ios::binary};
    if(not file)
      return {};
    std::string const whole{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    std::string const wanted{std::string{variable} + '='};
    for(size_t at{}; at < whole.size();)
      {
      size_t const end{std::min(whole.find('\0', at), whole.size())};
      if(std::string_view const entry{whole.data() + at, end - at}; entry.starts_with(wanted))
        return std::string{entry.substr(wanted.size())};
      at = end + 1u;
      }
    return {};
    }

  class watcher_t
    {
  public:
    watcher_t() : thread_{[this] { run(); }} {}

    watcher_t(watcher_t const &) = delete;
    auto operator=(watcher_t const &) -> watcher_t & = delete;

    ~watcher_t()
      {
        {
        std::scoped_lock const lock{mutex_};
        stopping_ = true;
        }
      wake_.notify_one();
      thread_.join();
      if(connection_ != nullptr)
        xcb_.xcb_disconnect(connection_);
      }

    [[nodiscard]]
    auto take() noexcept -> bool
      { return presses_.exchange(0u) != 0u; }

  private:
    xcb_t xcb_;
    xcb_connection_t * connection_{};
    xcb_window_t root_{};
    xcb_atom_t active_window_{};
    xcb_atom_t window_pid_{};
    std::string key_;
    xcb_keycode_t keycode_{};
    bool was_down_{};
    std::atomic<uint32_t> presses_{};
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_{};
    // last: it runs over everything above
    std::thread thread_;

    ///rief waits that long, or less when the watcher is being stopped - false then
    [[nodiscard]]
    auto pause(std::chrono::milliseconds length) -> bool
      {
      std::unique_lock lock{mutex_};
      return not wake_.wait_for(lock, length, [this] { return stopping_; });
      }

    auto run() -> void
      {
      if(not xcb_.load())
        {
        log("screenshot key not watched, no libxcb in the game's process");
        return;
        }
      do
        {
        if(connection_ == nullptr and not connect())
          continue;
        if(xcb_.xcb_connection_has_error(connection_) != 0)
          {
          log("the X connection watching the screenshot key broke, connecting again");
          xcb_.xcb_disconnect(connection_);
          connection_ = nullptr;
          keycode_ = 0u;
          continue;
          }
        poll();
        }
      while(pause(std::chrono::milliseconds{connection_ != nullptr ? 30 : 5000}));
      }

    [[nodiscard]]
    auto connect() -> bool
      {
      // DISPLAY is the one the game draws on - Proton passes it into the container
      int screen{};
      xcb_connection_t * const connection{xcb_.xcb_connect(nullptr, &screen)};
      if(xcb_.xcb_connection_has_error(connection) != 0)
        {
        xcb_.xcb_disconnect(connection);
        return false;
        }
      xcb_screen_iterator_t roots{xcb_.xcb_setup_roots_iterator(xcb_.xcb_get_setup(connection))};
      for(; screen > 0 and roots.rem > 1; --screen)
        xcb_.xcb_screen_next(&roots);
      if(roots.data == nullptr)
        {
        xcb_.xcb_disconnect(connection);
        return false;
        }
      connection_ = connection;
      root_ = roots.data->root;
      active_window_ = atom("_NET_ACTIVE_WINDOW");
      window_pid_ = atom("_NET_WM_PID");
      log("watching the screenshot key on the X server");
      return true;
      }

    [[nodiscard]]
    auto atom(std::string_view name) -> xcb_atom_t
      {
      reply_ptr_t const reply{xcb_.xcb_intern_atom_reply(
        connection_, xcb_.xcb_intern_atom(connection_, 1u, uint16_t(name.size()), name.data()), nullptr
      )};
      return reply ? reply->atom : xcb_atom_t{};
      }

    ///\brief the key's code on this keyboard - looked up again whenever the tool asks for another key
    [[nodiscard]]
    auto keycode_of(xcb_keysym_t keysym) -> xcb_keycode_t
      {
      xcb_setup_t const * const setup{xcb_.xcb_get_setup(connection_)};
      uint8_t const count{uint8_t(setup->max_keycode - setup->min_keycode + 1)};
      reply_ptr_t const reply{xcb_.xcb_get_keyboard_mapping_reply(
        connection_, xcb_.xcb_get_keyboard_mapping(connection_, setup->min_keycode, count), nullptr
      )};
      if(not reply or reply->keysyms_per_keycode == 0u)
        return 0u;
      xcb_keysym_t const * const keysyms{xcb_.xcb_get_keyboard_mapping_keysyms(reply.reply)};
      int const total{xcb_.xcb_get_keyboard_mapping_keysyms_length(reply.reply)};
      for(int index{}; index < total; ++index)
        if(keysyms[index] == keysym)
          return xcb_keycode_t(setup->min_keycode + index / reply->keysyms_per_keycode);
      return 0u;
      }

    [[nodiscard]]
    auto property(xcb_window_t window, xcb_atom_t name, xcb_atom_t type) -> std::optional<uint32_t>
      {
      if(name == xcb_atom_t{})
        return std::nullopt;
      reply_ptr_t const reply{xcb_.xcb_get_property_reply(
        connection_, xcb_.xcb_get_property(connection_, 0u, window, name, type, 0u, 1u), nullptr
      )};
      if(not reply or reply->format != 32u or xcb_.xcb_get_property_value_length(reply.reply) < 4)
        return std::nullopt;
      return *static_cast<uint32_t const *>(xcb_.xcb_get_property_value(reply.reply));
      }

    ///\brief whether the window the player is looking at is this game's
    ///\detail XWayland hears the key while any X window has the focus - the other account's game, the Steam
    /// client. The active window's process is this one, or one of the same Wine prefix: under a virtual
    /// desktop the window belongs to explorer.exe. What cannot be told counts as ours - better a picture
    /// too many than a key that never works
    [[nodiscard]]
    auto game_has_focus() -> bool
      {
      auto const window{property(root_, active_window_, XCB_ATOM_WINDOW)};
      if(not window or *window == 0u)
        return true;
      auto const pid{property(*window, window_pid_, XCB_ATOM_CARDINAL)};
      if(not pid)
        return true;
      if(*pid == uint32_t(::getpid()))
        return true;
      char const * const ours{std::getenv("WINEPREFIX")};
      std::string const theirs{environment_of(*pid, "WINEPREFIX")};
      if(ours == nullptr or *ours == '\0' or theirs.empty())
        return true;
      return theirs == ours;
      }

    auto poll() -> void
      {
      // no frame for a moment - the tool restarting, say - changes nothing; the key stays what it was
      auto const snapshot{ipc_client().snapshot()};
      if(not snapshot)
        return;
      bool const changed{snapshot->frame.screenshot.key != key_};
      if(changed)
        {
        key_ = snapshot->frame.screenshot.key;
        keycode_ = key_.empty() ? xcb_keycode_t{} : keycode_of(keysym_of(key_));
        if(not key_.empty() and keycode_ == 0u)
          log("screenshot key {} not found on the keyboard", key_);
        }
      if(keycode_ == 0u)
        return;

      reply_ptr_t const keymap{xcb_.xcb_query_keymap_reply(connection_, xcb_.xcb_query_keymap(connection_), nullptr)};
      if(not keymap)
        return;
      bool const down{(keymap->keys[keycode_ / 8u] & (1u << (keycode_ % 8u))) != 0u};
      // the press, not the holding - one picture however long the key stays down, and a key newly asked
      // for that happens to be held is no press either
      if(down and not was_down_ and not changed and game_has_focus())
        {
        presses_.fetch_add(1u);
        log("screenshot key {} pressed", key_);
        }
      was_down_ = down;
      }
    };

  service_t<watcher_t> watcher;
  }  // namespace

auto take_screenshot_press() noexcept -> bool
  {
  try
    {
    return watcher.get([] { return std::make_unique<watcher_t>(); }).take();
    }
  catch(...)
    {
    // asked every frame, so said only the first time
    static std::atomic_flag said;
    if(not said.test_and_set())
      report("overlay: screenshot key watcher failed ({})", exception_text());
    return false;
    }
  }

auto stop_keyboard() noexcept -> void
  { watcher.stop(); }
  }  // namespace eht_overlay
