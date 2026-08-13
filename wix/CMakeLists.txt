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

cmake_minimum_required(VERSION 3.15)

PROJECT(MYODBC_MSI_INSTALLER)

INCLUDE(${CMAKE_SOURCE_DIR}/../version.cmake)

#-------------- wix installer ------------------------
IF(WIN32)
  INCLUDE(${CMAKE_SOURCE_DIR}/cmake/FindWix.cmake)

  # ----------- generate version information ---------
  # Keep generated files in the binary tree.  The former TRY_RUN helper wrote
  # into whichever working directory happened to be active, so an in-source
  # artifact could hide a broken clean package build.
  SET(ODBC_VERSION "${CONNECTOR_NUMERIC_VERSION}")
  SET(ODBC_VERSION_SUFFIX "")
  MATH(EXPR ODBC_VERSION_NUMBER
       "${CONNECTOR_MAJOR} * 10000 + ${CONNECTOR_MINOR} * 100 + ${CONNECTOR_PATCH}")
  FILE(WRITE "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs"
       "<Include xmlns=\"http://wixtoolset.org/schemas/v4/wxs\">\n"
       "<?define odbc_ver_major=\"${CONNECTOR_MAJOR}\" ?>\n"
       "<?define odbc_ver_short=\"${CONNECTOR_BASE_VERSION}\" ?>\n"
       "<?define odbc_ver_long=\"${CONNECTOR_NUMERIC_VERSION}\" ?>\n"
       "<?define odbc_ver_prev=\"${CONNECTOR_MAJOR_PREV}.${CONNECTOR_MINOR_PREV}.${CONNECTOR_PATCH_PREV}\" ?>\n"
       "<?define odbc_ver_num=\"${ODBC_VERSION_NUMBER}\" ?>\n"
       "<?define odbc_driver_type=\"Unicode and ANSI\" ?>\n"
       "<?define odbc_driver_type_suffix=\"\" ?>\n")
  # -------------- find binary resources -------------
  FIND_PATH(WIX_RESOURCE MySQLConnector.ico
          ./resources
          $ENV{WIX_RESOURCE}
          ${CMAKE_SOURCE_DIR}/../../../wix-installer/resources)
  IF(NOT WIX_RESOURCE)
          MESSAGE(ERROR "Can't find binary MySQL resource files. Please specify WIX_RESOURCE")
  ENDIF(NOT WIX_RESOURCE)
  MESSAGE(STATUS "Wix resources found in ${WIX_RESOURCE}")
  # --------------------------------------------------

  # -------------- add wix variables --- -------------
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs" "<?define odbc_resource=\"${WIX_RESOURCE}\" ?>")
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs" "<?define odbc_msi_ver=\"${MATRIXONE_MSI_VERSION}\" ?>")
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs" "<?define matrixone_package_version=\"${MATRIXONE_PACKAGE_VERSION}\" ?>")
  IF(${MSI_64})
     FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs" "<?define odbc_64bit=1 ?>")
     SET(MSI_VERSION "winx64")
  ELSE(${MSI_64})
     FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs" "<?define odbc_64bit=0 ?>")
     SET(MSI_VERSION "win32")
  ENDIF(${MSI_64})
  IF(${LICENSE})
     FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs" "<?define license=\"commercial\" ?><?define odbc_license_suffix=\" - Commercial\" ?>")
  ELSE(${LICENSE})
     FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs" "<?define license=\"community\" ?><?define odbc_license_suffix=\"\" ?>")
  ENDIF(${LICENSE})
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/myodbc_version.wxs" "</Include>")
  # --------------------------------------------------

  IF(ODBC_VERSION_SUFFIX STREQUAL "a")
	  SET(PACKAGE_SUFFIX "a")
  ENDIF(ODBC_VERSION_SUFFIX STREQUAL "a")

  IF(${LICENSE})
	  SET(MSI_PACKAGE "matrixone-odbc-commercial-${MATRIXONE_PACKAGE_VERSION}-${MSI_VERSION}${PACKAGE_SUFFIX}.msi")
  ELSE(${LICENSE})
	  SET(MSI_PACKAGE "matrixone-odbc-${MATRIXONE_PACKAGE_VERSION}-${MSI_VERSION}${PACKAGE_SUFFIX}.msi")
  ENDIF(${LICENSE})

  # A package revision must have a new ProductCode so Windows Installer treats
  # it as a major upgrade.  Derive the GUID deterministically so rebuilding the
  # same revision still produces a repairable package with the same identity.
  STRING(UUID CONNECTOR_PKG_ID1
         NAMESPACE "AF8C2D5E-2B35-4F37-A83A-8B447E0FC7D8"
         NAME "matrixone-odbc-${MATRIXONE_PACKAGE_VERSION}-${MSI_VERSION}-${LICENSE}-${ODBC_VERSION_SUFFIX}"
         TYPE SHA1 UPPER)

