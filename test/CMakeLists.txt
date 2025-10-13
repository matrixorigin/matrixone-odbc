# Copyright (c) 2007, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0, as
# published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation. The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# Without limiting anything contained in the foregoing, this file,
# which is part of Connector/ODBC, is also subject to the
# Universal FOSS Exception, version 1.0, a copy of which can be found at
# https://oss.oracle.com/licenses/universal-foss-exception.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

##########################################################################

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/test)


# put the test exe's in test/
SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/test")

IF(EXPOSE_TOFIX_TESTS)
  ADD_DEFINITIONS(-DEXPOSE_TOFIX_TESTS)
ENDIF(EXPOSE_TOFIX_TESTS)


if(GCC OR CLANG)

  # Note: We have tests for deprecated ODBC APIs such as `SQLSetStmtOption()`

  add_compile_options(-Wno-deprecated-declarations)

  # Note: Warnings in tests should be fixed but that has low priority at the moment

  add_compile_options(
    -Wno-format
    -Wno-sequence-point  # my_auth.cc
    -Wno-sign-compare
    -Wno-unused-function
    -Wno-array-bounds
    -Wno-unused-variable  # my_basics.c
  )

  if(HAVE_FORMAT_SECURITY)
    add_compile_options(-Wno-format-security)
  endif()


  if(CLANG)

    # Note: We need {x} scalar initializers for my_meta_data test
    add_compile_options(-Wno-braced-scalar-init)

  else()

    # GCC specific -- should be fixed eventually
    add_compile_options(-Wno-stringop-truncation)

  endif()

endif()


# Note: Some unit tests use std::filesystem part of the standard C++ library
# and for some versions of the standard library its implementation is in
# additional library stdc++fs.

set(EXTRA_TEST_LIBS)
if(NOT WIN32 AND NOT APPLE)

  set(check_src "${CMAKE_CURRENT_BINARY_DIR}/filesystem_chk.cc")
  file(WRITE "${check_src}"
    "#include <filesystem>\n"
    "int main() { (void)std::filesystem::current_path(); }"
  )

  try_compile(COMPILE_RESULT
    ${CMAKE_CURRENT_BINARY_DIR} "${check_src}"
    # OUTPUT_VARIABLE build_log
  )

  if(NOT COMPILE_RESULT)
    list(APPEND EXTRA_TEST_LIBS -lstdc++fs)
  endif()

endif()


ENABLE_TESTING()

IF(NOT WIN32)

  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ODBC_LINK_FLAGS}")

ELSE()

  if(COMPILER_SUPPORTS_ZC_PREPROCESSOR)
    # Note: This is required only by my_meta_data test but with time might
    # be used by other tests
    add_compile_options(/Zc:preprocessor /bigobj)
  endif()

ENDIF(NOT WIN32)


set(TESTS
  my_auth my_data my_catalog3 my_options my_param2 my_result3
)

if(COMPILER_SUPPORTS_ZC_PREPROCESSOR)
  list(APPEND TESTS my_meta_data)
endif()

set(TESTS_C
  my_basics my_blob my_bulk my_catalog1 my_catalog2
  my_crash my_curext my_cursor my_datetime my_desc
  my_dyn_cursor my_error my_info my_keys my_param
  my_prepare my_relative my_result1 my_result2 my_scroll
  my_setup my_tran my_types my_unicode my_unixodbc
  my_use_result my_bug13766 my_pooling
)


