# CMake Linux defaults module

include_guard(GLOBAL)

# Set default installation directories
include(GNUInstallDirs)

if(CMAKE_INSTALL_LIBDIR MATCHES "(CMAKE_SYSTEM_PROCESSOR)")
  string(REPLACE "CMAKE_SYSTEM_PROCESSOR" "${CMAKE_SYSTEM_PROCESSOR}" CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
endif()

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PLUGIN_EMAIL}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OBS Studio plugin for saving replay buffer clips of custom lengths")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PLUGIN_WEBSITE}")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")

# shlibdeps only adds an unversioned obs-studio, which Ubuntu's own OBS 30 also
# satisfies, so require the OBS the plugin is built against. That is the PPA's,
# which moves on its own and can be newer than the one buildspec.json targets.
find_package(libobs QUIET)
string(JSON _obs_version GET ${buildspec} dependencies obs-studio version)
if(libobs_VERSION)
  if(NOT libobs_VERSION VERSION_EQUAL _obs_version)
    message(
      WARNING
      "Building against OBS ${libobs_VERSION}, but buildspec.json targets OBS ${_obs_version}. "
      "ClipWriter mirrors that version's ffmpeg-mux; diff it against this one."
    )
  endif()
  set(_obs_version "${libobs_VERSION_MAJOR}.${libobs_VERSION_MINOR}")
endif()
set(CPACK_DEBIAN_PACKAGE_DEPENDS "obs-studio (>= ${_obs_version})")
set(CPACK_SET_DESTDIR ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25.0 OR NOT CMAKE_CROSSCOMPILING)
  set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
endif()

set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/release")

set(CPACK_SOURCE_GENERATOR "TXZ")
# The ignore rules are regular expressions, so escape the checkout path first
string(REGEX REPLACE "([][+.*()^$?|])" "\\\\\\1" _source_dir_regex "${CMAKE_CURRENT_SOURCE_DIR}")
set(
  CPACK_SOURCE_IGNORE_FILES
  ".*~$"
  \\.git/
  \\.github/
  \\.gitignore
  \\.ccache/
  "^${_source_dir_regex}/build_"
  cmake/\\.CMakeBuildNumber
  "^${_source_dir_regex}/release/"
)

set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-source")
set(CPACK_ARCHIVE_THREADS 0)

include(CPack)

if(NOT TARGET OBS::libobs)
  find_package(LibObs REQUIRED)
  add_library(OBS::libobs ALIAS libobs)

  if(ENABLE_FRONTEND_API)
    find_path(
      obs-frontend-api_INCLUDE_DIR
      NAMES obs-frontend-api.h
      PATHS /usr/include /usr/local/include
      PATH_SUFFIXES obs
    )

    find_library(obs-frontend-api_LIBRARY NAMES obs-frontend-api PATHS /usr/lib /usr/local/lib)

    if(obs-frontend-api_LIBRARY)
      if(NOT TARGET OBS::obs-frontend-api)
        if(IS_ABSOLUTE "${obs-frontend-api_LIBRARY}")
          add_library(OBS::obs-frontend-api UNKNOWN IMPORTED)
          set_property(TARGET OBS::obs-frontend-api PROPERTY IMPORTED_LOCATION "${obs-frontend-api_LIBRARY}")
        else()
          add_library(OBS::obs-frontend-api INTERFACE IMPORTED)
          set_property(TARGET OBS::obs-frontend-api PROPERTY IMPORTED_LIBNAME "${obs-frontend-api_LIBRARY}")
        endif()

        set_target_properties(
          OBS::obs-frontend-api
          PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${obs-frontend-api_INCLUDE_DIR}"
        )
      endif()
    endif()
  endif()

  macro(find_package)
    if(NOT "${ARGV0}" STREQUAL libobs AND NOT "${ARGV0}" STREQUAL obs-frontend-api)
      _find_package(${ARGV})
    endif()
  endmacro()
endif()
