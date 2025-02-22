cmake_minimum_required(VERSION 3.16)
project(replay-buffer-pro VERSION 1.0.0)

# Find Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

# Find OBS
if(WIN32)
    # Set OBS paths relative to this project
    get_filename_component(OBS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../obs-studio" ABSOLUTE)
    
    if(NOT EXISTS "${OBS_ROOT_DIR}")
        message(FATAL_ERROR "OBS Studio source not found at ${OBS_ROOT_DIR}. Please clone obs-studio repository at the same level as this plugin.")
    endif()

    message(STATUS "OBS Include Dir: ${OBS_ROOT_DIR}/libobs")
    message(STATUS "OBS Root Dir: ${OBS_ROOT_DIR}")

    set(obs_INCLUDE_DIRS
        "${OBS_ROOT_DIR}"
        "${OBS_ROOT_DIR}/libobs"
        "${OBS_ROOT_DIR}/frontend/api"
        "${OBS_ROOT_DIR}/deps/jansson/include"
        "${OBS_ROOT_DIR}/build/config"
    )

    # Set library paths to the build directory
    set(obs_LIBRARY_DIRS
        "${OBS_ROOT_DIR}/build/libobs/RelWithDebInfo"
        "${OBS_ROOT_DIR}/build/frontend/api/RelWithDebInfo"
    )

    # Set the actual library files
    set(OBS_LIBRARIES
        "${OBS_ROOT_DIR}/build/libobs/RelWithDebInfo/obs.lib"
        "${OBS_ROOT_DIR}/build/frontend/api/RelWithDebInfo/obs-frontend-api.lib"
    )

    # Add debug output
    foreach(dir ${obs_INCLUDE_DIRS})
        message(STATUS "Checking include directory: ${dir}")
        if(EXISTS "${dir}")
            message(STATUS "  Directory exists")
            file(GLOB headers "${dir}/*.h")
            foreach(header ${headers})
                message(STATUS "  Found header: ${header}")
            endforeach()
        else()
            message(STATUS "  Directory does not exist")
        endif()
    endforeach()

    # Check library paths
    foreach(lib ${OBS_LIBRARIES})
        message(STATUS "Checking library: ${lib}")
        if(EXISTS "${lib}")
            message(STATUS "  Library exists")
        else()
            message(STATUS "  Library does not exist")
        endif()
    endforeach()
else()
    find_package(LibObs REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
    src/plugin-main.cpp
    src/replay_buffer_pro.cpp
)

add_library(${PROJECT_NAME} MODULE ${SOURCES})

message(STATUS "OBS Include Dirs: ${obs_INCLUDE_DIRS}")
message(STATUS "OBS Library Dirs: ${obs_LIBRARY_DIRS}")
message(STATUS "OBS Libraries: ${OBS_LIBRARIES}")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${obs_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    src
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${obs_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OBS_LIBRARIES}
    Qt6::Widgets
    Qt6::Core
)

# Installation
if(WIN32)
    set(PLUGIN_DEST "C:/Program Files/obs-studio/obs-plugins/64bit")
    set(DATA_DEST "C:/Program Files/obs-studio/data/obs-plugins/${PROJECT_NAME}")
else()
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