cmake_minimum_required(VERSION 3.16)

project(vnm_msdf_text
  VERSION 0.1.0
  DESCRIPTION "Small CPU-side MSDF text atlas builder"
  LANGUAGES CXX
)

option(VNM_MSDF_TEXT_USE_SYSTEM_LIBS
  "Prefer system/precompiled FreeType and msdfgen over FetchContent"
  OFF)

include(FetchContent)

function(vnm_msdf_text_set_fetchcontent_build_dir name dir_name)
  string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" _fc_compiler_tag)
  set(_fc_config_suffix "-${_fc_compiler_tag}")
  if(CMAKE_BUILD_TYPE)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _fc_bt_lower)
    string(APPEND _fc_config_suffix "-${_fc_bt_lower}")
  endif()

  set(FETCHCONTENT_BINARY_DIR_${name}
    "${FETCHCONTENT_BASE_DIR}/${dir_name}-build${_fc_config_suffix}"
    CACHE PATH "" FORCE)
endfunction()

set(VNM_MSDF_TEXT_DEFAULT_FONT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/monospace.ttf"
  CACHE FILEPATH "Default MSDF text font file")
get_filename_component(VNM_MSDF_TEXT_DEFAULT_FONT_DIR
  "${VNM_MSDF_TEXT_DEFAULT_FONT_FILE}"
  DIRECTORY)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/workspace_deps_overrides.cmake OPTIONAL)
if(VNM_MSDF_TEXT_DEP_OVERRIDES_FILE AND EXISTS "${VNM_MSDF_TEXT_DEP_OVERRIDES_FILE}")
  message(STATUS "vnm_msdf_text: Loading dependency overrides from ${VNM_MSDF_TEXT_DEP_OVERRIDES_FILE}")
  include("${VNM_MSDF_TEXT_DEP_OVERRIDES_FILE}")
endif()