FOREACH(T IN LISTS TESTS TESTS_C)

  set(FN "${T}.cc")
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${FN}")
    set(FN "${T}.c")
  endif()

  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${FN}")
    message(SEND_ERROR "Could not find sources for test: ${T}")
    continue()
  endif()

  IF(WIN32)
    ADD_EXECUTABLE(${T} ${FN} odbctap.h)
  ELSE(WIN32)
    ADD_EXECUTABLE(${T} ${FN})
  ENDIF(WIN32)

  add_version_info(${T}
    "MySQL Connector/ODBC unit test."
    "Tests the driver functionality."
  )

  add_dependencies(${T} generate_test_config_files)

  set_target_properties(${T} PROPERTIES FOLDER Tests)

  IF(MYSQL_CXX_LINKAGE)
    SET_TARGET_PROPERTIES(${T} PROPERTIES
          LINKER_LANGUAGE CXX
          LINK_FLAGS "${MYSQLODBCCONN_LINK_FLAGS_ENV} ${MYSQL_LINK_FLAGS}"
          COMPILE_FLAGS "${MYSQLODBCCONN_COMPILE_FLAGS_ENV} ${MYSQL_CXXFLAGS}")
  ENDIF(MYSQL_CXX_LINKAGE)

  install_test(${T})

  SET_TARGET_PROPERTIES(${T} PROPERTIES
      LINK_FLAGS "${MYSQLODBCCONN_LINK_FLAGS_ENV} ${MYSQL_LINK_FLAGS}")

  IF(WIN32)
    IF (WITH_NODEFAULTLIB)
      SET_TARGET_PROPERTIES(${T} PROPERTIES
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
        LINK_FLAGS_RELWITHDEBINFO "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
        LINK_FLAGS_RELEASE "/NODEFAULTLIB:${WITH_NODEFAULTLIB}")
    ENDIF ()
  ENDIF(WIN32)

  target_link_libraries(${T} ${EXTRA_TEST_LIBS})

  IF(WIN32)
    IF(MYSQL8)
      TARGET_LINK_LIBRARIES(${T} ${ODBCLIB} ${ODBCINSTLIB} legacy_stdio_definitions.lib myodbc-util)
    ELSE(MYSQL8)
      TARGET_LINK_LIBRARIES(${T} ${ODBCLIB} ${ODBCINSTLIB} myodbc-util)
    ENDIF(MYSQL8)
  ELSE(WIN32)
    TARGET_LINK_LIBRARIES(${T} ${ODBC_LINK_FLAGS} ${ODBCINSTLIB} myodbc-util)
  ENDIF(WIN32)
  ADD_TEST(${T} ${T})
  set_property(TEST ${T} PROPERTY SKIP_RETURN_CODE 77)

  if(APPLE)
    set_property(TARGET ${T} PROPERTY BUILD_WITH_INSTALL_RPATH ON)
    set_property(TARGET ${T} APPEND PROPERTY INSTALL_RPATH "@loader_path")
  elseif(NOT WIN32)
    set_property(TARGET ${T} PROPERTY BUILD_WITH_INSTALL_RPATH ON)
    set_property(TARGET ${T} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
  endif()

ENDFOREACH()

# Adding testsuites for testing driver without DM. May be useful to test behaviors that normally are initiated by DM only
# Separate testsuites for ansi and unicode driver are created - we can't use same test for both since there is no DM to convert
# data and to map calls. Excluding OS X so far, since direct linking there is problematic

set(skip_no_dm "1")

IF(NOT skip_no_dm)

IF(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

  FOREACH(CONNECTOR_DRIVER_TYPE_SHORT ${CONNECTOR_DRIVER_TYPES_SHORT})

    # Skipping unicode driver so far
    IF (NOT ${CONNECTOR_DRIVER_TYPE_SHORT} STREQUAL "w")
      SET(T "my_nodm${CONNECTOR_DRIVER_TYPE_SHORT}")

      IF(WIN32)
        ADD_EXECUTABLE(${T} ${T}.c odbctap.h)
      ELSE(WIN32)
        ADD_EXECUTABLE(${T} ${T}.c)
      ENDIF(WIN32)

      add_version_info(${T}
        "MySQL Connector/ODBC unit test."
        "Tests the driver functionality."
      )

      IF(MYSQL_CXX_LINKAGE)
        SET_TARGET_PROPERTIES(${T} PROPERTIES
              LINKER_LANGUAGE CXX
              LINK_FLAGS "${MYSQLODBCCONN_LINK_FLAGS_ENV} ${MYSQL_LINK_FLAGS}"
              COMPILE_FLAGS "${MYSQLODBCCONN_COMPILE_FLAGS_ENV} ${MYSQL_CXXFLAGS}")
      ENDIF(MYSQL_CXX_LINKAGE)

      install_test({T})
      SET_TARGET_PROPERTIES(${T} PROPERTIES
          LINK_FLAGS "${MYSQLODBCCONN_LINK_FLAGS_ENV} ${MYSQL_LINK_FLAGS}")

      IF(WIN32)
        IF (MYSQLCLIENT_STATIC_LINKING)
          SET_TARGET_PROPERTIES(${T} PROPERTIES
            LINK_FLAGS_DEBUG "/NODEFAULTLIB:LIBCMTD /NODEFAULTLIB:LIBCMT"
            LINK_FLAGS_RELWITHDEBINFO "/NODEFAULTLIB:LIBCMT"
            LINK_FLAGS_RELEASE "/NODEFAULTLIB:LIBCMT")
        ENDIF (MYSQLCLIENT_STATIC_LINKING)
      ENDIF(WIN32)

      # Not quite clear atm whether ${ODBCINSTLIB} is needed
      TARGET_LINK_LIBRARIES(${T} myodbc-${CONNECTOR_DRIVER_TYPE_SHORT})

      ADD_TEST(${T} ${T})
    ENDIF (NOT ${CONNECTOR_DRIVER_TYPE_SHORT} STREQUAL "w")

  ENDFOREACH(CONNECTOR_DRIVER_TYPE_SHORT)

ENDIF(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

ENDIF(NOT skip_no_dm)

TARGET_LINK_LIBRARIES(my_basics ${CMAKE_THREAD_LIBS_INIT})


#
# Custom command and target generate_test_config_files to generate
# ODBC configuration files which define drivers and data sources used
# by our tests. These files are generated by
# cmake/generateinifiles.cmake script run at build time (only then
# the locations of the drivers are known). Here we prepare input variables
# for the script.
#

set(drivers_options)

foreach(idx RANGE 0 1)

  list(GET CONNECTOR_DRIVER_TYPE_LIST ${idx} driver_type)
  list(GET CONNECTOR_DRIVER_TYPES_SHORT ${idx} driver_type_short)
  set(driver_tgt "myodbc-${driver_type_short}")
  if(TARGET ${driver_tgt})

    math(EXPR DRIVERS_COUNT "${idx}+1")

    list(APPEND drivers_options
      -DDRIVER_LOCATION${DRIVERS_COUNT}=$<TARGET_FILE:${driver_tgt}>
      -DCONNECTOR_DRIVER_TYPE${DRIVERS_COUNT}=${driver_type}
      -DCONNECTOR_DRIVER_TYPE_SHORT${DRIVERS_COUNT}=${driver_type_short}
    )

  endif()

endforeach()

add_custom_command(
  OUTPUT "odbcinst.ini" "odbc.ini"
  COMMAND ${CMAKE_COMMAND}
    -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DCONNECTOR_MAJOR=${CONNECTOR_MAJOR}
    -DCONNECTOR_MINOR=${CONNECTOR_MINOR}
    -DDRIVERS_COUNT=${DRIVERS_COUNT}
    ${drivers_options}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/generateinifiles.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(generate_test_config_files
  DEPENDS "odbcinst.ini" "odbc.ini"
)

set_target_properties(generate_test_config_files PROPERTIES FOLDER Misc)


install_test_files(
	${CMAKE_CURRENT_BINARY_DIR}/CTestTestfile.cmake
	${CMAKE_CURRENT_BINARY_DIR}/odbc.ini
	${CMAKE_CURRENT_BINARY_DIR}/odbcinst.ini
)

# Install tokens for OpenID authentication
foreach (token
  openid_token_correct.txt
  openid_token_empty.txt
  openid_token_expired.txt
  openid_token_invalid.txt
  openid_token_issuer2.txt
  openid_token_key2.txt
  openid_token_user2.txt
)
  # Copy tokens to build dir
  file(COPY tokens/${token} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

  # Install tokens
  install_test_files(tokens/${token})

endforeach()
