cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

# FFmpeg libraries (from prebuilt obs-deps)
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY NAMES avformat)
find_library(AVCODEC_LIBRARY NAMES avcodec)
find_library(AVUTIL_LIBRARY NAMES avutil)

if(NOT AVFORMAT_LIBRARY OR NOT AVCODEC_LIBRARY OR NOT AVUTIL_LIBRARY)
  message(FATAL_ERROR "FFmpeg libraries (avformat, avcodec, avutil) not found in CMAKE_PREFIX_PATH. "
    "These are required for video trimming functionality.")
endif()

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${AVFORMAT_INCLUDE_DIR})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})

# Plugin sources
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/main.cpp
    src/plugin/plugin.cpp
    src/plugin/plugin.hpp
    src/utils/obs-utils.cpp
    src/utils/obs-utils.hpp
    src/utils/duration-format.cpp
    src/utils/duration-format.hpp
    src/utils/video-trimmer.cpp
    src/utils/video-trimmer.hpp
    src/utils/logger.hpp
    src/ui/ui-components.cpp
    src/ui/ui-components.hpp
    src/managers/settings-manager.cpp
    src/managers/settings-manager.hpp
    src/managers/save-button-settings.cpp
    src/managers/save-button-settings.hpp
    src/managers/replay-buffer-manager.cpp
    src/managers/replay-buffer-manager.hpp
    src/managers/hotkey-manager.cpp
    src/managers/hotkey-manager.hpp
    src/config/config.hpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Release package configuration
set(RELEASE_DIR "${CMAKE_BINARY_DIR}/release-package/obs-studio")
set(RELEASE_PLUGIN_DIR "${RELEASE_DIR}/obs-plugins/64bit")
set(RELEASE_DATA_DIR "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}")
set(RELEASE_ZIP_DIR "${CMAKE_BINARY_DIR}/releases/${PROJECT_VERSION}")
set(RELEASE_ZIP "${RELEASE_ZIP_DIR}/${CMAKE_PROJECT_NAME}-windows-x64.zip")

add_custom_target(prepare_release
  DEPENDS ${CMAKE_PROJECT_NAME}
  # Create directory structure
  COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_PLUGIN_DIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_DATA_DIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_ZIP_DIR}"

  # Copy plugin DLL
  COMMAND ${CMAKE_COMMAND} -E copy
    "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
    "${RELEASE_PLUGIN_DIR}/"

  # Copy data files (locale)
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/data"
    "${RELEASE_DATA_DIR}"

  # Create ZIP file
  COMMAND ${CMAKE_COMMAND} -E tar "cf" "${RELEASE_ZIP}" --format=zip
    "${RELEASE_DIR}/"

  COMMENT "Creating release package..."
)
