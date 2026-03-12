# CMake macOS build dependencies module
# Downloads prebuilt obs-deps (including matching Qt6) and OBS Studio source
# Based on obs-plugintemplate: https://github.com/obsproject/obs-plugintemplate

include_guard(GLOBAL)

include(buildspec_common)

function(_check_dependencies_macos)
  set(arch universal)
  set(platform macos)

  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  set(prebuilt_filename "macos-deps-VERSION-ARCH-REVISION.tar.xz")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "macos-deps-qt6-VERSION-ARCH-REVISION.tar.xz")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  set(obs-studio_filename "VERSION.tar.gz")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()

  # Remove quarantine attribute so macOS allows loading the downloaded libs
  execute_process(
    COMMAND xattr -r -d com.apple.quarantine "${dependencies_dir}"
    RESULT_VARIABLE result
    OUTPUT_QUIET
    ERROR_QUIET
  )

  list(APPEND CMAKE_FRAMEWORK_PATH "${dependencies_dir}/Frameworks")
  set(CMAKE_FRAMEWORK_PATH ${CMAKE_FRAMEWORK_PATH} PARENT_SCOPE)

  # AGL was removed from macOS 14+ SDKs but is referenced by Qt6's FindWrapOpenGL.cmake.
  # Create a stub dylib framework in the build tree so the linker is satisfied at
  # build time without shipping the stub to end users.
  set(_agl_stub_dir "${CMAKE_BINARY_DIR}/stub-frameworks/AGL.framework/Versions/A")
  if(NOT EXISTS "${_agl_stub_dir}/AGL")
    file(MAKE_DIRECTORY "${_agl_stub_dir}")
    file(WRITE "${CMAKE_BINARY_DIR}/stub-frameworks/agl_stub.c"
      "// AGL stub — framework removed in macOS 14+ SDK, satisfied at link time only.\n")
    execute_process(
      COMMAND clang
        -arch arm64 -arch x86_64
        -dynamiclib
        -mmacosx-version-min=12.0
        -install_name "@rpath/AGL.framework/Versions/A/AGL"
        -o "${_agl_stub_dir}/AGL"
        "${CMAKE_BINARY_DIR}/stub-frameworks/agl_stub.c"
      RESULT_VARIABLE _result
    )
    if(NOT _result EQUAL 0)
      message(WARNING "Could not create AGL stub framework — link may fail on macOS 14+ SDK")
    else()
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "Versions/A/AGL"
          "${CMAKE_BINARY_DIR}/stub-frameworks/AGL.framework/AGL"
      )
      message(STATUS "Created AGL stub framework for macOS 14+ SDK compatibility")
    endif()
  endif()
  list(APPEND CMAKE_FRAMEWORK_PATH "${CMAKE_BINARY_DIR}/stub-frameworks")
  set(CMAKE_FRAMEWORK_PATH ${CMAKE_FRAMEWORK_PATH} PARENT_SCOPE)
endfunction()

_check_dependencies_macos()
