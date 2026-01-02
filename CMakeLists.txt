cmake_minimum_required(VERSION 3.20..4.1.20)
project(EliteHelpTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_EXTENSIONS OFF)


include(cmake/get_cpm.cmake)
find_package(Boost REQUIRED COMPONENTS program_options)

# ----------------------------------------------------------------
# simple_enum
# ----------------------------------------------------------------
find_package(
  simple_enum
  CONFIG
  QUIET)
if(NOT simple_enum_FOUND)
  cpmaddpackage(
    Name
    simple_enum
    GITHUB_REPOSITORY
    arturbac/simple_enum
    GIT_TAG
    v0.9.4)
  add_library(
    simple_enum::simple_enum
    ALIAS
    simple_enum)
endif()
set(HAVE_SIMPLE_ENUM 1)
find_package(
  simple_enum
  CONFIG
  REQUIRED)
  
# ----------------------------------------------------------------
# small_vectors
# ----------------------------------------------------------------
find_package(
  small_vectors
  CONFIG
  QUIET)
if(NOT small_vectors_FOUND)
  cpmaddpackage(
    small_vectors
    GITHUB_REPOSITORY
    arturbac/small_vectors
    GIT_TAG
    v3.3.6)

  add_library(
    small_vectors::small_vectors
    ALIAS
    small_vectors)
endif()
find_package(
  small_vectors
  CONFIG
  REQUIRED)
  
# ----------------------------------------------------------------
# stralgo
# ----------------------------------------------------------------
find_package(
  stralgo
  CONFIG
  QUIET)
if(NOT stralgo_FOUND)
  cpmaddpackage(
    stralgo
    GITHUB_REPOSITORY
    arturbac/stralgo
    GIT_TAG
    v1.4.1
    OPTIONS
    "STRALGO_ENABLE_UNIT_TESTS OFF")

  add_library(
    stralgo::stralgo
    ALIAS
    stralgo)
endif()
find_package(
  stralgo
  CONFIG
  REQUIRED)
  
# ----------------------------------------------------------------
# glaze
# ----------------------------------------------------------------
find_package(glaze QUIET)
if(NOT glaze_FOUND)
  cpmaddpackage(
    glaze
    GITHUB_REPOSITORY
    stephenberry/glaze
    GIT_TAG
    v6.5.0
    OPTIONS
    "glaze_DISABLE_SIMD_WHEN_SUPPORTED ON")
endif()
set(HAVE_GLAZE 1)
find_package(glaze REQUIRED)


# ----------------------------------------------------------------
# spdlog
# ----------------------------------------------------------------
# find_package(spdlog QUIET) if(NOT spdlog_FOUND)
set(SPDLOG_USE_STD_FORMAT ON)
cpmaddpackage(
  NAME
  spdlog
  GITHUB_REPOSITORY
  arturbac/spdlog
  GIT_TAG
  v1.14.1_1
  OPTIONS
  "SPDLOG_USE_STD_FORMAT ON"
  "SPDLOG_BUILD_SHARED ${BUILD_SHARED_LIBS}"
  "SPDLOG_ENABLE_PCH OFF"
  "SPDLOG_BUILD_PIC ON"
  "SPDLOG_SYSTEM_INCLUDES ON")
# endif()
find_package(spdlog REQUIRED)


find_package(Threads REQUIRED)
find_package(
  Boost
  1.70.0
  CONFIG
  REQUIRED
  COMPONENTS system
             thread
             program_options)
             


set(PROJECT_CXX_FLAGS
  "-fPIC"
  # Assume that the destructor of a thrown exception object will not throw. The generated code for catch handlers will be smaller.
  # A throw expression of a type with a potentially-throwing destructor will lead to an error.
  "-fassume-nothrow-exception-dtor"
  "-Weverything"
  # "-Werror"
  "-Wunused-command-line-argument"
  "-Wno-unsafe-buffer-usage"
  "-Wno-float-equal"
  "-Wno-switch-default"
  "-Wno-ctad-maybe-unsupported"
  "-Wno-c++1z-extensions"
  "-Wno-c++98-compat"
  "-Wno-c++98-compat-pedantic"
  "-Wno-c++20-compat"
  "-Wno-c++20-extensions"
  "-Wno-padded"
  "-Wno-misleading-indentation"
  "-Wno-deprecated-builtins"
  "-Wcharacter-conversion"
  "-Wno-double-promotion"
  "-Wno-exit-time-destructors"
  "-Wno-header-hygiene"
  "-Wno-deprecated-declarations"
  "-Wno-missing-noreturn"
  "-Wno-undefined-inline"
  "-Wno-weak-vtables"
  "-Wno-unused-parameter"
  "-Wno-date-time"
  "-Wno-format-nonliteral"
  )
  
set(STRICT_CXX_FLAGS
  "-Werror=sign-conversion"
  "-Werror=shorten-64-to-32"
  "-Werror=nrvo"
  "-Werror=implicit-int-conversion"
  "-Werror=float-conversion"
  "-Werror=implicit-int-float-conversion"
  "-Werror=return-type"
)
add_library(eht)

target_sources(eht
    PUBLIC FILE_SET headers
        TYPE HEADERS
        BASE_DIRS include
)

target_link_libraries(eht PUBLIC
  simple_enum::simple_enum
  small_vectors::small_vectors
  stralgo::stralgo
  glaze::glaze
  )

add_executable(journal_tailer)


target_link_libraries(journal_tailer PRIVATE
  Boost::program_options
  eht
  )

  
add_subdirectory(src)
add_subdirectory(ui)
add_subdirectory(unit_tests)
#add_executable(journal_tests tests.cpp)
#target_link_libraries(journal_tests PRIVATE ut)

