cmake_minimum_required(VERSION 3.16)

# On macOS, pull in prebuilt obs-deps (Qt6, FFmpeg, OBS) before project() so
# that find_package() can locate them via CMAKE_PREFIX_PATH.
if(APPLE)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/common"
                                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos")

  # Default install prefix: OBS user plugins directory
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX
      "$ENV{HOME}/Library/Application Support/obs-studio/plugins"
      CACHE STRING "Default plugin installation directory" FORCE)
  endif()

  # Use RPATH from bundle layout so Qt resolves from OBS Frameworks/
  set(CMAKE_MACOSX_RPATH TRUE)
  set(CMAKE_SKIP_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
  set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

  # Download prebuilt obs-deps, Qt6, and OBS Studio source/build at configure time
  include(buildspec)
endif()

project(replay-buffer-pro VERSION 1.4.0)

# Find Qt
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

# Find OBS
if(WIN32)
  get_filename_component(OBS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../obs-studio" ABSOLUTE)

  if(NOT EXISTS "${OBS_ROOT_DIR}")
    message(FATAL_ERROR "OBS Studio source not found at ${OBS_ROOT_DIR}. Please clone obs-studio repository at the same level as this plugin.")
  endif()

  # Find OBS deps directory that contains FFmpeg libraries
  file(GLOB OBS_DEPS_DIRS "${OBS_ROOT_DIR}/.deps/obs-deps-*")
  set(OBS_DEPS_DIR "")
  
  # Look for a deps directory that has FFmpeg libraries
  foreach(DEPS_DIR ${OBS_DEPS_DIRS})
    if(EXISTS "${DEPS_DIR}/lib/avformat.lib")
      set(OBS_DEPS_DIR "${DEPS_DIR}")
      break()
    endif()
  endforeach()
  
  if(NOT OBS_DEPS_DIR)
    message(FATAL_ERROR "OBS deps with FFmpeg libraries not found. Expected directory like ${OBS_ROOT_DIR}/.deps/obs-deps-* with avformat.lib")
  endif()
  
  message(STATUS "Using OBS deps with FFmpeg: ${OBS_DEPS_DIR}")

  set(obs_INCLUDE_DIRS
    "${OBS_ROOT_DIR}/libobs"
    "${OBS_ROOT_DIR}/frontend/api"
    "${OBS_ROOT_DIR}/build/config"
    "${OBS_DEPS_DIR}/include"  # FFmpeg headers from OBS deps
  )

  set(OBS_LIBRARIES
    "${OBS_ROOT_DIR}/build/libobs/$<CONFIG>/obs.lib"
    "${OBS_ROOT_DIR}/build/frontend/api/$<CONFIG>/obs-frontend-api.lib"
  )

  # FFmpeg libraries from OBS dependencies
  set(FFMPEG_LIBRARIES
    "${OBS_DEPS_DIR}/lib/avformat.lib"
    "${OBS_DEPS_DIR}/lib/avcodec.lib"
    "${OBS_DEPS_DIR}/lib/avutil.lib"
  )
elseif(APPLE)
  # macOS: Use prebuilt obs-deps (downloaded at configure time via buildspec.cmake).
  # OBS and Qt6 are found via CMAKE_PREFIX_PATH set by buildspec.cmake.
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)

  # FFmpeg: prebuilt obs-deps ships dylibs but no .pc / CMake config files.
  # Locate them directly inside .deps using find_library.
  set(_deps_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-2025-07-11-universal")
  find_library(AVFORMAT_LIB avformat HINTS "${_deps_dir}/lib" REQUIRED)
  find_library(AVCODEC_LIB  avcodec  HINTS "${_deps_dir}/lib" REQUIRED)
  find_library(AVUTIL_LIB   avutil   HINTS "${_deps_dir}/lib" REQUIRED)
  set(FFMPEG_LIBRARIES ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB})
  set(FFMPEG_INCLUDE_DIRS "${_deps_dir}/include")
  message(STATUS "Using prebuilt FFmpeg: ${AVFORMAT_LIB}")
else()
  # Linux: Find OBS via CMake package config (installed via cmake --install or distro package).
  # OBS 28+ ships libobs and obs-frontend-api as separate CMake packages.
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)

  # Find FFmpeg via pkg-config (system libraries on Linux).
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libavutil)
  set(FFMPEG_LIBRARIES ${LIBAV_LIBRARIES})
  set(FFMPEG_INCLUDE_DIRS ${LIBAV_INCLUDE_DIRS})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
  src/main.cpp
  src/plugin/plugin.cpp
  src/utils/obs-utils.cpp
  src/utils/duration-format.cpp
  src/utils/video-trimmer.cpp
  src/ui/ui-components.cpp
  src/managers/settings-manager.cpp
  src/managers/save-button-settings.cpp
  src/managers/replay-buffer-manager.cpp
  src/managers/hotkey-manager.cpp
)

