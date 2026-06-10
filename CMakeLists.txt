cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(score_addon_ltc LANGUAGES CXX)

# Use the Avendish the host (ossia score) provides; otherwise fetch it.
find_package(Avendish QUIET)
if(NOT Avendish_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    Avendish
    GIT_REPOSITORY "https://github.com/celtera/avendish"
    GIT_TAG main)
  FetchContent_Populate(Avendish)
  list(APPEND CMAKE_PREFIX_PATH "${avendish_SOURCE_DIR}")
  find_package(Avendish REQUIRED)
endif()

include(ltc.cmake)
include(xwax.cmake)

if(COMMAND score_common_setup)
  score_common_setup()
endif()

# Object dependencies, linked so they reach the score base target / every back-end.
add_library(ltc_deps INTERFACE)
target_link_libraries(ltc_deps INTERFACE ltc::ltc xwax::xwax Qt::Core)

avnd_addon_init(NAME score_addon_ltc)

avnd_addon_object(
  BASE score_addon_ltc C_NAME ltc_generator CLASS LTCGenerator NAMESPACE ao
  DEPENDENCIES ltc_deps
  SOURCES LTC/LTC.hpp LTC/LTC.cpp)

avnd_addon_object(
  BASE score_addon_ltc C_NAME avnd_xwax_dvs CLASS XWaxDVS NAMESPACE ao
  DEPENDENCIES ltc_deps
  SOURCES LTC/XWax.hpp LTC/XWax.cpp)

avnd_addon_object(
  BASE score_addon_ltc C_NAME ltc_input CLASS LTCInput NAMESPACE ao
  DEPENDENCIES ltc_deps
  SOURCES LTC/LTCInput.hpp)

avnd_addon_finalize(NAME score_addon_ltc UUID d62cfa0c-e539-4b8d-aca8-d3674862a5d2 VERSION 1)
