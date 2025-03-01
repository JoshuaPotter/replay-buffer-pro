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
    src/plugin.cpp
    src/obs-utils.cpp
    src/ui-components.cpp
    src/settings-manager.cpp
    src/dock-state-manager.cpp
    src/replay-buffer-manager.cpp
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

# Installation
if(WIN32)
    set(PLUGIN_DEST "C:/Program Files/obs-studio/obs-plugins/64bit")
    set(DATA_DEST "C:/Program Files/obs-studio/data/obs-plugins/${PROJECT_NAME}")
    
    # Install FFmpeg
    install(FILES "${FFMPEG_PATH}/ffmpeg.exe"
        DESTINATION ${DATA_DEST}
    )
else()
    # Linux/Mac paths
    set(PLUGIN_DEST "${CMAKE_INSTALL_PREFIX}/lib/obs-plugins")
    set(DATA_DEST "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/${PROJECT_NAME}")
endif()

# Install plugin binary
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${PLUGIN_DEST}
    RUNTIME DESTINATION ${PLUGIN_DEST}
)

# Install data files
install(DIRECTORY data/
    DESTINATION ${DATA_DEST}
)