ENDIF(WIN32)
#-----------------------------------------------------

SET(WIX_DOCUMENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc" CACHE PATH
    "Directory containing README, license, change log, and build metadata")
SET(LICENSE_FILE "${WIX_DOCUMENT_DIR}/LICENSE.txt")
SET(LICENSE_RTF  "${CMAKE_CURRENT_BINARY_DIR}/resources/LICENSE.rtf")

FILE(READ ${LICENSE_FILE} CONTENTS)
STRING(REGEX REPLACE "\n" "\\\\par\n" CONTENTS "${CONTENTS}")
STRING(REGEX REPLACE "\t" "\\\\tab" CONTENTS "${CONTENTS}")
FILE(WRITE "${LICENSE_RTF}"
     "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset0 Courier New;}}\\viewkind4\\uc1\\pard\\lang1031\\f0\\fs15")
FILE(APPEND "${LICENSE_RTF}" "${CONTENTS}")
FILE(APPEND "${LICENSE_RTF}" "\n}\n")

# Generating XML list of plugins

SET(XML_COMPONENT_BITNESS "always32")
SET(XML_FILE_SOURCE_ARCH x86)
IF(${MSI_64})
  SET(XML_COMPONENT_BITNESS "always64")
  SET(XML_FILE_SOURCE_ARCH x64)
ENDIF()

SET(XML_PLUGINS "${CMAKE_CURRENT_BINARY_DIR}/mysql_odbc_plugins.wxs")
SET(XML_PLUGINS_GUID "DA0F35CE-9A33-4EDD-9B83-47E679CABCDE")
IF(${MSI_64})
  SET(XML_PLUGINS_GUID "F5FC6873-2D8A-45D6-8B3B-EC2DB6782A79")
ENDIF()

FILE(WRITE "${XML_PLUGINS}" "<Include xmlns='http://wixtoolset.org/schemas/v4/wxs'><Component Id='plugins' Guid='${XML_PLUGINS_GUID}' SharedDllRefCount='yes' Bitness='${XML_COMPONENT_BITNESS}'>")
FILE(GLOB PLUGIN_DLL_LIST "${CMAKE_CURRENT_BINARY_DIR}/${XML_FILE_SOURCE_ARCH}/plugin/*.dll")
MESSAGE("LOOKING INSIDE ${CMAKE_CURRENT_BINARY_DIR}/${XML_FILE_SOURCE_ARCH}/plugin/*.dll")
MESSAGE("PLUGIN_DLL_LIST=${PLUGIN_DLL_LIST}")
FOREACH(_PLUGIN_DLL ${PLUGIN_DLL_LIST})
  get_filename_component(_PLUGIN_DLL_FILE ${_PLUGIN_DLL} NAME)
  FILE(APPEND "${XML_PLUGINS}" "<File Id='${_PLUGIN_DLL_FILE}' Name='${_PLUGIN_DLL_FILE}' DiskId='1' Source='${XML_FILE_SOURCE_ARCH}\\plugin\\${_PLUGIN_DLL_FILE}' />")
