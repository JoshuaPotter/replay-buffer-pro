cmake_minimum_required(VERSION 3.16)
project(replay-buffer-pro VERSION 1.3.0)

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
  # macOS: Robust build support for Apple Silicon and Intel
  
  # Detect Homebrew prefix automatically (Apple Silicon vs Intel)
  # Check environment variable first, then common locations
  if(DEFINED ENV{HOMEBREW_PREFIX})
    set(HOMEBREW_PREFIX "$ENV{HOMEBREW_PREFIX}")
  elseif(EXISTS "/opt/homebrew")
    set(HOMEBREW_PREFIX "/opt/homebrew")
  elseif(EXISTS "/usr/local")
    set(HOMEBREW_PREFIX "/usr/local")
  else()
    message(FATAL_ERROR "Could not find Homebrew installation. "
      "Please install Homebrew from https://brew.sh/\n"
      "Or set HOMEBREW_PREFIX environment variable:
"
      "export HOMEBREW_PREFIX=/path/to/homebrew")
  endif()
  message(STATUS "Using Homebrew prefix: ${HOMEBREW_PREFIX}")
  
  # Allow user to override OBS source directory
  set(OBS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../obs-studio" CACHE PATH "Path to OBS Studio source directory")
  get_filename_component(OBS_ROOT_DIR "${OBS_SOURCE_DIR}" ABSOLUTE)
  
  if(NOT EXISTS "${OBS_ROOT_DIR}")
    message(FATAL_ERROR 
      "OBS Studio source not found at ${OBS_ROOT_DIR}.\n\n"
      "Please clone obs-studio repository as a sibling of this plugin:\n"
      "  git clone --recursive https://github.com/obsproject/obs-studio.git ../obs-studio\n\n"
      "Or set OBS_SOURCE_DIR when configuring:\n"
      "  cmake -B build -DOBS_SOURCE_DIR=/path/to/obs-studio ...")
  endif()
  message(STATUS "OBS Studio source: ${OBS_ROOT_DIR}")

  # Find OBS build directory with support for multiple configs
  # OBS typically uses Xcode generator on macOS, creating config subdirectories
  set(OBS_BUILD_DIR "")
  set(OBS_LIBOBS_PATH "")
  set(OBS_FRONTEND_PATH "")
  
  # Function to check OBS build paths with a given config name
  function(CHECK_OBS_BUILD_CONFIG CONFIG_NAME)
    if(OBS_BUILD_DIR)
      return() # Already found
    endif()
    
    # Xcode-style framework build (Debug/Release/RelWithDebInfo)
    if(EXISTS "${OBS_ROOT_DIR}/build_macos/libobs/${CONFIG_NAME}/libobs.framework/libobs")
      set(OBS_BUILD_DIR "${OBS_ROOT_DIR}/build_macos" PARENT_SCOPE)
      set(OBS_LIBOBS_PATH "${OBS_ROOT_DIR}/build_macos/libobs/${CONFIG_NAME}/libobs.framework/libobs" PARENT_SCOPE)
      set(OBS_FRONTEND_PATH "${OBS_ROOT_DIR}/build_macos/frontend/api/${CONFIG_NAME}/obs-frontend-api.dylib" PARENT_SCOPE)
    # Unix Makefiles style
    elseif(EXISTS "${OBS_ROOT_DIR}/build_macos/libobs/libobs.dylib")
      set(OBS_BUILD_DIR "${OBS_ROOT_DIR}/build_macos" PARENT_SCOPE)
      set(OBS_LIBOBS_PATH "${OBS_ROOT_DIR}/build_macos/libobs/libobs.dylib" PARENT_SCOPE)
      set(OBS_FRONTEND_PATH "${OBS_ROOT_DIR}/build_macos/frontend-api/libobs-frontend-api.dylib" PARENT_SCOPE)
    elseif(EXISTS "${OBS_ROOT_DIR}/build/libobs/libobs.dylib")
      set(OBS_BUILD_DIR "${OBS_ROOT_DIR}/build" PARENT_SCOPE)
      set(OBS_LIBOBS_PATH "${OBS_ROOT_DIR}/build/libobs/libobs.dylib" PARENT_SCOPE)
      set(OBS_FRONTEND_PATH "${OBS_ROOT_DIR}/build/frontend-api/libobs-frontend-api.dylib" PARENT_SCOPE)
    endif()
  endfunction()
  
  # Check for OBS build in order of preference
  CHECK_OBS_BUILD_CONFIG("RelWithDebInfo")  # OBS default for release builds
  CHECK_OBS_BUILD_CONFIG("Release")
  CHECK_OBS_BUILD_CONFIG("Debug")
  
  if(NOT OBS_BUILD_DIR)
    message(FATAL_ERROR 
      "OBS Studio build not found. Please build OBS Studio first.\n\n"
      "Build commands:\n"
      "  cd ${OBS_ROOT_DIR}\n"
      "  cmake -B build_macos -S . -G Xcode \\n"
      "    -DCMAKE_PREFIX_PATH=\"${HOMEBREW_PREFIX}\" \\n"
      "    -DCMAKE_OSX_ARCHITECTURES=\"${CMAKE_OSX_ARCHITECTURES}\"\n"
      "  cmake --build build_macos\n\n"
      "IMPORTANT: macOS requires the full Xcode app (not just Command Line Tools):\n"
      "  1. Install Xcode from the App Store or Apple Developer\n"
      "  2. Run: sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer\n"
      "  3. Accept license: sudo xcodebuild -license accept\n\n"
      "Dependencies (install via Homebrew):\n"
      "  brew install qt6 ffmpeg simde pkg-config cmake")
  endif()
  message(STATUS "OBS Studio build: ${OBS_BUILD_DIR}")

  # Verify obsconfig.h exists
  if(NOT EXISTS "${OBS_BUILD_DIR}/config/obsconfig.h")
    message(FATAL_ERROR 
      "obsconfig.h not found in ${OBS_BUILD_DIR}/config/\n"
      "Please ensure OBS Studio build completed successfully.")
  endif()

  # Verify OBS libraries exist
  if(NOT EXISTS "${OBS_LIBOBS_PATH}")
    message(FATAL_ERROR 
      "libobs not found at: ${OBS_LIBOBS_PATH}\n"
      "Please rebuild OBS Studio with the selected configuration.")
  endif()
  
  if(NOT EXISTS "${OBS_FRONTEND_PATH}")
    message(FATAL_ERROR 
      "obs-frontend-api not found at: ${OBS_FRONTEND_PATH}\n"
      "Please ensure OBS Studio was built with the frontend API enabled.")
  endif()

  set(obs_INCLUDE_DIRS
    "${OBS_ROOT_DIR}/libobs"
    "${OBS_ROOT_DIR}/frontend/api"
    "${OBS_BUILD_DIR}/config"
    "${HOMEBREW_PREFIX}/include"  # simde and other Homebrew headers
  )

  # OBS libraries - link against OBS build output
  set(OBS_LIBRARIES
    "${OBS_LIBOBS_PATH}"
    "${OBS_FRONTEND_PATH}"
  )
  
  message(STATUS "Using libobs: ${OBS_LIBOBS_PATH}")
  message(STATUS "Using obs-frontend-api: ${OBS_FRONTEND_PATH}")

  # Find FFmpeg via pkg-config (Homebrew on macOS)
  find_package(PkgConfig REQUIRED)
  
  # Ensure pkg-config can find Homebrew packages
  set(ENV{PKG_CONFIG_PATH} "${HOMEBREW_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
  
  pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libavutil)
  
  if(NOT LIBAV_FOUND)
    message(FATAL_ERROR 
      "FFmpeg libraries not found via pkg-config.\n\n"
      "Please install FFmpeg via Homebrew:\n"
      "  brew install ffmpeg\n\n"
      "And ensure pkg-config path includes Homebrew:\n"
      "  export PKG_CONFIG_PATH=\"${HOMEBREW_PREFIX}/lib/pkgconfig:\$PKG_CONFIG_PATH\"")
  endif()
  
  message(STATUS "FFmpeg libraries found: ${LIBAV_LIBRARIES}")
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
  target_include_directories(${PROJECT_NAME} PRIVATE ${obs_INCLUDE_DIRS} ${LIBAV_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Widgets
    Qt6::Core
    ${OBS_LIBRARIES}
  )
  # Link FFmpeg using pkg-config linker flags
  target_link_options(${PROJECT_NAME} PRIVATE ${LIBAV_LDFLAGS})
else()
  target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api)
endif()

# Link common libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  Qt6::Widgets
  Qt6::Core
  ${FFMPEG_LIBRARIES}
)

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
