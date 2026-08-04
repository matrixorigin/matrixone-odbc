# Copyright (c) 2025, Oracle and/or its affiliates.
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

CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/pbi/MatrixOne.version.pqm.in ${CMAKE_SOURCE_DIR}/pbi/MatrixOne.version.pqm)

set(PBI_FILES
    "Diagnostics.pqm"
    "MatrixOne.pq"
    "MatrixOne.version.pqm"
    "MatrixOne16.png"
    "MatrixOne20.png"
    "MatrixOne24.png"
    "MatrixOne32.png"
    "MatrixOne40.png"
    "MatrixOne48.png"
    "MatrixOne64.png"
    "MatrixOne80.png"
    "OdbcConstants.pqm"
    "resources.resx"
)

# Output file names
set(ZIP_OUTPUT "${CMAKE_BINARY_DIR}/pbi/MatrixOne.zip")
set(MEZ_OUTPUT "${CMAKE_BINARY_DIR}/pbi/MatrixOne.mez")

# We are not installing files anywhere.
# They will have be picked up from the binary directory if required.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/pbi")

# cmake can create zip archives
add_custom_command(
    OUTPUT "${MEZ_OUTPUT}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E tar cf "${MEZ_OUTPUT}" --format=zip -- ${PBI_FILES}
    DEPENDS ${PBI_FILES}
    COMMENT "Creating mez file in ${CMAKE_BINARY_DIR}/pbi"
    VERBATIM
)

add_custom_target(make_mez ALL
    DEPENDS "${MEZ_OUTPUT}"
)