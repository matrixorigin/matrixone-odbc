#
# Set one of these variables depending on which compiler is used:
#
# - MSVC
# - GCC
# - CLANG
# - SUNPRO
#
# The variable is set to the compiler version. Depending on compiler used
# additional variables can be set.
#

if(CMAKE_CXX_COMPILER_ID)
  set(compiler_id ${CMAKE_CXX_COMPILER_ID})
  set(compiler_version ${CMAKE_CXX_COMPILER_VERSION})
else()
  # Note: for C only projects

  set(compiler_id ${CMAKE_C_COMPILER_ID})
  set(compiler_version ${CMAKE_C_COMPILER_VERSION})
endif()

if(MSVC)

  # VS_VER  -- MSVC toolchain version (14,15 etc)
  # VS      -- string "vsNN" where NN is toolchain version

  set(MSVC ${compiler_version} CACHE INTERNAL "")

  if(DEFINED MSVC_TOOLSET_VERSION)
    string(REGEX REPLACE "^(..).*$" "\\1" VS ${MSVC_TOOLSET_VERSION})
  else()

    #message("-- msvc version: ${MSVC_VERSION}")
    set(VS_18 12)
    set(VS_19 14)

    string(REGEX REPLACE "^(..).*$" "\\1" VS ${MSVC_VERSION})
    set(VS ${VS_${VS}})

  endif()

  #message("-- vs: ${VS}")
  set(VS_VER ${VS} CACHE INTERNAL "")
  set(VS     "vs${VS}" CACHE INTERNAL "")

  #
  # TOOLSET and CXX_FRONTEND
  #
  set(TOOLSET "MSVC" CACHE INTERNAL "")
  set(CXX_FRONTEND "MSVC" CACHE INTERNAL "")
  set(TOOLSET_MSVC "1" CACHE INTERNAL "")
  set(CXX_FRONTEND_MSVC "1" CACHE INTERNAL "")

  #
  # If clang-cl is used, we should still set CLANG variable
  #

  if(compiler_id MATCHES "Clang")
    set(CLANG ${compiler_version} CACHE INTERNAL "")
  endif()

elseif(compiler_id MATCHES "SunPro")

  set(SUNPRO ${compiler_version} CACHE INTERNAL "")

elseif(compiler_id MATCHES "Clang")

  set(CLANG ${compiler_version} CACHE INTERNAL "")

  #
  # TOOLSET and CXX_FRONTEND
  #
  # If Clang is used on Windows we assume it is used from VS Code or Visual
  # Studio and set TOOLSET to "MSVC", otherwise TOOLSET is "GCC". Also,
  # on Windows there are two variants of Clang: the "classic" one which accepts
  # the same options as on Linux and "cl" variant which accepts MSVC compiler
  # options. This is indicated by CXX_FRONTEND variable set to "GCC" or "MSVC"
  # and variables CXX_FRONTEND_GCC, CXX_FRONTEND_MSVC.
  #

  if(WIN32)

    set(TOOLSET "MSVC" CACHE INTERNAL "")
    set(TOOLSET_MSVC "1" CACHE INTERNAL "")

    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
      set(CXX_FRONTEND "MSVC" CACHE INTERNAL "")
      set(CXX_FRONTEND_MSVC "1" CACHE INTERNAL "")
      # clang-cl behaves has MSVC
      set(MSVC ${compiler_version} CACHE INTERNAL "")
    else()
      set(CXX_FRONTEND "GCC" CACHE INTERNAL "")
      set(CXX_FRONTEND_GCC "1" CACHE INTERNAL "")
    endif()

  else()

    set(TOOLSET "GCC" CACHE INTERNAL "")
    set(CXX_FRONTEND "GCC" CACHE INTERNAL "")
    set(TOOLSET_GCC "1" CACHE INTERNAL "")
    set(CXX_FRONTEND_GCC "1" CACHE INTERNAL "")

  endif()

else()

  if(CMAKE_COMPILER_IS_GNUCXX)
    set(GCC ${compiler_version} CACHE INTERNAL "")
  endif()

endif()

#
# Check compiler flags that are not supported by all compiler versions that we # use.
#

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

if(MSVC)

  #
  # Note: We don't test for /Zc:preprocessor using check_cxx_compiler_flag()
  # because even if compiler does support the flag the standard library headers
  # might break on older Windows SDK versions. By compiling sample code we make
  # sure that all works well together.
  #

  set(CMAKE_REQUIRED_FLAGS "/Zc:preprocessor")
  check_cxx_source_compiles(
    "#include <windows.h> int main() { return 0; }"
    COMPILER_SUPPORTS_ZC_PREPROCESSOR
  )
  set(CMAKE_REQUIRED_FLAGS)

else()
  check_cxx_compiler_flag("-Wdeprecated-builtins" HAVE_DEPRECATED_BUILTINS)
endif()


#
# Macro to enable C++14 for all targets in the current directory and below.
#

macro(enable_cxx14)

  if((CMAKE_VERSION VERSION_LESS 3.1) OR (CMAKE_CXX_COMPILER_ID MATCHES "SunPro"))

    # Note: cmake does not know how to enable C++14 for SunPro compiler
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

  else()

    # use C++14
    set(CMAKE_CXX_STANDARD 14)
    # error out if not supported
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    # do not use compiler specific extensions
    set(CMAKE_CXX_EXTENSIONS OFF)

  endif()

endmacro()


macro(enable_cxx17)

  if((CMAKE_VERSION VERSION_LESS 3.8) OR (CMAKE_CXX_COMPILER_ID MATCHES "SunPro"))

    # Note: cmake does not know how to enable C++17 for SunPro compiler
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

  else()

    # use C++14
    set(CMAKE_CXX_STANDARD 17)
    # error out if not supported
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    # do not use compiler specific extensions
    set(CMAKE_CXX_EXTENSIONS OFF)

  endif()

endmacro()

