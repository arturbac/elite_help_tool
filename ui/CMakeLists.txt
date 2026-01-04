find_package(Qt6 REQUIRED COMPONENTS Widgets #[[Sql]])
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(elite_help_tool)

target_link_libraries(elite_help_tool PRIVATE
  eht
  Qt6::Widgets 
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
)
add_subdirectory(src)