if(VNM_MSDF_TEXT_USE_SYSTEM_LIBS)
  find_package(Freetype QUIET)
  if(FREETYPE_FOUND)
    if(TARGET Freetype::Freetype)
      # ok
    elseif(TARGET freetype)
      add_library(Freetype::Freetype ALIAS freetype)
    else()
      add_library(Freetype::Freetype INTERFACE IMPORTED)
      set_target_properties(Freetype::Freetype PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${FREETYPE_LIBRARIES}")
    endif()
  endif()

  find_package(msdfgen QUIET)
endif()

if(TARGET Freetype::Freetype)
  message(STATUS "vnm_msdf_text: Using existing Freetype::Freetype target")
elseif(TARGET freetype)
  add_library(Freetype::Freetype ALIAS freetype)
  message(STATUS "vnm_msdf_text: Using existing freetype target")
else()
  if(FETCHCONTENT_SOURCE_DIR_FREETYPE AND
     EXISTS "${FETCHCONTENT_SOURCE_DIR_FREETYPE}/CMakeLists.txt")
    message(STATUS "vnm_msdf_text: Using local freetype checkout: ${FETCHCONTENT_SOURCE_DIR_FREETYPE}")
  else()
    message(STATUS "vnm_msdf_text: Fetching freetype")
  endif()
  vnm_msdf_text_set_fetchcontent_build_dir(freetype freetype)
  FetchContent_Declare(freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG VER-2-13-3
    GIT_SHALLOW FALSE
    BINARY_DIR "${FETCHCONTENT_BINARY_DIR_freetype}"
  )

  set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
  set(FT_ENABLE_ERROR_STRINGS OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(freetype)
  if(TARGET freetype AND NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
  endif()
endif()

if(TARGET msdfgen::msdfgen-core AND TARGET msdfgen::msdfgen-ext)
  message(STATUS "vnm_msdf_text: Using existing msdfgen targets")
else()
  if(FETCHCONTENT_SOURCE_DIR_MSDFGEN AND
     EXISTS "${FETCHCONTENT_SOURCE_DIR_MSDFGEN}/CMakeLists.txt")
    message(STATUS "vnm_msdf_text: Using local msdfgen checkout: ${FETCHCONTENT_SOURCE_DIR_MSDFGEN}")
  else()
    message(STATUS "vnm_msdf_text: Fetching msdfgen")
  endif()
  vnm_msdf_text_set_fetchcontent_build_dir(msdfgen msdfgen)
  FetchContent_Declare(msdfgen
    GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
    GIT_TAG v1.12.1
    GIT_SHALLOW FALSE
    BINARY_DIR "${FETCHCONTENT_BINARY_DIR_msdfgen}"
  )
  set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
  set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
  set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
  set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
  set(MSDFGEN_USE_SKIA OFF CACHE BOOL "" FORCE)
  set(MSDFGEN_CORE_ONLY OFF CACHE BOOL "" FORCE)
  if(MSVC)
    set(MSDFGEN_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
  endif()
  FetchContent_MakeAvailable(msdfgen)
endif()

foreach(VNM_MSDF_TEXT_MSDFGEN_TARGET msdfgen-core msdfgen-ext)
  if(TARGET ${VNM_MSDF_TEXT_MSDFGEN_TARGET})
    get_target_property(
      VNM_MSDF_TEXT_MSDFGEN_PUBLIC_HEADERS
      ${VNM_MSDF_TEXT_MSDFGEN_TARGET}
      PUBLIC_HEADER)
    if(VNM_MSDF_TEXT_MSDFGEN_PUBLIC_HEADERS)
      set(VNM_MSDF_TEXT_MSDFGEN_ABSOLUTE_PUBLIC_HEADERS)
      foreach(VNM_MSDF_TEXT_MSDFGEN_PUBLIC_HEADER ${VNM_MSDF_TEXT_MSDFGEN_PUBLIC_HEADERS})
        if(IS_ABSOLUTE "${VNM_MSDF_TEXT_MSDFGEN_PUBLIC_HEADER}")
          list(APPEND
            VNM_MSDF_TEXT_MSDFGEN_ABSOLUTE_PUBLIC_HEADERS
            "${VNM_MSDF_TEXT_MSDFGEN_PUBLIC_HEADER}")
        else()
          list(APPEND
            VNM_MSDF_TEXT_MSDFGEN_ABSOLUTE_PUBLIC_HEADERS
            "${msdfgen_SOURCE_DIR}/${VNM_MSDF_TEXT_MSDFGEN_PUBLIC_HEADER}")
        endif()
      endforeach()
      set_target_properties(${VNM_MSDF_TEXT_MSDFGEN_TARGET}
        PROPERTIES
          PUBLIC_HEADER "${VNM_MSDF_TEXT_MSDFGEN_ABSOLUTE_PUBLIC_HEADERS}")
    endif()
  endif()
endforeach()

add_library(vnm_msdf_text STATIC
  src/msdf_text.cpp
  include/vnm_msdf_text/msdf_text.h
)

add_library(vnm_msdf_text::vnm_msdf_text ALIAS vnm_msdf_text)

target_include_directories(vnm_msdf_text
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(vnm_msdf_text PUBLIC cxx_std_14)

target_link_libraries(vnm_msdf_text
  PUBLIC
    msdfgen::msdfgen-ext
    Freetype::Freetype
    msdfgen::msdfgen-core
)

if(WIN32)
  target_compile_definitions(vnm_msdf_text PRIVATE NOMINMAX)
endif()

if(MSVC)
  target_compile_options(vnm_msdf_text PRIVATE /W4)
else()
  target_compile_options(vnm_msdf_text PRIVATE -Wall -Wextra)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(GNUInstallDirs)
  install(DIRECTORY include/vnm_msdf_text
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(TARGETS vnm_msdf_text
    EXPORT vnm_msdf_textTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()
