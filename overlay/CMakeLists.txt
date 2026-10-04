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
  VERSION
  1.91.9
  # tag v1.91.9b, pinned to its commit - a tag can be moved
  GIT_TAG
  f5befd2d29e66809cd1110a152e375a7f1981f06
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
# czcionka wkompilowana w warstwe - w kontenerze gry czcionek systemu nie widac
# (Noto Sans Mono, SIL OFL 1.1, fonts/OFL.txt)
# ----------------------------------------------------------------
set(EHT_OVERLAY_FONT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/fonts/NotoSansMono-Regular.ttf)
set(EHT_OVERLAY_FONT_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/overlay_font.cc)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${EHT_OVERLAY_FONT_FILE})
file(READ ${EHT_OVERLAY_FONT_FILE} eht_font_hex HEX)
string(LENGTH "${eht_font_hex}" eht_font_hex_length)
math(EXPR eht_font_size "${eht_font_hex_length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," eht_font_bytes "${eht_font_hex}")
file(WRITE ${EHT_OVERLAY_FONT_SOURCE}
  "// generated from fonts/NotoSansMono-Regular.ttf by overlay/CMakeLists.txt\n"
  "#include \"overlay_font.h\"\n"
  "namespace eht_overlay\n  {\n"
  "alignas(4) unsigned char const overlay_font_data[] = {${eht_font_bytes}};\n"
  "std::size_t const overlay_font_size = ${eht_font_size}u;\n"
  "  }\n")

# ----------------------------------------------------------------
# sama warstwa i plugin z overlayem
# ----------------------------------------------------------------
# the layer the loader knows is thin: it tracks the game's objects and hands them to the plugin,
# which it loads again whenever the plugin's file is replaced
option(EHT_OVERLAY_SANITIZE "build the layer and the plugin with ASan and LSan, for the headless check" OFF)
if(EHT_OVERLAY_SANITIZE)
  set(eht_overlay_runtime_options -fsanitize=address -fno-omit-frame-pointer)
  # the shared runtime lies beside the compiler, where the loader does not look
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libclang_rt.asan-x86_64.so
    OUTPUT_VARIABLE eht_asan_runtime
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  get_filename_component(eht_asan_runtime_dir ${eht_asan_runtime} DIRECTORY)
  set(eht_overlay_runtime_link -fsanitize=address -shared-libasan -Wl,-rpath,${eht_asan_runtime_dir})
else()
  # the game's process has a libstdc++ of its own, older or newer - ours goes in whole and hidden
  set(eht_overlay_runtime_options)
  set(eht_overlay_runtime_link -static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL)
endif()

add_library(eht_overlay SHARED
  src/vk_layer.cc
  src/vk_log.cc)
target_include_directories(eht_overlay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(eht_overlay PRIVATE Vulkan::Headers)
target_compile_options(eht_overlay PRIVATE ${eht_overlay_runtime_options})
set_target_properties(eht_overlay PROPERTIES
  OUTPUT_NAME eht_overlay
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)
# the loader unloads the layer at vkDestroyInstance, while a game thread may still be inside a hook
# of it; the plugin, whose threads are all joined, is what gets unloaded
target_link_options(eht_overlay PRIVATE
  -Wl,-z,nodelete
  ${eht_overlay_runtime_link}
  -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/eht_overlay.version
  -Wl,--no-undefined)

add_library(eht_overlay_plugin SHARED
  src/vk_plugin.cc
  src/vk_log.cc
  src/vk_draw.cc
  src/vk_capture.cc
  src/vk_faces.cc
  src/vk_sample.cc
  src/vk_keyboard.cc
  src/vk_world.cc
  ${EHT_OVERLAY_FONT_SOURCE})
target_include_directories(eht_overlay_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(eht_overlay_plugin PRIVATE
  eht_overlay_ipc
  eht_imgui
  Vulkan::Headers)
target_compile_options(eht_overlay_plugin PRIVATE ${eht_overlay_runtime_options})
set_target_properties(eht_overlay_plugin PROPERTIES
  OUTPUT_NAME eht_overlay_plugin
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)
target_link_options(eht_overlay_plugin PRIVATE
  ${eht_overlay_runtime_link}
  -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/eht_overlay_plugin.version
  -Wl,--no-undefined)
if(EHT_OVERLAY_SANITIZE)
  target_compile_options(eht_imgui PRIVATE ${eht_overlay_runtime_options})
  target_compile_options(eht_overlay_ipc PRIVATE ${eht_overlay_runtime_options})
  # the tool's side links it too
  target_link_options(eht_overlay_ipc INTERFACE ${eht_overlay_runtime_link})
endif()
# the layer finds the plugin beside itself
add_dependencies(eht_overlay eht_overlay_plugin)

# ----------------------------------------------------------------
# opakowanie do %command% i zrodlo testowe
# ----------------------------------------------------------------
add_executable(eht-overlay-run src/launcher.cc)
target_include_directories(eht-overlay-run PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# tylko dla default_socket_path z naglowka protokolu
target_link_libraries(eht-overlay-run PRIVATE simple_enum::simple_enum glaze::glaze)

# jedyny cel ktory naprawde linkuje loader - reszta overlaya go nie dotyka
add_executable(eht-overlay-headless-check tools/headless_check.cc)
target_link_libraries(eht-overlay-headless-check PRIVATE Vulkan::Vulkan)
# the process must start with the sanitizer for the layer's and the plugin's leaks to be reported
if(EHT_OVERLAY_SANITIZE)
  target_compile_options(eht-overlay-headless-check PRIVATE ${eht_overlay_runtime_options})
  target_link_options(eht-overlay-headless-check PRIVATE ${eht_overlay_runtime_link})
endif()

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
  # copied beside and renamed over the old one: a game running keeps the library it loaded, where
  # writing into that same file would pull it from under the game
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:eht_overlay> ${EHT_OVERLAY_HOME_LIB}/libeht_overlay.so.new
  COMMAND ${CMAKE_COMMAND} -E rename ${EHT_OVERLAY_HOME_LIB}/libeht_overlay.so.new ${EHT_OVERLAY_HOME_LIB}/libeht_overlay.so
  # the same for the plugin: a running game sees the new file within a second and loads it
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:eht_overlay_plugin> ${EHT_OVERLAY_HOME_LIB}/libeht_overlay_plugin.so.new
  COMMAND ${CMAKE_COMMAND} -E rename ${EHT_OVERLAY_HOME_LIB}/libeht_overlay_plugin.so.new ${EHT_OVERLAY_HOME_LIB}/libeht_overlay_plugin.so
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:eht-overlay-run> ${EHT_OVERLAY_HOME_BIN}/eht-overlay-run
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/eht_overlay.json ${EHT_OVERLAY_HOME_MANIFEST}/eht_overlay.json
  DEPENDS eht_overlay eht_overlay_plugin eht-overlay-run
  COMMENT "instalacja warstwy do ~/.local - tylko stamtad widzi ja kontener Steam Linux Runtime")
