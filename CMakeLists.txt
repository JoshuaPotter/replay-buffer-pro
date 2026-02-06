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
else()
  find_package(LibObs REQUIRED)
  # For Linux/Mac, use system FFmpeg or find FFmpeg from OBS installation
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libavutil)
  set(FFMPEG_LIBRARIES ${LIBAV_LIBRARIES})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
  src/main.cpp
  src/plugin/plugin.cpp
  src/utils/obs-utils.cpp
  src/utils/video-trimmer.cpp
  src/ui/ui-components.cpp
  src/managers/settings-manager.cpp
  src/managers/replay-buffer-manager.cpp
  src/managers/hotkey-manager.cpp
)

add_library(${PROJECT_NAME} MODULE ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
  ${obs_INCLUDE_DIRS}
  ${Qt6Widgets_INCLUDE_DIRS}
  src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  ${OBS_LIBRARIES}
  ${FFMPEG_LIBRARIES}
  Qt6::Widgets
  Qt6::Core
)

# Regular installation (for development)
if(WIN32)
  set(PLUGIN_DEST "C:/Program Files/obs-studio/obs-plugins/64bit")
  set(DATA_DEST "C:/Program Files/obs-studio/data/obs-plugins/${PROJECT_NAME}")

  # Install plugin binary
  install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${PLUGIN_DEST}
    RUNTIME DESTINATION ${PLUGIN_DEST}
  )

  # Install data files
  install(DIRECTORY data/
    DESTINATION ${DATA_DEST}
  )
else()
  # Linux/Mac paths
  set(PLUGIN_DEST "${CMAKE_INSTALL_PREFIX}/lib/obs-plugins")
  set(DATA_DEST "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/${PROJECT_NAME}")
endif()

# Release package configuration
set(RELEASE_DIR "${CMAKE_BINARY_DIR}/obs-studio")
set(RELEASE_PLUGIN_DIR "${RELEASE_DIR}/obs-plugins/64bit")
set(RELEASE_DATA_DIR "${RELEASE_DIR}/data/obs-plugins/${PROJECT_NAME}")
set(RELEASE_ZIP_DIR "${CMAKE_BINARY_DIR}/releases/${PROJECT_VERSION}")
set(RELEASE_ZIP "${RELEASE_ZIP_DIR}/${PROJECT_NAME}-windows-x64.zip")

# Custom target that depends on the main target
add_custom_target(prepare_release
    DEPENDS ${PROJECT_NAME}
    # Create directory structure
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_DATA_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_ZIP_DIR}"

    # Copy plugin DLL
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

    COMMENT "Creating release package..."
)

# Create release target (no install)
add_custom_target(create_release
    DEPENDS prepare_release
    COMMENT "Creating release package..."
)
