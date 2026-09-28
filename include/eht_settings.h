#pragma once

#include <overlay_protocol.h>

#include <glaze/glaze.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>
#include <thread>

///\brief what used to be written into the code and is a matter of taste or of judgement
///
/// Everything a player might want to tune - how the overlay looks and where it stands, how long its
/// lists are, the thresholds behind the hints - sits in one JSON file beside the tool, created with
/// these defaults when it is missing and read again whenever it is saved. What is a fact about the
/// game stays in the code: which minerals can only be mined, which letter is which star class, how
/// many won days end a war. Those are not settings, and a file inviting anyone to change them would
/// only let them be wrong.
namespace eht
  {
///\brief a colour, written in the file as "#rrggbb" so that it can be read and picked by eye
struct colour_t
  {
  uint32_t rgb{};

  auto read(std::string const & text) -> void;
  [[nodiscard]]
  auto write() const -> std::string;
  };

struct overlay_colours_t
  {
  colour_t heading{0x9ad1ffu};
  colour_t plain{0xddddddu};
  ///\brief what asks for attention without being lost yet
  colour_t alert{0xd9a34au};
  ///\brief the good news - a mission ready to hand in, the best offer
  colour_t first{0x3cb371u};
  ///\brief a mission about to run out
  colour_t expiring{0xd9534fu};
  colour_t tick_influence{0xddddddu};
  colour_t tick_war{0xd9534fu};
  };

///\brief how many rows each list in the overlay shows
struct overlay_lists_t
  {
  uint32_t bodies{5u};
  uint32_t factions{5u};
  uint32_t missions{6u};
  uint32_t cargo{4u};
  uint32_t commodities{3u};
  uint32_t sources{2u};
  uint32_t settlement_work{4u};
  ///\brief how tall a column of the settlements-by-owner list shown on foot grows, in lines - the list
  /// goes on in the next column, as in a newspaper, up to three
  uint32_t settlement_rows{16u};
  ///\brief commodities listed for a construction site on the overlay, the most wanted first; 0 lists them
  /// all - what the port sells may well be at the end of the list
  uint32_t construction{0u};
  ///\brief how many characters of the small text a line of that list may take, all its columns
  /// together; lower it on a narrow side band, and the list keeps to fewer columns
  uint32_t settlement_line_chars{110u};
  ///\brief a settlement's name longer than this is cut and ends in "..." - one in nine is, the longest
  /// run to 40 characters, and the economy after it is what matters when choosing work
  uint32_t settlement_name_chars{28u};
  uint32_t trades{3u};
  uint32_t route_hops{10u};
  ///\brief the ships nearby, the nearest first
  uint32_t ships{8u};
  };

///\brief how often the overlay asks again - the database and the files it reads change at their own pace
struct overlay_refresh_t
  {
  uint32_t factions_s{60u};
  uint32_t market_s{5u};
  uint32_t supply_s{10u};
  uint32_t status_ms{500u};
  ///\brief how often the tool looks whether the picture changed - on foot the distances to the samples
  /// move with every step, and a readout that lags behind the walk is no help
  uint32_t publish_ms{500u};
  ///\brief a frame is sent at least this often even when nothing changed, so the layer knows we live
  uint32_t heartbeat_s{3u};
  ///\brief a block fades this long after its last refresh
  uint32_t block_ttl_ms{10000u};
  };

struct overlay_chart_t
  {
  uint32_t days{};
  ///\brief the plot itself, in pixels at scale 1
  uint32_t height{};
  };

///\brief the picture of the system in the corner of the right band
///\detail the sizes keep an order, not a scale - a star twice a planet, a giant above a rocky world
struct system_map_t
  {
  ///\brief how much taller than the rest of the overlay it is drawn
  float zoom{2.f};
  ///\brief what part of the band's width it takes
  float share{0.5f};
  float star_radius{10.f};
  float giant_radius{6.5f};
  float planet_radius{4.5f};
  float moon_radius{2.8f};
  float submoon_radius{2.f};
  float port_size{4.2f};
  colour_t line{0x5a6470u};
  colour_t label{0xa8b0bau};
  ///\brief the ring round the body we are at
  colour_t here{0x40e0ffu};
  ///\brief the ring round the body or port we are going to
  colour_t destination{0xffd24au};
  ///\brief over a body's own colour wherever there is life to sample
  colour_t bio{0x3cb371u};
  colour_t port{0xc8d0dcu};
  ///\brief the arrow by a body or port an open mission sends us to; one waiting to be handed in takes
  /// the colour of good news instead
  colour_t mission{0xff9a4au};
  float mission_arrow{6.f};
  };

///\brief the superpower's emblem in the panel of a hyperspace jump being charged
///\detail the game shows the right emblem only for an independent system and a wrong one for the Federation, the
/// Empire and the Alliance, so for those three the overlay paints the panel's middle over and draws the right
/// one. The place is on the middle screen, from its centre, in shares of its height
struct jump_emblem_t
  {
  bool enabled{true};
  float x{0.0014f};
  float y{-0.1808f};
  float width{0.085f};
  float height{0.040f};
  float emblem_height{0.030f};
  ///\brief the panel's own black, under the patch
  colour_t ground{0x020304u};
  ///\brief the factions of the system being jumped to, under the panel while the drive charges - names,
  /// influence and which way it went at the last tick
  bool factions{true};
  ///\brief the top of that list from the middle screen's centre, and its widest, in shares of its height
  float factions_y{-0.105f};
  float factions_width{0.6f};
  };

struct overlay_settings_t
  {
  overlay::layout_t layout;
  overlay_colours_t colours;
  overlay_lists_t lists;
  overlay_refresh_t refresh;
  ///\brief a body worth less than this is not listed as worth mapping
  uint32_t minimum_body_value{300000u};
  ///\brief a mission with less than this left is shown as about to run out
  uint32_t expiry_warning_h{3u};
  ///\brief how far back a crime with a bounty counts as probably still held against the commander - the
  /// squadron's Notoriety Decay clears older ones unseen
  uint32_t bounty_days{7u};
  ///\brief how far from the galactic average a price has to be to be worth a line
  double interesting_deviation{0.25};
  ///\brief and by how many credits a tonne at the least
  uint32_t interesting_margin{500u};
  ///\brief how long a kill stays on the target readout
  uint32_t kill_shown_s{10u};
  ///\brief how far a ship may stand to be listed as nearby; 0 turns the list off
  double ships_radius_ly{100.0};
  ///\brief and flown within this many days - the rest are kept, not used; 0 lists every one nearby
  uint32_t ships_flown_days{30u};
  overlay_chart_t influence_chart{.days = 20u, .height = 74u};
  overlay_chart_t tick_chart{.days = 30u, .height = 45u};
  system_map_t system_map;
  jump_emblem_t jump_emblem;
  };

///\brief exploration - what is worth a landing and where the pictures of what was sampled go
struct exploration_settings_t
  {
  ///\brief a species paying less than this is not worth sampling - the base value, before any bonus
  uint32_t bio_worth{5'000'000u};
  ///\brief how many bodies with life the overlay lists
  uint32_t bio_bodies{4u};
  ///\brief how many guesses at a genus' species are shown beside it
  uint32_t candidates{2u};
  ///\brief what a species below the worth is written in - still there, but not asking for attention
  colour_t below_worth{0x8a8a8au};
  ///\brief a genus whose one scan here would widen what is known of where it grows - worth it whatever it pays
  colour_t new_knowledge{0x7fb8ffu};
  ///\brief below this many finds of a genus on worlds like this one, the world counts as little known
  uint32_t little_known{3u};
  ///\brief where a white dwarf's danger begins, in its own radii - measured on the HUD at one DC dwarf, where the
  /// game gives no warning at all and the heat comes too late to be one; 0 shows no such line
  ///\brief the arrival star after a jump and the planet after the surface scanner, photographed without asking
  bool sky_pictures{true};
  ///\brief the picture's height as a share of the screen's, in the middle screen's own 16:9 shape
  float sky_size{0.8f};
  ///\brief when after the jump the star is taken, one picture for each - several while the moment worth it is
  /// being found out. At 0 s the tunnel is still on the screen; at 1 s the star is there
  std::vector<uint32_t> sky_star_delays_ms{1000u};
  ///\brief how long after the scanner closes the planet is taken - the cockpit comes back round it first
  uint32_t sky_planet_delay_ms{1500u};
  float white_dwarf_danger_radii{78.f};
  ///\brief how many measurements the figure above rests on, said beside it so it is read for what it is
  uint32_t white_dwarf_measurements{1u};
  ///\brief a picture of the middle of the screen is taken at every sample - the plant is right there
  bool capture{true};
  ///\brief the side of the square taken, as a share of the screen's height
  float capture_size{0.6f};
  ///\brief how long after the scan the picture is taken - the scan's rings and the sampler sealing the
  /// sample cover the plant first; the layer shows the frame and counts down meanwhile
  uint32_t capture_delay_ms{5000u};
  ///\brief where the pictures and the codex page are kept, relative to where the tool runs
  std::string codex_dir{"codex"};
  ///\brief other accounts' galaxy.sqlite, read for their finds - the knowledge of species is shared, the codex not
  std::vector<std::string> shared_galaxies{};
  uint32_t jpeg_quality{90u};
  ///\brief pictures of the surface scanner's view are kept per body and shown in the left band on the
  /// ground - its filters say where each genus grows, and the orbit is far from the Nomad
  bool scanner_pictures{true};
  ///\brief how often the open scanner is photographed
  uint32_t scanner_interval_ms{1000u};
  ///\brief the side of the square taken, as a share of the screen's height - the globe and the filter's name
  float scanner_size{0.75f};
  ///\brief how different, 0..255 on average, a view must be from every one kept to be kept as another
  float scanner_difference{12.f};
  ///\brief the most views kept of one body
  uint32_t scanner_views{12u};
  ///\brief the side of the thumbnails in the band, in pixels
  uint32_t scanner_thumbnail{400u};
  ///\brief how many thumbnails stand side by side
  uint32_t scanner_columns{4u};
  };

///\brief screenshots of the whole screen, overlay and all, taken by the layer at a key
struct screenshot_settings_t
  {
  ///\brief the X key name - F11, Print, Pause, KP_Multiply, a letter, or a keysym as 0xffc8; empty turns them off
  std::string key{"F11"};
  ///\brief where they are kept, relative to where the tool runs
  std::string dir{"screenshots"};
  ///\brief png keeps the overlay's text sharp; jpg is a fraction of the size
  std::string format{"png"};
  uint32_t jpeg_quality{92u};
  };

struct trade_settings_t
  {
  ///\brief a rate on fewer tonnes than this in stock or in demand is no rate
  uint32_t minimum_quantity{50u};
  };

///\brief the judgement behind reading the tick out of influence changes
struct tick_settings_t
  {
  ///\brief recalculations further apart than this belong to different waves
  uint32_t same_wave_gap_h{8u};
  ///\brief how much earlier than seen a wave is reported - a reading with the old value proves only
  /// that the new one had not reached us, so erring early is the safe side for handing missions in
  uint32_t client_lag_min{5u};
  ///\brief a window between two readings wider than this says nothing about when the tick came
  uint32_t max_window_h{24u};
  };

struct journal_settings_t
  {
  ///\brief how often the journal being written is looked at for new lines
  uint32_t tail_poll_ms{50u};
  ///\brief how often the directory is looked at for a newer journal
  uint32_t new_file_check_s{2u};
  ///\brief how often the mark of how far the journal was read is written down while playing
  uint32_t progress_write_s{60u};
  };

///\brief what goes to the Elite Dangerous Data Network, and from whom
///\detail Nothing goes unless enabled and the commander's FID is on a list. What an account on the
/// exploration list sends is the scans of systems nobody discovered before and nobody lives in - never a
/// populated system, where the data draws griefers to the influence work. What an account on the
/// bartender list sends is a carrier bar's stock, at the moment the game writes it
struct eddn_settings_t
  {
  bool enabled{false};
  ///\brief the test schemas: the gateway checks the messages, but they reach no one listening
  bool test{true};
  std::vector<std::string> exploration_commanders{};
  std::vector<std::string> bartender_commanders{};
  std::string upload_url{"https://eddn.edcd.io:4430/upload/"};
  };

struct windows_settings_t
  {
  ///\brief a move of influence smaller than this, in points, is not shown in the BGS window
  double bgs_smallest_move{0.3};
  ///\brief how long a finished faction state stays in the faction window
  uint32_t faction_keep_finished_h{24u};
  ///\brief a state lasting longer than this is taken for a missed ending
  uint32_t faction_max_duration_d{7u};
  uint32_t faction_tick_refresh_s{60u};
  };

///\brief the colours of the tool's own windows
struct gui_settings_t
  {
  ///\brief a body's value class, in the system window's table
  colour_t value_high{0x1166ffu};
  colour_t value_medium{0xffd700u};
  ///\brief the system's name at the top of the system window, by the value of its star
  colour_t system_high{0x1144aau};
  colour_t system_medium{0xffd700u};
  ///\brief the ticked columns of the table - terraformable, mapped, discovered
  colour_t flag_column{0x22aa22u};
  ///\brief the column saying someone mapped the body before us
  colour_t mapped_before_column{0xff3322u};
  ///\brief a mission's status in the mission window
  colour_t mission_ready{0x2fd700u};
  colour_t mission_open{0x1136ffu};
  colour_t ship_header{0xffad33u};
  colour_t hull_bar{0xcc4444u};
  colour_t fuel_bar{0x4444ccu};
  colour_t cargo_bar{0x44cc44u};
  colour_t module_healthy{0x2ecc71u};
  colour_t module_damaged{0xe74c3cu};
  colour_t module_on{0x00ff00u};
  colour_t module_off{0xff4444u};
  ///\brief a module below this share of its health is shown as damaged
  double module_damaged_below{0.4};
  };

///\brief the copy of the journals and the codex's pictures, made in the background
struct backup_settings_t
  {
  bool enabled{true};
  ///\brief a directory of its own for each commander goes under it
  std::string dir{"~/.backups/eht"};
  ///\brief a backup once this many days have passed since the last - 0 never for the time alone
  uint32_t every_days{30u};
  ///\brief and once this many new pictures came into the codex and the sky album - 0 never for the pictures
  uint32_t every_pictures{100u};
  ///\brief zstd's level for the journals - 9 packs them some seventy times smaller in seconds
  int32_t level{9};
  };

struct settings_t
  {
  overlay_settings_t overlay;
  exploration_settings_t exploration;
  screenshot_settings_t screenshots;
  gui_settings_t gui;
  trade_settings_t trade;
  tick_settings_t ticks;
  journal_settings_t journal;
  windows_settings_t windows;
  eddn_settings_t eddn;
  backup_settings_t backup;
  };

///\brief switches read from the file when someone put them there, and never written into a file of
/// defaults - they are for one commander's own setup, not settings to offer everyone
struct private_settings_t
  {
  bool sjona_private{};
  };

///\brief the private switches in force now
[[nodiscard]]
auto private_settings() noexcept -> private_settings_t;

///\brief the name of the file, looked for in the directory the tool runs in
inline constexpr std::string_view settings_file_name{"eht_settings.json"};

///\brief the settings in force now - a snapshot, so a reload never changes them under a reader's feet
[[nodiscard]]
auto settings() -> std::shared_ptr<settings_t const>;

///\brief reads the file into the settings in force, or writes it with the defaults when it is missing
///\detail a file missing some keys - written by an older version - is filled in and written back, so
/// every setting is always there to be found. A file that does not parse is left alone and the
/// settings in force stay as they were
///\returns false when the file exists but could not be read
auto load_settings(std::filesystem::path const & path) -> bool;

///\brief watches the file and reloads it whenever it is saved
class settings_watcher_t
  {
public:
  explicit settings_watcher_t(std::filesystem::path path);

private:
  std::filesystem::path path_;
  std::jthread worker_;
  };
  }  // namespace eht

template<>
struct glz::meta<eht::colour_t>
  {
  static constexpr auto value{glz::custom<&eht::colour_t::read, &eht::colour_t::write>};
  };
