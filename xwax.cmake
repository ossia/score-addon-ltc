if(NOT TARGET xwax)
  add_library(xwax STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/xwax/delayline.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/xwax/filters.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/xwax/lut.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/xwax/lut_mk2.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/xwax/pitch_kalman.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/xwax/timecoder.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/xwax/timecoder_mk2.c"
  )
  add_library(xwax::xwax ALIAS xwax)
  target_include_directories(xwax PUBLIC 3rdparty/xwax)

  set_target_properties(xwax PROPERTIES
    UNITY_BUILD OFF
  )
endif()