add_library(${PROJECT_NAME} MODULE ${SOURCES})

# Common includes and link targets
target_include_directories(${PROJECT_NAME} PRIVATE
  ${Qt6Widgets_INCLUDE_DIRS}
  src
)

# Platform-specific OBS wiring
if(WIN32)
  target_include_directories(${PROJECT_NAME} PRIVATE ${obs_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${OBS_LIBRARIES})
elseif(APPLE)
  target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    Qt6::Widgets
    Qt6::Core
    ${FFMPEG_LIBRARIES}
  )
  # AGL stub framework (created by buildspec.cmake) is added to CMAKE_FRAMEWORK_PATH
  # so the linker resolves -framework AGL from Qt6 against the stub, not the SDK.
  target_link_options(${PROJECT_NAME} PRIVATE
    -F${CMAKE_BINARY_DIR}/stub-frameworks
  )
else()
  target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api)
endif()

# Link common libraries (Windows/Linux only — macOS links in the platform block above)
if(NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Core
    ${FFMPEG_LIBRARIES}
  )
endif()

# Regular installation (for development)
if(WIN32)
  # Default to the standard OBS installation directory on Windows if the user
  # hasn't overridden CMAKE_INSTALL_PREFIX.
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "C:/Program Files/obs-studio" CACHE PATH
        "Default OBS install prefix on Windows" FORCE)
  endif()
  set(PLUGIN_DEST "obs-plugins/64bit")
  set(DATA_DEST "data/obs-plugins/${PROJECT_NAME}")
elseif(APPLE)
  # macOS: OBS user plugins expect the layout:
  #   <prefix>/bin/<plugin>.so
  #   <prefix>/data/obs-plugins/<name>/
  # Set CMAKE_INSTALL_PREFIX to ~/Library/Application Support/obs-studio/plugins/<name>
  set(PLUGIN_DEST "bin")
  set(DATA_DEST "data/obs-plugins/${PROJECT_NAME}")
else()
  # Linux: standard FHS paths used by system-installed OBS.
  set(PLUGIN_DEST "lib/obs-plugins")
  set(DATA_DEST "share/obs/obs-plugins/${PROJECT_NAME}")
endif()

# Install plugin binary (all platforms)
install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${PLUGIN_DEST}
  RUNTIME DESTINATION ${PLUGIN_DEST}
)

# Install data files (all platforms)
install(DIRECTORY data/
  DESTINATION ${DATA_DEST}
)

# Release package configuration
if(WIN32)
  set(RELEASE_PLATFORM "windows-x64")
  set(RELEASE_PLUGIN_SUBDIR "obs-plugins/64bit")
elseif(APPLE)
  # macOS release package mirrors the user plugin install layout:
  #   <name>/bin/<plugin>.so
  #   <name>/data/obs-plugins/<name>/
  set(RELEASE_PLUGIN_SUBDIR "${PROJECT_NAME}/bin")
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _arch)
  set(RELEASE_PLATFORM "macos-${_arch}")
else()
  set(RELEASE_PLUGIN_SUBDIR "lib/obs-plugins")
  set(RELEASE_PLATFORM "linux-x86_64")
endif()

set(RELEASE_DIR "${CMAKE_BINARY_DIR}/release-staging")
set(RELEASE_PLUGIN_DIR "${RELEASE_DIR}/${RELEASE_PLUGIN_SUBDIR}")
# macOS data lives under <name>/data/..., other platforms at root data/...
if(APPLE)
  set(RELEASE_DATA_DIR "${RELEASE_DIR}/${PROJECT_NAME}/data/obs-plugins/${PROJECT_NAME}")
else()
  set(RELEASE_DATA_DIR "${RELEASE_DIR}/data/obs-plugins/${PROJECT_NAME}")
endif()
set(RELEASE_ZIP_DIR "${CMAKE_BINARY_DIR}/releases/${PROJECT_VERSION}")
set(RELEASE_ZIP "${RELEASE_ZIP_DIR}/${PROJECT_NAME}-${RELEASE_PLATFORM}.zip")

# Custom target that depends on the main target
add_custom_target(prepare_release
    DEPENDS ${PROJECT_NAME}
    # Create directory structure
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_DATA_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_ZIP_DIR}"

    # Copy plugin binary
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:${PROJECT_NAME}>"
        "${RELEASE_PLUGIN_DIR}/"

    # Copy data files (locale)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "${RELEASE_DATA_DIR}"

    # Create ZIP file
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${RELEASE_ZIP}" --format=zip
        "${RELEASE_DIR}/"

    COMMENT "Creating release package for ${RELEASE_PLATFORM}..."
)
