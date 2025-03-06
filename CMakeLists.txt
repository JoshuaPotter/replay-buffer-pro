cmake_minimum_required(VERSION 3.16)
project(replay-buffer-pro VERSION 1.0.0)

# Find Qt
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

# Find OBS
if(WIN32)
  get_filename_component(OBS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../obs-studio" ABSOLUTE)

  if(NOT EXISTS "${OBS_ROOT_DIR}")
    message(FATAL_ERROR "OBS Studio source not found at ${OBS_ROOT_DIR}. Please clone obs-studio repository at the same level as this plugin.")
  endif()

  set(obs_INCLUDE_DIRS
    "${OBS_ROOT_DIR}/libobs"
    "${OBS_ROOT_DIR}/frontend/api"
    "${OBS_ROOT_DIR}/build/config"
  )

  set(OBS_LIBRARIES
    "${OBS_ROOT_DIR}/build/libobs/RelWithDebInfo/obs.lib"
    "${OBS_ROOT_DIR}/build/frontend/api/RelWithDebInfo/obs-frontend-api.lib"
  )
else()
  find_package(LibObs REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
  src/main.cpp
  src/plugin/plugin.cpp
  src/utils/obs-utils.cpp
  src/ui/ui-components.cpp
  src/managers/settings-manager.cpp
  src/managers/dock-state-manager.cpp
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
  Qt6::Widgets
  Qt6::Core
)

# Define paths for FFmpeg
set(FFMPEG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg")

# Regular installation (for development)
if(WIN32)
  set(PLUGIN_DEST "C:/Program Files/obs-studio/obs-plugins/64bit")
  set(DATA_DEST "C:/Program Files/obs-studio/data/obs-plugins/${PROJECT_NAME}")

  # Install FFmpeg
  install(FILES "${FFMPEG_PATH}/ffmpeg.exe"
    DESTINATION ${DATA_DEST}
  )

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
set(RELEASE_ZIP "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}-windows-x64.zip")

# Custom target that depends on the main target
add_custom_target(prepare_release
    DEPENDS ${PROJECT_NAME}
    # Create directory structure
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_DATA_DIR}"

    # Copy plugin DLL
    COMMAND ${CMAKE_COMMAND} -E copy 
        "$<TARGET_FILE:${PROJECT_NAME}>"
        "${RELEASE_PLUGIN_DIR}/"

    # Copy data files (locale)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "${RELEASE_DATA_DIR}"

    # Copy FFmpeg
    COMMAND ${CMAKE_COMMAND} -E copy
        "${FFMPEG_PATH}/ffmpeg.exe"
        "${RELEASE_DATA_DIR}/"

    # Create ZIP file
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${RELEASE_ZIP}" --format=zip
        "${RELEASE_DIR}/"

    COMMENT "Creating release package..."
)

# Create release target (no install)
add_custom_target(create_release
    COMMAND ${CMAKE_COMMAND} --build . --target prepare_release
    COMMENT "Creating release package..."
)
