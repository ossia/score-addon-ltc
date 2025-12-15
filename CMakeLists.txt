cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

if(NOT TARGET score_lib_base)
  include("${SCORE_SOURCE_DIR}/cmake/ScoreExternalAddon.cmake")
endif()

if(NOT TARGET score_plugin_avnd)
  return()
endif()

project(score_addon_ltc LANGUAGES CXX)

include(ltc.cmake)
include(xwax.cmake)

score_common_setup()

avnd_score_plugin_init(
  BASE_TARGET score_addon_ltc
)

avnd_score_plugin_add(
  BASE_TARGET score_addon_ltc
  SOURCES
    LTC/LTC.hpp
    LTC/LTC.cpp
  TARGET ltc_generator
  MAIN_CLASS LTCGenerator
  NAMESPACE ao
)

avnd_score_plugin_add(
  BASE_TARGET score_addon_ltc
  SOURCES
    LTC/XWax.hpp
    LTC/XWax.cpp
  TARGET avnd_xwax_dvs
  MAIN_CLASS XWaxDVS
  NAMESPACE ao
)
target_link_libraries(score_addon_ltc PRIVATE ltc::ltc xwax::xwax Qt::Core)

avnd_score_plugin_finalize(
  BASE_TARGET score_addon_ltc
  PLUGIN_VERSION 1
  PLUGIN_UUID "d62cfa0c-e539-4b8d-aca8-d3674862a5d2"
)

