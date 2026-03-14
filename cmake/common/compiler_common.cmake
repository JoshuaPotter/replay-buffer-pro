# CMake common compiler options module

include_guard(GLOBAL)

# Set C and C++ language standards to C17 and C++17
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Set symbols to be hidden by default for C and C++
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

# clang options for C
set(
  _obs_clang_c_options
  -fno-strict-aliasing
  -Wno-trigraphs
  -Wno-missing-field-initializers
  -Wno-missing-prototypes
  -Werror=return-type
  -Wunreachable-code
  -Wno-missing-braces
  -Wparentheses
  -Wswitch
  -Wno-unused-function
  -Wno-unused-label
  -Wunused-parameter
  -Wunused-variable
  -Wunused-value
  -Wempty-body
  -Wuninitialized
  -Wno-unknown-pragmas
  -Wconstant-conversion
  -Wno-conversion
  -Wint-conversion
  -Wbool-conversion
  -Wenum-conversion
  -Wsign-compare
  -Wshorten-64-to-32
  -Wpointer-sign
  -Wformat-security
  -Wno-shadow
  -Wno-float-conversion
)

# clang options for C++
set(
  _obs_clang_cxx_options
  ${_obs_clang_c_options}
  -Wno-non-virtual-dtor
  -Wno-overloaded-virtual
  -Wno-exit-time-destructors
  -Winvalid-offsetof
  -Wmove
  -Wrange-loop-analysis
  -Wno-shadow
)

if(CMAKE_CXX_STANDARD GREATER_EQUAL 20)
  list(APPEND _obs_clang_cxx_options -fno-char8_t)
endif()

if(NOT DEFINED CMAKE_COMPILE_WARNING_AS_ERROR)
  set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
endif()
