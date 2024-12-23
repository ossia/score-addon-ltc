if(OSSIA_USE_SYSTEM_LIBRARIES)
    find_library(LTC_LIBRARY NAMES ltc)
    find_path(LTC_INCLUDE_DIR ltc.h)

    if(LTC_LIBRARY AND LTC_INCLUDE_DIR)
      add_library(ltc INTERFACE IMPORTED GLOBAL)
      add_library(ltc::ltc ALIAS ltc)
      target_include_directories(ltc INTERFACE ${LTC_INCLUDE_DIR})
      target_link_libraries(ltc INTERFACE ${LTC_LIBRARY})
    endif()
endif()


if(NOT TARGET ltc)
    add_library(ltc STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libltc/src/encoder.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libltc/src/decoder.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libltc/src/ltc.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libltc/src/timecode.c"
    )
    add_library(ltc::ltc ALIAS ltc)
    target_include_directories(ltc PUBLIC 3rdparty/libltc/src)
endif()

