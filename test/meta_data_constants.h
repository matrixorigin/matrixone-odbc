// Copyright (c) 2025, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms, as
// designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of Connector/ODBC, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// https://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

#ifndef ODBC_META_DATA_CONSTANTS_H
#define ODBC_META_DATA_CONSTANTS_H


#define MYSQL_TYPE_LIST(_) \
  MYSQL_TYPES_NUMERIC(_) \
  MYSQL_TYPES_TEMPORAL(_) \
  MYSQL_TYPES_STRING(_) \
  MYSQL_TYPES_SPATIAL(_) \
  MYSQL_TYPES_OTHER(_) \


#define MYSQL_TYPES_NUMERIC(_) \
 _(numeric, integer) \
 _(numeric, smallint) \
 _(numeric, tinyint) \
 _(numeric, mediumint) \
 _(numeric, bigint) \
 _(numeric, decimal) \
 _(numeric, float) \
 _(numeric, double) \

#define MYSQL_TYPES_TEMPORAL(_) \
 _(temporal, date) \
 _(temporal, time) \
 _(temporal, datetime) \
 _(temporal, timestamp) \
 _(temporal, year) \

#define MYSQL_TYPES_STRING(_) \
 _(string, char) \
 _(string, varchar) \
 _(string, text) \
 _(string, tinytext) \
 _(string, mediumtext) \
 _(string, longtext) \
 _(string, binary) \
 _(string, varbinary) \
 _(string, blob) \
 _(string, tinyblob) \
 _(string, mediumblob) \
 _(string, longblob) \
 _(string, set) \
 _(string, enum) \

#define MYSQL_TYPES_SPATIAL(_) \
 _(spatial, geometry) \
 _(spatial, geometrycollection) \
 _(spatial, point) \
 _(spatial, multipoint) \
 _(spatial, linestring) \
 _(spatial, multilinestring) \
 _(spatial, polygon) \
 _(spatial, multipolygon) \

#define MYSQL_TYPES_OTHER(_) \
 _(other, json) \
 _(other, bit) \
 _(other, vector) /* from 9.0.0 */ \


/*
  MySQL column attributes

  [1] https://dev.mysql.com/doc/refman/en/create-table.html
  [2] https://dev.mysql.com/doc/refman/en/data-types.html

  Note: For unique/primary attribute the value true means that KEY is also
  specified, false that it is given without KEY.

  Note: The optional value of KEY attribute is the key prefix length.

  Note: Attributes that are ignored or tested elsewhere

  - SIGNED
  - VISIBLE | INVISIBLE
  - DEFAULT ...
  - COMMENT ...
  - COLUMN_FORMAT ...
  - [SECONDARY_]ENGINE_ATTRIBUTE ..
  - STORAGE ...

  Note: Currently we don't test SIGNED attribute, only UNSIGNED one.
*/

#define MYSQL_ATTR_LIST(_) \
  _(not_null, bool) \
  _(unsigned, bool) \
  _(auto_increment, unit_t) \
  _(unique, unit_t) \
  _(key, optional<int>) \


// --------------------------------------------------------

/*
  ODBC SQL types

  Note: The SQL_DATETIME and SQL_INTERVAL codes are used for verbose
  representation of datetime/interval types -- see SQL_DATA_TYPE column
  in [2] and SQL_DESC_TYPE field in [3]

  [1] https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-data-types?view=sql-server-ver16

  [2] https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function?view=sql-server-ver16

  [3] https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescfield-function?view=sql-server-ver16
*/

#define ODBC_TYPE_LIST(_) \
  _(string, char, SQL_CHAR) \
  _(string, varchar, SQL_VARCHAR) \
  _(string, longvarchar, SQL_LONGVARCHAR) \
  _(string, wchar, SQL_WCHAR) \
  _(string, wvarchar, SQL_WVARCHAR) \
  _(string, wlongvarchar, SQL_WLONGVARCHAR) \
  \
  _(binary, binary, SQL_BINARY) \
  _(binary, varbinary, SQL_VARBINARY) \
  _(binary, longvarbinary, SQL_LONGVARBINARY) \
  \
  _(numeric, decimal, SQL_DECIMAL) \
  _(numeric, numeric, SQL_NUMERIC) \
  _(numeric, smallint, SQL_SMALLINT) \
  _(numeric, integer, SQL_INTEGER) \
  _(numeric, real, SQL_REAL) \
  _(numeric, float, SQL_FLOAT) \
  _(numeric, double, SQL_DOUBLE) \
  _(numeric, tinyint, SQL_TINYINT) \
  _(numeric, bigint, SQL_BIGINT) \
  \
  _(datetime, type_date, SQL_TYPE_DATE) \
  _(datetime, type_time, SQL_TYPE_TIME) \
  _(datetime, type_timestamp, SQL_TYPE_TIMESTAMP) \
  _(interval, interval_month, SQL_INTERVAL_MONTH) \
  _(interval, interval_year, SQL_INTERVAL_YEAR) \
  _(interval, interval_year_to_month, SQL_INTERVAL_YEAR_TO_MONTH) \
  _(interval, interval_day, SQL_INTERVAL_DAY) \
  _(interval, interval_hour, SQL_INTERVAL_HOUR) \
  _(interval, interval_minute, SQL_INTERVAL_MINUTE) \
  _(interval, interval_second, SQL_INTERVAL_SECOND) \
  _(interval, interval_day_to_hour, SQL_INTERVAL_DAY_TO_HOUR) \
  _(interval, interval_day_to_minute, SQL_INTERVAL_DAY_TO_MINUTE) \
  _(interval, interval_day_to_second, SQL_INTERVAL_DAY_TO_SECOND) \
  _(interval, interval_hour_to_minute, SQL_INTERVAL_HOUR_TO_MINUTE) \
  _(interval, interval_hour_to_second, SQL_INTERVAL_HOUR_TO_SECOND) \
  _(interval, interval_minute_to_second, SQL_INTERVAL_MINUTE_TO_SECOND) \
  _(other, bit, SQL_BIT) \
  _(other, guid, SQL_GUID) \
  _(other, datetime, SQL_DATETIME) \
  _(other, interval, SQL_INTERVAL) \