ENDFOREACH()

FILE(APPEND "${XML_PLUGINS}" "</Component></Include>")

# Generating XML list of dependencies

SET(XML_DEPENDENCIES "${CMAKE_CURRENT_BINARY_DIR}/mysql_odbc_dependencies.wxs")
SET(XML_DEPS_GUID "8D0139C3-219E-4818-B1CD-4F8F43BEF28A")
IF(${MSI_64})
  SET(XML_DEPS_GUID "E49B9F03-4289-4EC1-AAF8-5B5DC2CE8AB2")
ENDIF()

SET(EXCLUDED_FILES "${MYODBC_ANSI_NAME}.dll" "${MYODBC_UNICODE_NAME}.dll" "${MYODBC_SETUP_NAME}.dll"
  "libcrypto-1_1-x64.dll" "libcrypto-1_1.dll"
  "libssl-1_1-x64.dll" "libssl-1_1.dll"
  "libcrypto-3-x64.dll" "libssl-3-x64.dll"
  "myodbc-installer.exe")

FILE(WRITE "${XML_DEPENDENCIES}" "<Include xmlns='http://wixtoolset.org/schemas/v4/wxs'><Component Id='dependencies' Guid='${XML_DEPS_GUID}' SharedDllRefCount='yes' Bitness='${XML_COMPONENT_BITNESS}'>")
FOREACH(_MASK "*.dll" "*.exe")
  FILE(GLOB DEP_FILE_LIST "${CMAKE_CURRENT_BINARY_DIR}/${XML_FILE_SOURCE_ARCH}/${_MASK}")
  MESSAGE("LOOKING INSIDE ${CMAKE_CURRENT_BINARY_DIR}/${XML_FILE_SOURCE_ARCH}/${_MASK}")
  MESSAGE("DEP_FILE_LIST=${DEP_FILE_LIST}")
  FOREACH(_DEP_FILE ${DEP_FILE_LIST})
    get_filename_component(_DEP_FILE_RESULT ${_DEP_FILE} NAME)
    LIST(FIND EXCLUDED_FILES ${_DEP_FILE_RESULT} _INDEX_FOUND)
    IF(_INDEX_FOUND EQUAL -1)
      # Add files not in the exclude list
      FILE(APPEND "${XML_DEPENDENCIES}" "<File Id='${_DEP_FILE_RESULT}' Name='${_DEP_FILE_RESULT}' DiskId='1' Source='${XML_FILE_SOURCE_ARCH}\\${_DEP_FILE_RESULT}' />")
    ENDIF()
  ENDFOREACH()
ENDFOREACH()
FILE(APPEND "${XML_DEPENDENCIES}" "</Component></Include>")

#-----------------------------------------------------


CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_odbc.wxs.in
               ${CMAKE_CURRENT_BINARY_DIR}/mysql-odbc-${ODBC_VERSION}-${MSI_VERSION}.wxs @ONLY)
FOREACH(_WIX_INPUT
        mysql_common_ui.wxs
        mysql_odbc_fragment.wxs
        mysql-connector-odbc-msi-arpprops.wxs
        FolderExists.vbs)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${_WIX_INPUT}
                 ${CMAKE_CURRENT_BINARY_DIR}/${_WIX_INPUT} COPYONLY)
ENDFOREACH()

ADD_CUSTOM_TARGET(
        MSI_INSTALLER ALL
        DEPENDS ${MSI_PACKAGE})

ADD_CUSTOM_COMMAND(
        OUTPUT ${MSI_PACKAGE}
        DEPENDS mysql_odbc.wxs.in
                mysql_common_ui.wxs
        COMMAND "${WIX_EXECUTABLE}" build -arch ${XML_FILE_SOURCE_ARCH}
                -out ${MSI_PACKAGE}
                mysql-odbc-${ODBC_VERSION}-${MSI_VERSION}.wxs
                mysql_odbc_fragment.wxs
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
