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
# The plugin is built for each Ubuntu release whose PPA provides OBS, and those
# releases build against different OBS and FFmpeg majors, so name the .deb and .ddeb
# with the release: 24.04 and 26.04 packages must not be installed on the other. The
# package name stays the same, so `apt remove replay-buffer-pro` keeps working.
# Only Ubuntu itself gets the release in the name; derivatives such as Mint and Pop!_OS
# number their releases differently, and other distributions name no Ubuntu release.
set(_os_id "")
set(_os_version "")
if(EXISTS "/etc/os-release")
  file(STRINGS "/etc/os-release" _os_release REGEX "^(ID|VERSION_ID)=")
  foreach(_os_line IN LISTS _os_release)
    if(_os_line MATCHES "^ID=\"?([^\"]*)\"?$")
      set(_os_id "${CMAKE_MATCH_1}")
    elseif(_os_line MATCHES "^VERSION_ID=\"?([0-9]+\\.[0-9]+)\"?$")
      set(_os_version "${CMAKE_MATCH_1}")
    endif()
  endforeach()
endif()
set(_deb_arch "${CMAKE_C_LIBRARY_ARCHITECTURE}")
if(NOT _deb_arch)
  set(_deb_arch "${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
endif()
if(_os_id STREQUAL "ubuntu" AND _os_version)
  string(REPLACE "-linux-" "-ubuntu${_os_version}-linux-" _deb_arch "${_deb_arch}")
endif()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${_deb_arch}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PLUGIN_EMAIL}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OBS Studio plugin for saving replay buffer clips of custom lengths")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PLUGIN_WEBSITE}")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")

# shlibdeps only adds an unversioned obs-studio, which Ubuntu's own OBS also
# satisfies, so require the OBS the plugin is built against. That is the PPA's,
# which moves on its own and can differ from the one buildspec.json targets.
find_package(libobs QUIET)
string(JSON _obs_version GET ${buildspec} dependencies obs-studio version)
if(libobs_VERSION)
  set(_libobs_major_minor "${libobs_VERSION_MAJOR}.${libobs_VERSION_MINOR}")
  # Linux builds against the OBS in the distro's PPA, which may differ from the
  # buildspec.json target Windows and macOS use. The supported Linux majors are
  # listed in dependencies.obs-studio.linuxSupported.
  string(JSON _linux_supported_count ERROR_VARIABLE _linux_supported_err LENGTH ${buildspec} dependencies obs-studio linuxSupported)
  if(NOT _linux_supported_err)
    set(_linux_supported FALSE)
    math(EXPR _linux_supported_last "${_linux_supported_count} - 1")
    foreach(_i RANGE 0 ${_linux_supported_last})
      string(JSON _linux_supported_version GET ${buildspec} dependencies obs-studio linuxSupported ${_i})
      if(_libobs_major_minor VERSION_EQUAL _linux_supported_version)
        set(_linux_supported TRUE)
      endif()
    endforeach()
    if(NOT _linux_supported)
      message(
        WARNING
        "Building against OBS ${libobs_VERSION}, which is not in buildspec.json's dependencies.obs-studio.linuxSupported. "
        "ClipWriter mirrors that version's ffmpeg-mux; diff it against this one."
      )
    endif()
  else()
    # Patch releases don't change what the plugin builds against, so compare major.minor
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" _obs_target "${_obs_version}")
    if(NOT _libobs_major_minor VERSION_EQUAL _obs_target)
      message(
        WARNING
        "Building against OBS ${libobs_VERSION}, but buildspec.json targets OBS ${_obs_version}. "
        "ClipWriter mirrors that version's ffmpeg-mux; diff it against this one."
      )
    endif()
  endif()
  set(_obs_version "${_libobs_major_minor}")
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