// IRD fields that can be read with SQLColAttribute()

#define COL_ATTR_LIST(_) \
  DESC_REC_FIELDS(_) \
  _(auto_unique_value, bool, SQL_DESC_AUTO_UNIQUE_VALUE) \
  _(base_column_name, string, SQL_DESC_BASE_COLUMN_NAME) \
  _(base_table_name, string, SQL_DESC_BASE_TABLE_NAME) \
  _(case_sensitive, bool, SQL_DESC_CASE_SENSITIVE) \
  _(catalog_name, string, SQL_DESC_CATALOG_NAME) \
  _(concise_type, enum, SQL_DESC_CONCISE_TYPE) \
  _(display_size, integer, SQL_DESC_DISPLAY_SIZE) \
  _(fixed_prec_scale, bool, SQL_DESC_FIXED_PREC_SCALE) \
  _(label, string, SQL_DESC_LABEL) \
  _(length, ulen, SQL_DESC_LENGTH) \
  _(literal_prefix, string, SQL_DESC_LITERAL_PREFIX) \
  _(literal_suffix, string, SQL_DESC_LITERAL_SUFFIX) \
  _(local_type_name, string, SQL_DESC_LOCAL_TYPE_NAME) \
  _(num_prec_radix, integer, SQL_DESC_NUM_PREC_RADIX) \
  _(schema_name, string, SQL_DESC_SCHEMA_NAME) \
  _(searchable, enum, SQL_DESC_SEARCHABLE) \
  _(table_name, string, SQL_DESC_TABLE_NAME) \
  _(type_name, string, SQL_DESC_TYPE_NAME) \
  _(unnamed, enum, SQL_DESC_UNNAMED) \
  _(unsigned, bool, SQL_DESC_UNSIGNED) \
  _(updatable, enum, SQL_DESC_UPDATABLE) \

#define DESC_REC_FIELDS(_) \
  _(name, string, SQL_DESC_NAME) \
  _(type, enum, SQL_DESC_TYPE) \
  _(octet_length, len, SQL_DESC_OCTET_LENGTH) \
  _(precision, smallint, SQL_DESC_PRECISION) \
  _(scale, smallint, SQL_DESC_SCALE) \
  _(nullable, enum, SQL_DESC_NULLABLE) \

// IRD fields that can be read with SQLGetDescRec()

#define DESC_REC_LIST(_) \
  DESC_REC_FIELDS(_) \
  _(datetime_interval_code, enum, SQL_DESC_DATETIME_INTERVAL_CODE) \

#define DESC_EXTRA_LIST(_) \
  _(datetime_interval_code, enum, SQL_DESC_DATETIME_INTERVAL_CODE) \
  _(datetime_interval_precision, integer, SQL_DESC_DATETIME_INTERVAL_PRECISION) \
  _(rowver, bool, SQL_DESC_ROWVER) \

// All IRD fields (accessed via SQLGetDescField())

#define DESC_FIELD_LIST(_) \
  COL_ATTR_LIST(_) \
  DESC_EXTRA_LIST(_) \


// ODBC enumeration constants

#define UNNAMED_VAL_LIST(_) \
  _(named, SQL_NAMED) \
  _(unnamed, SQL_UNNAMED) \


#define NULLABLE_VAL_LIST(_) \
  _(nullable, SQL_NULLABLE) \
  _(no_nulls, SQL_NO_NULLS) \
  _(unknown, SQL_NULLABLE_UNKNOWN) \


#define SEARCHABLE_VAL_LIST(_) \
  _(none, SQL_PRED_NONE) \
  _(char, SQL_PRED_CHAR) \
  _(basic, SQL_PRED_BASIC) \
  _(searchable, SQL_PRED_SEARCHABLE) \


#define UPDATABLE_VAL_LIST(_) \
  _(readonly, SQL_ATTR_READONLY) \
  _(write, SQL_ATTR_WRITE) \
  _(unknown, SQL_ATTR_READWRITE_UNKNOWN) \


#define INTERVAL_CODE_VAL_LIST(_) \
  _(day, SQL_CODE_DAY) \
  _(hour, SQL_CODE_HOUR) \
  _(day_to_hour, SQL_CODE_DAY_TO_HOUR) \
  _(day_to_minute, SQL_CODE_DAY_TO_MINUTE) \
  _(day_to_second, SQL_CODE_DAY_TO_SECOND) \
  _(hour_to_minute, SQL_CODE_HOUR_TO_MINUTE) \
  _(hour_to_second, SQL_CODE_HOUR_TO_SECOND) \
  _(minute, SQL_CODE_MINUTE) \
  _(minute_to_second, SQL_CODE_MINUTE_TO_SECOND) \
  _(month, SQL_CODE_MONTH) \
  _(second, SQL_CODE_SECOND) \
  _(year, SQL_CODE_YEAR) \
  _(year_to_month, SQL_CODE_YEAR_TO_MONTH) \

#define INTERVAL_CODE_TYPE(_) \
  _(date, SQL_CODE_DATE) \
  _(time, SQL_CODE_TIME) \
  _(timestamp, SQL_CODE_TIMESTAMP) \


#endif