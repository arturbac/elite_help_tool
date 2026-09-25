# ----------------------------------------------------------------
# overlay - warstwa Vulkana rysujaca w oknie gry
#
# nie linkuje loadera Vulkana, nie linkuje eht ani Qt. jedyne co trafia do procesu gry
# to ta biblioteka, wiec wszystko co ma w srodku jest statyczne i schowane
# ----------------------------------------------------------------
find_package(Vulkan REQUIRED)

cpmaddpackage(
  NAME
  imgui
  GITHUB_REPOSITORY
  ocornut/imgui
  GIT_TAG
  v1.91.9b
  DOWNLOAD_ONLY
  YES)

add_library(eht_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)

target_include_directories(eht_imgui SYSTEM PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends)
target_include_directories(eht_imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(eht_imgui PUBLIC Vulkan::Headers)
target_compile_definitions(eht_imgui PUBLIC
  IMGUI_IMPL_VULKAN_NO_PROTOTYPES=1
  IMGUI_USER_CONFIG="eht_imgui_config.h")
# cudzy kod, nie nasze ostrzezenia
target_compile_options(eht_imgui PRIVATE -w)
set_target_properties(eht_imgui PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)

# ----------------------------------------------------------------
# wspolne ipc - klient siedzi w grze, serwer w elite_help_tool
# ----------------------------------------------------------------
add_library(eht_overlay_ipc STATIC src/overlay_ipc.cc)
target_include_directories(eht_overlay_ipc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(eht_overlay_ipc PUBLIC
  glaze::glaze
  simple_enum::simple_enum
  Threads::Threads)
set_target_properties(eht_overlay_ipc PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)

# ----------------------------------------------------------------
# sama warstwa
# ----------------------------------------------------------------
add_library(eht_overlay SHARED
  src/vk_layer.cc
  src/vk_draw.cc)
target_link_libraries(eht_overlay PRIVATE
  eht_overlay_ipc
  eht_imgui
  Vulkan::Headers)
set_target_properties(eht_overlay PROPERTIES
  OUTPUT_NAME eht_overlay
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)
target_link_options(eht_overlay PRIVATE
  -static-libstdc++
  -static-libgcc
  -Wl,--exclude-libs,ALL
  -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/eht_overlay.version
  -Wl,--no-undefined)

# ----------------------------------------------------------------
# opakowanie do %command% i zrodlo testowe
# ----------------------------------------------------------------
add_executable(eht-overlay-run src/launcher.cc)
target_include_directories(eht-overlay-run PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# tylko dla default_socket_path z naglowka protokolu
target_link_libraries(eht-overlay-run PRIVATE simple_enum::simple_enum glaze::glaze)

add_executable(eht-overlay-feed src/feed.cc)
target_link_libraries(eht-overlay-feed PRIVATE eht_overlay_ipc)

# ----------------------------------------------------------------
# instalacja
#
# kontener Steam Linux Runtime widzi katalog domowy, ale nie widzi katalogu budowania,
# wiec warstwa musi fizycznie wyladowac pod $HOME - inaczej gra jej nie znajdzie
# ----------------------------------------------------------------
set(EHT_OVERLAY_HOME_LIB "$ENV{HOME}/.local/lib")
set(EHT_OVERLAY_HOME_BIN "$ENV{HOME}/.local/bin")
set(EHT_OVERLAY_HOME_MANIFEST "$ENV{HOME}/.local/share/vulkan/implicit_layer.d")
set(EHT_OVERLAY_LIBRARY_PATH "${EHT_OVERLAY_HOME_LIB}/libeht_overlay.so")

configure_file(eht_overlay.json.in ${CMAKE_CURRENT_BINARY_DIR}/eht_overlay.json @ONLY)

add_custom_target(overlay-install
  COMMAND ${CMAKE_COMMAND} -E make_directory ${EHT_OVERLAY_HOME_LIB}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${EHT_OVERLAY_HOME_BIN}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${EHT_OVERLAY_HOME_MANIFEST}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:eht_overlay> ${EHT_OVERLAY_HOME_LIB}/libeht_overlay.so
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:eht-overlay-run> ${EHT_OVERLAY_HOME_BIN}/eht-overlay-run
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/eht_overlay.json ${EHT_OVERLAY_HOME_MANIFEST}/eht_overlay.json
  DEPENDS eht_overlay eht-overlay-run
  COMMENT "instalacja warstwy do ~/.local - tylko stamtad widzi ja kontener Steam Linux Runtime")
