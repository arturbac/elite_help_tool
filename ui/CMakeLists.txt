find_package(Qt6 REQUIRED COMPONENTS Widgets Charts #[[Sql]])
# EDDN: HTTPS through Boost.Beast, the messages gzipped
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
# the hot drop reads the HUD's text with tesseract; without it the tool builds and the hot drop records nothing
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT IMPORTED_TARGET tesseract)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(elite_help_tool)
# Qt's code generators wait for every library the target links, so moc and rcc of this target would start
# only after libeht.a, and with them every object of the tool. moc needs nothing from those libraries
set_target_properties(elite_help_tool PROPERTIES AUTOGEN_ORIGIN_DEPENDS OFF)

# the window's icon; eht.png is eht.svg drawn at 256 px (rsvg-convert -w 256 -h 256). A target of its own,
# because rcc run for elite_help_tool would wait for libeht.a
add_library(eht_icons OBJECT icons/eht.qrc)
set_target_properties(eht_icons PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_link_libraries(eht_icons PRIVATE Qt6::Core)

target_link_libraries(elite_help_tool PRIVATE
  eht_icons
  eht
  eht_overlay_ipc
  Qt6::Widgets 
  Qt6::Charts
  OpenSSL::SSL
  OpenSSL::Crypto
  ZLIB::ZLIB
  # Qt6::Sql
  )
if(TESSERACT_FOUND)
  target_link_libraries(elite_help_tool PRIVATE PkgConfig::TESSERACT)
  target_compile_definitions(elite_help_tool PRIVATE EHT_HAVE_TESSERACT=1)
else()
  message(STATUS "tesseract not found - the hot drop cannot read the HUD")
  target_compile_definitions(elite_help_tool PRIVATE EHT_HAVE_TESSERACT=0)
endif()
target_sources(elite_help_tool
    PUBLIC FILE_SET headers
        TYPE HEADERS
        BASE_DIRS include
        FILES 
            include/journal_log.h
            include/logic.h
            include/qformat.h
            include/system_window.h
            include/ship_loadout.h
            include/main_window.h
            include/mission_window.h
            include/route_window.h
            include/faction_window.h
            include/faction_state_window.h
            include/micro_resource_window.h
            include/bgs_window.h
            include/construction_window.h
            include/network_incident_window.h
            include/credits_window.h
            include/ships_window.h
            include/surface_window.h
            include/overlay_feed.h
            include/sky_album.h
            include/glare_watch.h
            include/hud_reader.h
            include/sensor_watch.h
            include/netstate_watch.h
            include/eht_extension.h
)
add_subdirectory(src)

# An optional extension from outside this repository: a directory with a CMakeLists.txt that makes a
# library named eht_extension, defining eht::extension::make_extension. Without one the tool gets the
# empty implementation
set(EHT_EXTENSION_DIR "" CACHE PATH "directory of an optional extension built into the tool")
if(EHT_EXTENSION_DIR AND EXISTS "${EHT_EXTENSION_DIR}/CMakeLists.txt")
  add_subdirectory("${EHT_EXTENSION_DIR}" "${CMAKE_BINARY_DIR}/eht_extension")
  target_link_libraries(elite_help_tool PRIVATE eht_extension)
  message(STATUS "elite_help_tool: extension from ${EHT_EXTENSION_DIR}")
else()
  target_sources(elite_help_tool PRIVATE src/eht_extension_none.cc)
endif()
