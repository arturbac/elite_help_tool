find_package(Qt6 REQUIRED COMPONENTS Widgets Charts #[[Sql]])
# EDDN: HTTPS through Boost.Beast, the messages gzipped
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(elite_help_tool)

target_link_libraries(elite_help_tool PRIVATE
  eht
  eht_overlay_ipc
  Qt6::Widgets 
  Qt6::Charts
  OpenSSL::SSL
  OpenSSL::Crypto
  ZLIB::ZLIB
  # Qt6::Sql
  )
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
            include/ships_window.h
            include/surface_window.h
            include/overlay_feed.h
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
