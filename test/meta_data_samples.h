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

#pragma once

#define SAMPLE_LIST(_) \
  SAMPLE_LIST_NUMERIC(_) \
  SAMPLE_LIST_TEMPORAL(_) \
  SAMPLE_LIST_STRING(_) \
  SAMPLE_LIST_SPATIAL(_) \
  SAMPLE_LIST_OTHER(_) \


#define SAMPLE_LIST_VECTOR(_) \
  _(37,0,vector,vector,({})) \
  _(37,1,vector,vector,({}), a_not_null(true)) \
  _(37,2,vector,vector,({}), a_not_null(false)) \
  _(37,3,vector,vector,({0})) \
  _(37,4,vector,vector,({0}), a_not_null(true)) \
  _(37,5,vector,vector,({0}), a_not_null(false)) \
  _(37,6,vector,vector,({16383})) \
  _(37,7,vector,vector,({16383}), a_not_null(true)) \
  _(37,8,vector,vector,({16383}), a_not_null(false)) \


#define SAMPLE_LIST_NUMERIC(_) \
  _(0,0,numeric,integer,({})) \
  _(0,1,numeric,integer,({}), a_not_null(true)) \
  _(0,2,numeric,integer,({}), a_not_null(false)) \
  _(0,3,numeric,integer,({}), a_unique({})) \
  _(0,4,numeric,integer,({}), a_not_null(true), a_unique({})) \
  _(0,5,numeric,integer,({}), a_not_null(false), a_unique({})) \
  _(0,6,numeric,integer,({}), a_key({})) \
  _(0,7,numeric,integer,({}), a_key({}), a_not_null(true)) \
  _(0,8,numeric,integer,({}), a_key({}), a_unique({})) \
  _(0,9,numeric,integer,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(0,10,numeric,integer,({}), a_unsigned(true)) \
  _(0,11,numeric,integer,({}), a_unsigned(false)) \
  _(0,12,numeric,integer,({}), a_not_null(true), a_unsigned(true)) \
  _(0,13,numeric,integer,({}), a_not_null(true), a_unsigned(false)) \
  _(0,14,numeric,integer,({}), a_not_null(false), a_unsigned(true)) \
  _(0,15,numeric,integer,({}), a_not_null(false), a_unsigned(false)) \
  _(0,16,numeric,integer,({}), a_unique({}), a_unsigned(true)) \
  _(0,17,numeric,integer,({}), a_unique({}), a_unsigned(false)) \
  _(0,18,numeric,integer,({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(0,19,numeric,integer,({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(0,20,numeric,integer,({}), a_not_null(false), a_unique({}), a_unsigned(true)) \
  _(0,21,numeric,integer,({}), a_not_null(false), a_unique({}), a_unsigned(false)) \
  _(0,22,numeric,integer,({}), a_key({}), a_unsigned(true)) \
  _(0,23,numeric,integer,({}), a_key({}), a_unsigned(false)) \
  _(0,24,numeric,integer,({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(0,25,numeric,integer,({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(0,26,numeric,integer,({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(0,27,numeric,integer,({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(0,28,numeric,integer,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(0,29,numeric,integer,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(0,30,numeric,integer,({}), a_auto_increment({}), a_key({})) \
  _(0,31,numeric,integer,({}), a_auto_increment({}), a_key({}), a_not_null(true)) \
  _(0,32,numeric,integer,({}), a_auto_increment({}), a_key({}), a_unique({})) \
  _(0,33,numeric,integer,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({})) \
  _(0,34,numeric,integer,({}), a_auto_increment({}), a_key({}), a_unsigned(true)) \
  _(0,35,numeric,integer,({}), a_auto_increment({}), a_key({}), a_unsigned(false)) \
  _(0,36,numeric,integer,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(0,37,numeric,integer,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(0,38,numeric,integer,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(0,39,numeric,integer,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(0,40,numeric,integer,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(0,41,numeric,integer,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(1,0,numeric,smallint,({})) \
  _(1,1,numeric,smallint,({}), a_not_null(true)) \
  _(1,2,numeric,smallint,({}), a_not_null(false)) \
  _(1,3,numeric,smallint,({}), a_unique({})) \
  _(1,4,numeric,smallint,({}), a_not_null(true), a_unique({})) \
  _(1,5,numeric,smallint,({}), a_not_null(false), a_unique({})) \
  _(1,6,numeric,smallint,({}), a_key({})) \
  _(1,7,numeric,smallint,({}), a_key({}), a_not_null(true)) \
  _(1,8,numeric,smallint,({}), a_key({}), a_unique({})) \
  _(1,9,numeric,smallint,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(1,10,numeric,smallint,({}), a_unsigned(true)) \
  _(1,11,numeric,smallint,({}), a_unsigned(false)) \
  _(1,12,numeric,smallint,({}), a_not_null(true), a_unsigned(true)) \
  _(1,13,numeric,smallint,({}), a_not_null(true), a_unsigned(false)) \
  _(1,14,numeric,smallint,({}), a_not_null(false), a_unsigned(true)) \
  _(1,15,numeric,smallint,({}), a_not_null(false), a_unsigned(false)) \
  _(1,16,numeric,smallint,({}), a_unique({}), a_unsigned(true)) \
  _(1,17,numeric,smallint,({}), a_unique({}), a_unsigned(false)) \
  _(1,18,numeric,smallint,({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(1,19,numeric,smallint,({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(1,20,numeric,smallint,({}), a_not_null(false), a_unique({}), a_unsigned(true)) \
  _(1,21,numeric,smallint,({}), a_not_null(false), a_unique({}), a_unsigned(false)) \
  _(1,22,numeric,smallint,({}), a_key({}), a_unsigned(true)) \
  _(1,23,numeric,smallint,({}), a_key({}), a_unsigned(false)) \
  _(1,24,numeric,smallint,({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(1,25,numeric,smallint,({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(1,26,numeric,smallint,({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(1,27,numeric,smallint,({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(1,28,numeric,smallint,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(1,29,numeric,smallint,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(1,30,numeric,smallint,({}), a_auto_increment({}), a_key({})) \
  _(1,31,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_not_null(true)) \
  _(1,32,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_unique({})) \
  _(1,33,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({})) \
  _(1,34,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_unsigned(true)) \
  _(1,35,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_unsigned(false)) \
  _(1,36,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(1,37,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(1,38,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(1,39,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(1,40,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(1,41,numeric,smallint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(2,0,numeric,tinyint,({})) \
  _(2,1,numeric,tinyint,({}), a_not_null(true)) \
  _(2,2,numeric,tinyint,({}), a_not_null(false)) \
  _(2,3,numeric,tinyint,({}), a_unique({})) \
  _(2,4,numeric,tinyint,({}), a_not_null(true), a_unique({})) \
  _(2,5,numeric,tinyint,({}), a_not_null(false), a_unique({})) \
  _(2,6,numeric,tinyint,({}), a_key({})) \
  _(2,7,numeric,tinyint,({}), a_key({}), a_not_null(true)) \
  _(2,8,numeric,tinyint,({}), a_key({}), a_unique({})) \
  _(2,9,numeric,tinyint,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(2,10,numeric,tinyint,({}), a_unsigned(true)) \
  _(2,11,numeric,tinyint,({}), a_unsigned(false)) \
  _(2,12,numeric,tinyint,({}), a_not_null(true), a_unsigned(true)) \
  _(2,13,numeric,tinyint,({}), a_not_null(true), a_unsigned(false)) \
  _(2,14,numeric,tinyint,({}), a_not_null(false), a_unsigned(true)) \
  _(2,15,numeric,tinyint,({}), a_not_null(false), a_unsigned(false)) \
  _(2,16,numeric,tinyint,({}), a_unique({}), a_unsigned(true)) \
  _(2,17,numeric,tinyint,({}), a_unique({}), a_unsigned(false)) \
  _(2,18,numeric,tinyint,({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(2,19,numeric,tinyint,({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(2,20,numeric,tinyint,({}), a_not_null(false), a_unique({}), a_unsigned(true)) \
  _(2,21,numeric,tinyint,({}), a_not_null(false), a_unique({}), a_unsigned(false)) \
  _(2,22,numeric,tinyint,({}), a_key({}), a_unsigned(true)) \
  _(2,23,numeric,tinyint,({}), a_key({}), a_unsigned(false)) \
  _(2,24,numeric,tinyint,({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(2,25,numeric,tinyint,({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(2,26,numeric,tinyint,({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(2,27,numeric,tinyint,({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(2,28,numeric,tinyint,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(2,29,numeric,tinyint,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(2,30,numeric,tinyint,({}), a_auto_increment({}), a_key({})) \
  _(2,31,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_not_null(true)) \
  _(2,32,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_unique({})) \
  _(2,33,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({})) \
  _(2,34,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_unsigned(true)) \
  _(2,35,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_unsigned(false)) \
  _(2,36,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(2,37,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(2,38,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(2,39,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(2,40,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(2,41,numeric,tinyint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(3,0,numeric,mediumint,({})) \
  _(3,1,numeric,mediumint,({}), a_not_null(true)) \
  _(3,2,numeric,mediumint,({}), a_not_null(false)) \
  _(3,3,numeric,mediumint,({}), a_unique({})) \
  _(3,4,numeric,mediumint,({}), a_not_null(true), a_unique({})) \
  _(3,5,numeric,mediumint,({}), a_not_null(false), a_unique({})) \
  _(3,6,numeric,mediumint,({}), a_key({})) \
  _(3,7,numeric,mediumint,({}), a_key({}), a_not_null(true)) \
  _(3,8,numeric,mediumint,({}), a_key({}), a_unique({})) \
  _(3,9,numeric,mediumint,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(3,10,numeric,mediumint,({}), a_unsigned(true)) \
  _(3,11,numeric,mediumint,({}), a_unsigned(false)) \
  _(3,12,numeric,mediumint,({}), a_not_null(true), a_unsigned(true)) \
  _(3,13,numeric,mediumint,({}), a_not_null(true), a_unsigned(false)) \
  _(3,14,numeric,mediumint,({}), a_not_null(false), a_unsigned(true)) \
  _(3,15,numeric,mediumint,({}), a_not_null(false), a_unsigned(false)) \
  _(3,16,numeric,mediumint,({}), a_unique({}), a_unsigned(true)) \
  _(3,17,numeric,mediumint,({}), a_unique({}), a_unsigned(false)) \
  _(3,18,numeric,mediumint,({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(3,19,numeric,mediumint,({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(3,20,numeric,mediumint,({}), a_not_null(false), a_unique({}), a_unsigned(true)) \
  _(3,21,numeric,mediumint,({}), a_not_null(false), a_unique({}), a_unsigned(false)) \
  _(3,22,numeric,mediumint,({}), a_key({}), a_unsigned(true)) \
  _(3,23,numeric,mediumint,({}), a_key({}), a_unsigned(false)) \
  _(3,24,numeric,mediumint,({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(3,25,numeric,mediumint,({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(3,26,numeric,mediumint,({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(3,27,numeric,mediumint,({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(3,28,numeric,mediumint,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(3,29,numeric,mediumint,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(3,30,numeric,mediumint,({}), a_auto_increment({}), a_key({})) \
  _(3,31,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_not_null(true)) \
  _(3,32,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_unique({})) \
  _(3,33,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({})) \
  _(3,34,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_unsigned(true)) \
  _(3,35,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_unsigned(false)) \
  _(3,36,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(3,37,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(3,38,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(3,39,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(3,40,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(3,41,numeric,mediumint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(4,0,numeric,bigint,({})) \
  _(4,1,numeric,bigint,({}), a_not_null(true)) \
  _(4,2,numeric,bigint,({}), a_not_null(false)) \
  _(4,3,numeric,bigint,({}), a_unique({})) \
  _(4,4,numeric,bigint,({}), a_not_null(true), a_unique({})) \
  _(4,5,numeric,bigint,({}), a_not_null(false), a_unique({})) \
  _(4,6,numeric,bigint,({}), a_key({})) \
  _(4,7,numeric,bigint,({}), a_key({}), a_not_null(true)) \
  _(4,8,numeric,bigint,({}), a_key({}), a_unique({})) \
  _(4,9,numeric,bigint,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(4,10,numeric,bigint,({}), a_unsigned(true)) \
  _(4,11,numeric,bigint,({}), a_unsigned(false)) \
  _(4,12,numeric,bigint,({}), a_not_null(true), a_unsigned(true)) \
  _(4,13,numeric,bigint,({}), a_not_null(true), a_unsigned(false)) \
  _(4,14,numeric,bigint,({}), a_not_null(false), a_unsigned(true)) \
  _(4,15,numeric,bigint,({}), a_not_null(false), a_unsigned(false)) \
  _(4,16,numeric,bigint,({}), a_unique({}), a_unsigned(true)) \
  _(4,17,numeric,bigint,({}), a_unique({}), a_unsigned(false)) \
  _(4,18,numeric,bigint,({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(4,19,numeric,bigint,({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(4,20,numeric,bigint,({}), a_not_null(false), a_unique({}), a_unsigned(true)) \
  _(4,21,numeric,bigint,({}), a_not_null(false), a_unique({}), a_unsigned(false)) \
  _(4,22,numeric,bigint,({}), a_key({}), a_unsigned(true)) \
  _(4,23,numeric,bigint,({}), a_key({}), a_unsigned(false)) \
  _(4,24,numeric,bigint,({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(4,25,numeric,bigint,({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(4,26,numeric,bigint,({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(4,27,numeric,bigint,({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(4,28,numeric,bigint,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(4,29,numeric,bigint,({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(4,30,numeric,bigint,({}), a_auto_increment({}), a_key({})) \
  _(4,31,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_not_null(true)) \
  _(4,32,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_unique({})) \
  _(4,33,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({})) \
  _(4,34,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_unsigned(true)) \
  _(4,35,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_unsigned(false)) \
  _(4,36,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(true)) \
  _(4,37,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unsigned(false)) \
  _(4,38,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(true)) \
  _(4,39,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_unique({}), a_unsigned(false)) \
  _(4,40,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(true)) \
  _(4,41,numeric,bigint,({}), a_auto_increment({}), a_key({}), a_not_null(true), a_unique({}), a_unsigned(false)) \
  _(5,0,numeric,decimal,({})) \
  _(5,1,numeric,decimal,({}), a_not_null(true)) \
  _(5,2,numeric,decimal,({}), a_not_null(false)) \
  _(5,3,numeric,decimal,({}), a_unique({})) \
  _(5,4,numeric,decimal,({}), a_not_null(true), a_unique({})) \
  _(5,5,numeric,decimal,({}), a_not_null(false), a_unique({})) \
  _(5,6,numeric,decimal,({}), a_key({})) \
  _(5,7,numeric,decimal,({}), a_key({}), a_not_null(true)) \
  _(5,8,numeric,decimal,({}), a_key({}), a_unique({})) \
  _(5,9,numeric,decimal,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(5,10,numeric,decimal,({65})) \
  _(5,11,numeric,decimal,({65}), a_not_null(true)) \
  _(5,12,numeric,decimal,({65}), a_not_null(false)) \
  _(5,13,numeric,decimal,({65}), a_unique({})) \
  _(5,14,numeric,decimal,({65}), a_not_null(true), a_unique({})) \
  _(5,15,numeric,decimal,({65}), a_not_null(false), a_unique({})) \
  _(5,16,numeric,decimal,({65}), a_key({})) \
  _(5,17,numeric,decimal,({65}), a_key({}), a_not_null(true)) \
  _(5,18,numeric,decimal,({65}), a_key({}), a_unique({})) \
  _(5,19,numeric,decimal,({65}), a_key({}), a_not_null(true), a_unique({})) \
  _(5,20,numeric,decimal,({6})) \
  _(5,21,numeric,decimal,({6}), a_not_null(true)) \
  _(5,22,numeric,decimal,({6}), a_not_null(false)) \
  _(5,23,numeric,decimal,({6}), a_unique({})) \
  _(5,24,numeric,decimal,({6}), a_not_null(true), a_unique({})) \
  _(5,25,numeric,decimal,({6}), a_not_null(false), a_unique({})) \
  _(5,26,numeric,decimal,({6}), a_key({})) \
  _(5,27,numeric,decimal,({6}), a_key({}), a_not_null(true)) \
  _(5,28,numeric,decimal,({6}), a_key({}), a_unique({})) \
  _(5,29,numeric,decimal,({6}), a_key({}), a_not_null(true), a_unique({})) \
  _(5,30,numeric,decimal,({65,2})) \
  _(5,31,numeric,decimal,({65,2}), a_not_null(true)) \
  _(5,32,numeric,decimal,({65,2}), a_not_null(false)) \
  _(5,33,numeric,decimal,({65,2}), a_unique({})) \
  _(5,34,numeric,decimal,({65,2}), a_not_null(true), a_unique({})) \
  _(5,35,numeric,decimal,({65,2}), a_not_null(false), a_unique({})) \
  _(5,36,numeric,decimal,({65,2}), a_key({})) \
  _(5,37,numeric,decimal,({65,2}), a_key({}), a_not_null(true)) \
  _(5,38,numeric,decimal,({65,2}), a_key({}), a_unique({})) \
  _(5,39,numeric,decimal,({65,2}), a_key({}), a_not_null(true), a_unique({})) \
  _(5,40,numeric,decimal,({6,2})) \
  _(5,41,numeric,decimal,({6,2}), a_not_null(true)) \
  _(5,42,numeric,decimal,({6,2}), a_not_null(false)) \
  _(5,43,numeric,decimal,({6,2}), a_unique({})) \
  _(5,44,numeric,decimal,({6,2}), a_not_null(true), a_unique({})) \
  _(5,45,numeric,decimal,({6,2}), a_not_null(false), a_unique({})) \
  _(5,46,numeric,decimal,({6,2}), a_key({})) \
  _(5,47,numeric,decimal,({6,2}), a_key({}), a_not_null(true)) \
  _(5,48,numeric,decimal,({6,2}), a_key({}), a_unique({})) \
  _(5,49,numeric,decimal,({6,2}), a_key({}), a_not_null(true), a_unique({})) \
  _(6,0,numeric,float,({})) \
  _(6,1,numeric,float,({}), a_not_null(true)) \
  _(6,2,numeric,float,({}), a_not_null(false)) \
  _(6,3,numeric,float,({}), a_unique({})) \
  _(6,4,numeric,float,({}), a_not_null(true), a_unique({})) \
  _(6,5,numeric,float,({}), a_not_null(false), a_unique({})) \
  _(6,6,numeric,float,({}), a_key({})) \
  _(6,7,numeric,float,({}), a_key({}), a_not_null(true)) \
  _(6,8,numeric,float,({}), a_key({}), a_unique({})) \
  _(6,9,numeric,float,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(6,10,numeric,float,({20})) \
  _(6,11,numeric,float,({20}), a_not_null(true)) \
  _(6,12,numeric,float,({20}), a_not_null(false)) \
  _(6,13,numeric,float,({20}), a_unique({})) \
  _(6,14,numeric,float,({20}), a_not_null(true), a_unique({})) \
  _(6,15,numeric,float,({20}), a_not_null(false), a_unique({})) \
  _(6,16,numeric,float,({20}), a_key({})) \
  _(6,17,numeric,float,({20}), a_key({}), a_not_null(true)) \
  _(6,18,numeric,float,({20}), a_key({}), a_unique({})) \
  _(6,19,numeric,float,({20}), a_key({}), a_not_null(true), a_unique({})) \
  _(6,20,numeric,float,({30})) \
  _(6,21,numeric,float,({30}), a_not_null(true)) \
  _(6,22,numeric,float,({30}), a_not_null(false)) \
  _(6,23,numeric,float,({30}), a_unique({})) \
  _(6,24,numeric,float,({30}), a_not_null(true), a_unique({})) \
  _(6,25,numeric,float,({30}), a_not_null(false), a_unique({})) \
  _(6,26,numeric,float,({30}), a_key({})) \
  _(6,27,numeric,float,({30}), a_key({}), a_not_null(true)) \
  _(6,28,numeric,float,({30}), a_key({}), a_unique({})) \
  _(6,29,numeric,float,({30}), a_key({}), a_not_null(true), a_unique({})) \
  _(7,0,numeric,double,({})) \
  _(7,1,numeric,double,({}), a_not_null(true)) \
  _(7,2,numeric,double,({}), a_not_null(false)) \
  _(7,3,numeric,double,({}), a_unique({})) \
  _(7,4,numeric,double,({}), a_not_null(true), a_unique({})) \
  _(7,5,numeric,double,({}), a_not_null(false), a_unique({})) \
  _(7,6,numeric,double,({}), a_key({})) \
  _(7,7,numeric,double,({}), a_key({}), a_not_null(true)) \
  _(7,8,numeric,double,({}), a_key({}), a_unique({})) \
  _(7,9,numeric,double,({}), a_key({}), a_not_null(true), a_unique({})) \

#define SAMPLE_LIST_TEMPORAL(_) \
  _(8,0,temporal,date,({})) \
  _(8,1,temporal,date,({}), a_not_null(true)) \
  _(8,2,temporal,date,({}), a_not_null(false)) \
  _(8,3,temporal,date,({}), a_unique({})) \
  _(8,4,temporal,date,({}), a_not_null(true), a_unique({})) \
  _(8,5,temporal,date,({}), a_not_null(false), a_unique({})) \
  _(8,6,temporal,date,({}), a_key({})) \
  _(8,7,temporal,date,({}), a_key({}), a_not_null(true)) \
  _(8,8,temporal,date,({}), a_key({}), a_unique({})) \
  _(8,9,temporal,date,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(9,0,temporal,time,({})) \
  _(9,1,temporal,time,({}), a_not_null(true)) \
  _(9,2,temporal,time,({}), a_not_null(false)) \
  _(9,3,temporal,time,({}), a_unique({})) \
  _(9,4,temporal,time,({}), a_not_null(true), a_unique({})) \
  _(9,5,temporal,time,({}), a_not_null(false), a_unique({})) \
  _(9,6,temporal,time,({}), a_key({})) \
  _(9,7,temporal,time,({}), a_key({}), a_not_null(true)) \
  _(9,8,temporal,time,({}), a_key({}), a_unique({})) \
  _(9,9,temporal,time,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(9,10,temporal,time,({0})) \
  _(9,11,temporal,time,({0}), a_not_null(true)) \
  _(9,12,temporal,time,({0}), a_not_null(false)) \
  _(9,13,temporal,time,({0}), a_unique({})) \
  _(9,14,temporal,time,({0}), a_not_null(true), a_unique({})) \
  _(9,15,temporal,time,({0}), a_not_null(false), a_unique({})) \
  _(9,16,temporal,time,({0}), a_key({})) \
  _(9,17,temporal,time,({0}), a_key({}), a_not_null(true)) \
  _(9,18,temporal,time,({0}), a_key({}), a_unique({})) \
  _(9,19,temporal,time,({0}), a_key({}), a_not_null(true), a_unique({})) \
  _(9,20,temporal,time,({6})) \
  _(9,21,temporal,time,({6}), a_not_null(true)) \
  _(9,22,temporal,time,({6}), a_not_null(false)) \
  _(9,23,temporal,time,({6}), a_unique({})) \
  _(9,24,temporal,time,({6}), a_not_null(true), a_unique({})) \
  _(9,25,temporal,time,({6}), a_not_null(false), a_unique({})) \
  _(9,26,temporal,time,({6}), a_key({})) \
  _(9,27,temporal,time,({6}), a_key({}), a_not_null(true)) \
  _(9,28,temporal,time,({6}), a_key({}), a_unique({})) \
  _(9,29,temporal,time,({6}), a_key({}), a_not_null(true), a_unique({})) \
  _(9,30,temporal,time,({2})) \
  _(9,31,temporal,time,({2}), a_not_null(true)) \
  _(9,32,temporal,time,({2}), a_not_null(false)) \
  _(9,33,temporal,time,({2}), a_unique({})) \
  _(9,34,temporal,time,({2}), a_not_null(true), a_unique({})) \
  _(9,35,temporal,time,({2}), a_not_null(false), a_unique({})) \
  _(9,36,temporal,time,({2}), a_key({})) \
  _(9,37,temporal,time,({2}), a_key({}), a_not_null(true)) \
  _(9,38,temporal,time,({2}), a_key({}), a_unique({})) \
  _(9,39,temporal,time,({2}), a_key({}), a_not_null(true), a_unique({})) \
  _(10,0,temporal,datetime,({})) \
  _(10,1,temporal,datetime,({}), a_not_null(true)) \
  _(10,2,temporal,datetime,({}), a_not_null(false)) \
  _(10,3,temporal,datetime,({}), a_unique({})) \
  _(10,4,temporal,datetime,({}), a_not_null(true), a_unique({})) \
  _(10,5,temporal,datetime,({}), a_not_null(false), a_unique({})) \
  _(10,6,temporal,datetime,({}), a_key({})) \
  _(10,7,temporal,datetime,({}), a_key({}), a_not_null(true)) \
  _(10,8,temporal,datetime,({}), a_key({}), a_unique({})) \
  _(10,9,temporal,datetime,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(10,10,temporal,datetime,({0})) \
  _(10,11,temporal,datetime,({0}), a_not_null(true)) \
  _(10,12,temporal,datetime,({0}), a_not_null(false)) \
  _(10,13,temporal,datetime,({0}), a_unique({})) \
  _(10,14,temporal,datetime,({0}), a_not_null(true), a_unique({})) \
  _(10,15,temporal,datetime,({0}), a_not_null(false), a_unique({})) \
  _(10,16,temporal,datetime,({0}), a_key({})) \
  _(10,17,temporal,datetime,({0}), a_key({}), a_not_null(true)) \
  _(10,18,temporal,datetime,({0}), a_key({}), a_unique({})) \
  _(10,19,temporal,datetime,({0}), a_key({}), a_not_null(true), a_unique({})) \
  _(10,20,temporal,datetime,({6})) \
  _(10,21,temporal,datetime,({6}), a_not_null(true)) \
  _(10,22,temporal,datetime,({6}), a_not_null(false)) \
  _(10,23,temporal,datetime,({6}), a_unique({})) \
  _(10,24,temporal,datetime,({6}), a_not_null(true), a_unique({})) \
  _(10,25,temporal,datetime,({6}), a_not_null(false), a_unique({})) \
  _(10,26,temporal,datetime,({6}), a_key({})) \
  _(10,27,temporal,datetime,({6}), a_key({}), a_not_null(true)) \
  _(10,28,temporal,datetime,({6}), a_key({}), a_unique({})) \
  _(10,29,temporal,datetime,({6}), a_key({}), a_not_null(true), a_unique({})) \
  _(10,30,temporal,datetime,({2})) \
  _(10,31,temporal,datetime,({2}), a_not_null(true)) \
  _(10,32,temporal,datetime,({2}), a_not_null(false)) \
  _(10,33,temporal,datetime,({2}), a_unique({})) \
  _(10,34,temporal,datetime,({2}), a_not_null(true), a_unique({})) \
  _(10,35,temporal,datetime,({2}), a_not_null(false), a_unique({})) \
  _(10,36,temporal,datetime,({2}), a_key({})) \
  _(10,37,temporal,datetime,({2}), a_key({}), a_not_null(true)) \
  _(10,38,temporal,datetime,({2}), a_key({}), a_unique({})) \
  _(10,39,temporal,datetime,({2}), a_key({}), a_not_null(true), a_unique({})) \
  _(11,0,temporal,timestamp,({})) \
  _(11,1,temporal,timestamp,({}), a_not_null(true)) \
  _(11,2,temporal,timestamp,({}), a_not_null(false)) \
  _(11,3,temporal,timestamp,({}), a_unique({})) \
  _(11,4,temporal,timestamp,({}), a_not_null(true), a_unique({})) \
  _(11,5,temporal,timestamp,({}), a_not_null(false), a_unique({})) \
  _(11,6,temporal,timestamp,({}), a_key({})) \
  _(11,7,temporal,timestamp,({}), a_key({}), a_not_null(true)) \
  _(11,8,temporal,timestamp,({}), a_key({}), a_unique({})) \
  _(11,9,temporal,timestamp,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(11,10,temporal,timestamp,({0})) \
  _(11,11,temporal,timestamp,({0}), a_not_null(true)) \
  _(11,12,temporal,timestamp,({0}), a_not_null(false)) \
  _(11,13,temporal,timestamp,({0}), a_unique({})) \
  _(11,14,temporal,timestamp,({0}), a_not_null(true), a_unique({})) \
  _(11,15,temporal,timestamp,({0}), a_not_null(false), a_unique({})) \
  _(11,16,temporal,timestamp,({0}), a_key({})) \
  _(11,17,temporal,timestamp,({0}), a_key({}), a_not_null(true)) \
  _(11,18,temporal,timestamp,({0}), a_key({}), a_unique({})) \
  _(11,19,temporal,timestamp,({0}), a_key({}), a_not_null(true), a_unique({})) \
  _(11,20,temporal,timestamp,({6})) \
  _(11,21,temporal,timestamp,({6}), a_not_null(true)) \
  _(11,22,temporal,timestamp,({6}), a_not_null(false)) \
  _(11,23,temporal,timestamp,({6}), a_unique({})) \
  _(11,24,temporal,timestamp,({6}), a_not_null(true), a_unique({})) \
  _(11,25,temporal,timestamp,({6}), a_not_null(false), a_unique({})) \
  _(11,26,temporal,timestamp,({6}), a_key({})) \
  _(11,27,temporal,timestamp,({6}), a_key({}), a_not_null(true)) \
  _(11,28,temporal,timestamp,({6}), a_key({}), a_unique({})) \
  _(11,29,temporal,timestamp,({6}), a_key({}), a_not_null(true), a_unique({})) \
  _(11,30,temporal,timestamp,({2})) \
  _(11,31,temporal,timestamp,({2}), a_not_null(true)) \
  _(11,32,temporal,timestamp,({2}), a_not_null(false)) \
  _(11,33,temporal,timestamp,({2}), a_unique({})) \
  _(11,34,temporal,timestamp,({2}), a_not_null(true), a_unique({})) \
  _(11,35,temporal,timestamp,({2}), a_not_null(false), a_unique({})) \
  _(11,36,temporal,timestamp,({2}), a_key({})) \
  _(11,37,temporal,timestamp,({2}), a_key({}), a_not_null(true)) \
  _(11,38,temporal,timestamp,({2}), a_key({}), a_unique({})) \
  _(11,39,temporal,timestamp,({2}), a_key({}), a_not_null(true), a_unique({})) \
  _(12,0,temporal,year,({})) \
  _(12,1,temporal,year,({}), a_not_null(true)) \
  _(12,2,temporal,year,({}), a_not_null(false)) \
  _(12,3,temporal,year,({}), a_unique({})) \
  _(12,4,temporal,year,({}), a_not_null(true), a_unique({})) \
  _(12,5,temporal,year,({}), a_not_null(false), a_unique({})) \
  _(12,6,temporal,year,({}), a_key({})) \
  _(12,7,temporal,year,({}), a_key({}), a_not_null(true)) \
  _(12,8,temporal,year,({}), a_key({}), a_unique({})) \
  _(12,9,temporal,year,({}), a_key({}), a_not_null(true), a_unique({})) \

#define SAMPLE_LIST_STRING(_) \
  _(13,0,string,char,({})) \
  _(13,1,string,char,({}), a_not_null(true)) \
  _(13,2,string,char,({}), a_not_null(false)) \
  _(13,3,string,char,({0})) \
  _(13,4,string,char,({0}), a_not_null(true)) \
  _(13,5,string,char,({0}), a_not_null(false)) \
  _(13,6,string,char,({255})) \
  _(13,7,string,char,({255}), a_not_null(true)) \
  _(13,8,string,char,({255}), a_not_null(false)) \
  _(13,9,string,char,({255}), a_unique({})) \
  _(13,10,string,char,({255}), a_not_null(true), a_unique({})) \
  _(13,11,string,char,({255}), a_not_null(false), a_unique({})) \
  _(13,12,string,char,({255}), a_key({})) \
  _(13,13,string,char,({255}), a_key(10)) \
  _(13,14,string,char,({255}), a_key({}), a_not_null(true)) \
  _(13,15,string,char,({255}), a_key(10), a_not_null(true)) \
  _(13,16,string,char,({255}), a_key({}), a_unique({})) \
  _(13,17,string,char,({255}), a_key(10), a_unique({})) \
  _(13,18,string,char,({255}), a_key({}), a_not_null(true), a_unique({})) \
  _(13,19,string,char,({255}), a_key(10), a_not_null(true), a_unique({})) \
  _(13,20,string,char,({60})) \
  _(13,21,string,char,({60}), a_not_null(true)) \
  _(13,22,string,char,({60}), a_not_null(false)) \
  _(13,23,string,char,({60}), a_unique({})) \
  _(13,24,string,char,({60}), a_not_null(true), a_unique({})) \
  _(13,25,string,char,({60}), a_not_null(false), a_unique({})) \
  _(13,26,string,char,({60}), a_key({})) \
  _(13,27,string,char,({60}), a_key(10)) \
  _(13,28,string,char,({60}), a_key({}), a_not_null(true)) \
  _(13,29,string,char,({60}), a_key(10), a_not_null(true)) \
  _(13,30,string,char,({60}), a_key({}), a_unique({})) \
  _(13,31,string,char,({60}), a_key(10), a_unique({})) \
  _(13,32,string,char,({60}), a_key({}), a_not_null(true), a_unique({})) \
  _(13,33,string,char,({60}), a_key(10), a_not_null(true), a_unique({})) \
  _(14,0,string,varchar,({0})) \
  _(14,1,string,varchar,({0}), a_not_null(true)) \
  _(14,2,string,varchar,({0}), a_not_null(false)) \
  _(14,3,string,varchar,({16381})) \
  _(14,4,string,varchar,({16381}), a_not_null(true)) \
  _(14,5,string,varchar,({16381}), a_not_null(false)) \
  _(14,6,string,varchar,({16381}), a_key(10)) \
  _(14,7,string,varchar,({16381}), a_key(10), a_not_null(true)) \
  _(14,8,string,varchar,({60})) \
  _(14,9,string,varchar,({60}), a_not_null(true)) \
  _(14,10,string,varchar,({60}), a_not_null(false)) \
  _(14,11,string,varchar,({60}), a_unique({})) \
  _(14,12,string,varchar,({60}), a_not_null(true), a_unique({})) \
  _(14,13,string,varchar,({60}), a_not_null(false), a_unique({})) \
  _(14,14,string,varchar,({60}), a_key({})) \
  _(14,15,string,varchar,({60}), a_key(10)) \
  _(14,16,string,varchar,({60}), a_key({}), a_not_null(true)) \
  _(14,17,string,varchar,({60}), a_key(10), a_not_null(true)) \
  _(14,18,string,varchar,({60}), a_key({}), a_unique({})) \
  _(14,19,string,varchar,({60}), a_key(10), a_unique({})) \
  _(14,20,string,varchar,({60}), a_key({}), a_not_null(true), a_unique({})) \
  _(14,21,string,varchar,({60}), a_key(10), a_not_null(true), a_unique({})) \
  _(15,0,string,text,({})) \
  _(15,1,string,text,({}), a_not_null(true)) \
  _(15,2,string,text,({}), a_not_null(false)) \
  _(15,3,string,text,({0})) \
  _(15,4,string,text,({0}), a_not_null(true)) \
  _(15,5,string,text,({0}), a_not_null(false)) \
  _(15,6,string,text,({0})) \
  _(15,7,string,text,({0}), a_not_null(true)) \
  _(15,8,string,text,({0}), a_not_null(false)) \
  _(15,9,string,text,({16250})) \
  _(15,10,string,text,({16250}), a_not_null(true)) \
  _(15,11,string,text,({16250}), a_not_null(false)) \
  _(15,12,string,text,({16250}), a_key(10)) \
  _(15,13,string,text,({16250}), a_key(10), a_not_null(true)) \
  _(15,14,string,text,({4000000})) \
  _(15,15,string,text,({4000000}), a_not_null(true)) \
  _(15,16,string,text,({4000000}), a_not_null(false)) \
  _(15,17,string,text,({4000000}), a_key(10)) \
  _(15,18,string,text,({4000000}), a_key(10), a_not_null(true)) \
  _(15,19,string,text,({1000000000})) \
  _(15,20,string,text,({1000000000}), a_not_null(true)) \
  _(15,21,string,text,({1000000000}), a_not_null(false)) \
  _(15,22,string,text,({1000000000}), a_key(10)) \
  _(15,23,string,text,({1000000000}), a_key(10), a_not_null(true)) \
  _(15,24,string,text,({60})) \
  _(15,25,string,text,({60}), a_not_null(true)) \
  _(15,26,string,text,({60}), a_not_null(false)) \
  _(15,27,string,text,({60}), a_key(10)) \
  _(15,28,string,text,({60}), a_key(10), a_not_null(true)) \
  _(16,0,string,tinytext,({})) \
  _(16,1,string,tinytext,({}), a_not_null(true)) \
  _(16,2,string,tinytext,({}), a_not_null(false)) \
  _(16,3,string,tinytext,({}), a_key(10)) \
  _(16,4,string,tinytext,({}), a_key(10), a_not_null(true)) \
  _(17,0,string,mediumtext,({})) \
  _(17,1,string,mediumtext,({}), a_not_null(true)) \
  _(17,2,string,mediumtext,({}), a_not_null(false)) \
  _(17,3,string,mediumtext,({}), a_key(10)) \
  _(17,4,string,mediumtext,({}), a_key(10), a_not_null(true)) \
  _(18,0,string,longtext,({})) \
  _(18,1,string,longtext,({}), a_not_null(true)) \
  _(18,2,string,longtext,({}), a_not_null(false)) \
  _(18,3,string,longtext,({}), a_key(10)) \
  _(18,4,string,longtext,({}), a_key(10), a_not_null(true)) \
  _(19,0,string,binary,({})) \
  _(19,1,string,binary,({}), a_not_null(true)) \
  _(19,2,string,binary,({}), a_not_null(false)) \
  _(19,3,string,binary,({0})) \
  _(19,4,string,binary,({0}), a_not_null(true)) \
  _(19,5,string,binary,({0}), a_not_null(false)) \
  _(19,6,string,binary,({255})) \
  _(19,7,string,binary,({255}), a_not_null(true)) \
  _(19,8,string,binary,({255}), a_not_null(false)) \
  _(19,9,string,binary,({255}), a_unique({})) \
  _(19,10,string,binary,({255}), a_not_null(true), a_unique({})) \
  _(19,11,string,binary,({255}), a_not_null(false), a_unique({})) \
  _(19,12,string,binary,({255}), a_key({})) \
  _(19,13,string,binary,({255}), a_key(10)) \
  _(19,14,string,binary,({255}), a_key({}), a_not_null(true)) \
  _(19,15,string,binary,({255}), a_key(10), a_not_null(true)) \
  _(19,16,string,binary,({255}), a_key({}), a_unique({})) \
  _(19,17,string,binary,({255}), a_key(10), a_unique({})) \
  _(19,18,string,binary,({255}), a_key({}), a_not_null(true), a_unique({})) \
  _(19,19,string,binary,({255}), a_key(10), a_not_null(true), a_unique({})) \
  _(19,20,string,binary,({60})) \
  _(19,21,string,binary,({60}), a_not_null(true)) \
  _(19,22,string,binary,({60}), a_not_null(false)) \
  _(19,23,string,binary,({60}), a_unique({})) \
  _(19,24,string,binary,({60}), a_not_null(true), a_unique({})) \
  _(19,25,string,binary,({60}), a_not_null(false), a_unique({})) \
  _(19,26,string,binary,({60}), a_key({})) \
  _(19,27,string,binary,({60}), a_key(10)) \
  _(19,28,string,binary,({60}), a_key({}), a_not_null(true)) \
  _(19,29,string,binary,({60}), a_key(10), a_not_null(true)) \
  _(19,30,string,binary,({60}), a_key({}), a_unique({})) \
  _(19,31,string,binary,({60}), a_key(10), a_unique({})) \
  _(19,32,string,binary,({60}), a_key({}), a_not_null(true), a_unique({})) \
  _(19,33,string,binary,({60}), a_key(10), a_not_null(true), a_unique({})) \
  _(20,0,string,varbinary,({0})) \
  _(20,1,string,varbinary,({0}), a_not_null(true)) \
  _(20,2,string,varbinary,({0}), a_not_null(false)) \
  _(20,3,string,varbinary,({65523})) \
  _(20,4,string,varbinary,({65523}), a_not_null(true)) \
  _(20,5,string,varbinary,({65523}), a_not_null(false)) \
  _(20,6,string,varbinary,({65523}), a_key(10)) \
  _(20,7,string,varbinary,({65523}), a_key(10), a_not_null(true)) \
  _(20,8,string,varbinary,({60})) \
  _(20,9,string,varbinary,({60}), a_not_null(true)) \
  _(20,10,string,varbinary,({60}), a_not_null(false)) \
  _(20,11,string,varbinary,({60}), a_unique({})) \
  _(20,12,string,varbinary,({60}), a_not_null(true), a_unique({})) \
  _(20,13,string,varbinary,({60}), a_not_null(false), a_unique({})) \
  _(20,14,string,varbinary,({60}), a_key({})) \
  _(20,15,string,varbinary,({60}), a_key(10)) \
  _(20,16,string,varbinary,({60}), a_key({}), a_not_null(true)) \
  _(20,17,string,varbinary,({60}), a_key(10), a_not_null(true)) \
  _(20,18,string,varbinary,({60}), a_key({}), a_unique({})) \
  _(20,19,string,varbinary,({60}), a_key(10), a_unique({})) \
  _(20,20,string,varbinary,({60}), a_key({}), a_not_null(true), a_unique({})) \
  _(20,21,string,varbinary,({60}), a_key(10), a_not_null(true), a_unique({})) \
  _(21,0,string,blob,({})) \
  _(21,1,string,blob,({}), a_not_null(true)) \
  _(21,2,string,blob,({}), a_not_null(false)) \
  _(21,3,string,blob,({0})) \
  _(21,4,string,blob,({0}), a_not_null(true)) \
  _(21,5,string,blob,({0}), a_not_null(false)) \
  _(21,6,string,blob,({0})) \
  _(21,7,string,blob,({0}), a_not_null(true)) \
  _(21,8,string,blob,({0}), a_not_null(false)) \
  _(21,9,string,blob,({65000})) \
  _(21,10,string,blob,({65000}), a_not_null(true)) \
  _(21,11,string,blob,({65000}), a_not_null(false)) \
  _(21,12,string,blob,({65000}), a_key(10)) \
  _(21,13,string,blob,({65000}), a_key(10), a_not_null(true)) \
  _(21,14,string,blob,({16000000})) \
  _(21,15,string,blob,({16000000}), a_not_null(true)) \
  _(21,16,string,blob,({16000000}), a_not_null(false)) \
  _(21,17,string,blob,({16000000}), a_key(10)) \
  _(21,18,string,blob,({16000000}), a_key(10), a_not_null(true)) \
  _(21,19,string,blob,({4000000000})) \
  _(21,20,string,blob,({4000000000}), a_not_null(true)) \
  _(21,21,string,blob,({4000000000}), a_not_null(false)) \
  _(21,22,string,blob,({4000000000}), a_key(10)) \
  _(21,23,string,blob,({4000000000}), a_key(10), a_not_null(true)) \
  _(21,24,string,blob,({60})) \
  _(21,25,string,blob,({60}), a_not_null(true)) \
  _(21,26,string,blob,({60}), a_not_null(false)) \
  _(21,27,string,blob,({60}), a_key(10)) \
  _(21,28,string,blob,({60}), a_key(10), a_not_null(true)) \
  _(22,0,string,tinyblob,({})) \
  _(22,1,string,tinyblob,({}), a_not_null(true)) \
  _(22,2,string,tinyblob,({}), a_not_null(false)) \
  _(22,3,string,tinyblob,({}), a_key(10)) \
  _(22,4,string,tinyblob,({}), a_key(10), a_not_null(true)) \
  _(23,0,string,mediumblob,({})) \
  _(23,1,string,mediumblob,({}), a_not_null(true)) \
  _(23,2,string,mediumblob,({}), a_not_null(false)) \
  _(23,3,string,mediumblob,({}), a_key(10)) \
  _(23,4,string,mediumblob,({}), a_key(10), a_not_null(true)) \
  _(24,0,string,longblob,({})) \
  _(24,1,string,longblob,({}), a_not_null(true)) \
  _(24,2,string,longblob,({}), a_not_null(false)) \
  _(24,3,string,longblob,({}), a_key(10)) \
  _(24,4,string,longblob,({}), a_key(10), a_not_null(true)) \
  _(25,0,string,set,({"foo","bar","baz"})) \
  _(25,1,string,set,({"foo","bar","baz"}), a_not_null(true)) \
  _(25,2,string,set,({"foo","bar","baz"}), a_not_null(false)) \
  _(25,3,string,set,({"foo","bar","baz"}), a_key({})) \
  _(25,4,string,set,({"foo","bar","baz"}), a_key({}), a_not_null(true)) \
  _(26,0,string,enum,({"foo","bar","baz"})) \
  _(26,1,string,enum,({"foo","bar","baz"}), a_not_null(true)) \
  _(26,2,string,enum,({"foo","bar","baz"}), a_not_null(false)) \
  _(26,3,string,enum,({"foo","bar","baz"}), a_key({})) \
  _(26,4,string,enum,({"foo","bar","baz"}), a_key({}), a_not_null(true)) \

#define SAMPLE_LIST_SPATIAL(_) \
  _(27,0,spatial,geometry,({})) \
  _(27,1,spatial,geometry,({}), a_not_null(true)) \
  _(27,2,spatial,geometry,({}), a_not_null(false)) \
  _(27,3,spatial,geometry,({}), a_key({}), a_not_null(true)) \
  _(28,0,spatial,geometrycollection,({})) \
  _(28,1,spatial,geometrycollection,({}), a_not_null(true)) \
  _(28,2,spatial,geometrycollection,({}), a_not_null(false)) \
  _(28,3,spatial,geometrycollection,({}), a_key({}), a_not_null(true)) \
  _(29,0,spatial,point,({})) \
  _(29,1,spatial,point,({}), a_not_null(true)) \
  _(29,2,spatial,point,({}), a_not_null(false)) \
  _(29,3,spatial,point,({}), a_key({}), a_not_null(true)) \
  _(30,0,spatial,multipoint,({})) \
  _(30,1,spatial,multipoint,({}), a_not_null(true)) \
  _(30,2,spatial,multipoint,({}), a_not_null(false)) \
  _(30,3,spatial,multipoint,({}), a_key({}), a_not_null(true)) \
  _(31,0,spatial,linestring,({})) \
  _(31,1,spatial,linestring,({}), a_not_null(true)) \
  _(31,2,spatial,linestring,({}), a_not_null(false)) \
  _(31,3,spatial,linestring,({}), a_key({}), a_not_null(true)) \
  _(32,0,spatial,multilinestring,({})) \
  _(32,1,spatial,multilinestring,({}), a_not_null(true)) \
  _(32,2,spatial,multilinestring,({}), a_not_null(false)) \
  _(32,3,spatial,multilinestring,({}), a_key({}), a_not_null(true)) \
  _(33,0,spatial,polygon,({})) \
  _(33,1,spatial,polygon,({}), a_not_null(true)) \
  _(33,2,spatial,polygon,({}), a_not_null(false)) \
  _(33,3,spatial,polygon,({}), a_key({}), a_not_null(true)) \
  _(34,0,spatial,multipolygon,({})) \
  _(34,1,spatial,multipolygon,({}), a_not_null(true)) \
  _(34,2,spatial,multipolygon,({}), a_not_null(false)) \
  _(34,3,spatial,multipolygon,({}), a_key({}), a_not_null(true)) \

#define SAMPLE_LIST_OTHER(_) \
  _(35,0,other,json,({})) \
  _(35,1,other,json,({}), a_not_null(true)) \
  _(35,2,other,json,({}), a_not_null(false)) \
  _(36,0,other,bit,({})) \
  _(36,1,other,bit,({}), a_not_null(true)) \
  _(36,2,other,bit,({}), a_not_null(false)) \
  _(36,3,other,bit,({}), a_unique({})) \
  _(36,4,other,bit,({}), a_not_null(true), a_unique({})) \
  _(36,5,other,bit,({}), a_not_null(false), a_unique({})) \
  _(36,6,other,bit,({}), a_key({})) \
  _(36,7,other,bit,({}), a_key({}), a_not_null(true)) \
  _(36,8,other,bit,({}), a_key({}), a_unique({})) \
  _(36,9,other,bit,({}), a_key({}), a_not_null(true), a_unique({})) \
  _(36,10,other,bit,({1})) \
  _(36,11,other,bit,({1}), a_not_null(true)) \
  _(36,12,other,bit,({1}), a_not_null(false)) \
  _(36,13,other,bit,({1}), a_unique({})) \
  _(36,14,other,bit,({1}), a_not_null(true), a_unique({})) \
  _(36,15,other,bit,({1}), a_not_null(false), a_unique({})) \
  _(36,16,other,bit,({1}), a_key({})) \
  _(36,17,other,bit,({1}), a_key({}), a_not_null(true)) \
  _(36,18,other,bit,({1}), a_key({}), a_unique({})) \
  _(36,19,other,bit,({1}), a_key({}), a_not_null(true), a_unique({})) \
  _(36,20,other,bit,({64})) \
  _(36,21,other,bit,({64}), a_not_null(true)) \
  _(36,22,other,bit,({64}), a_not_null(false)) \
  _(36,23,other,bit,({64}), a_unique({})) \
  _(36,24,other,bit,({64}), a_not_null(true), a_unique({})) \
  _(36,25,other,bit,({64}), a_not_null(false), a_unique({})) \
  _(36,26,other,bit,({64}), a_key({})) \
  _(36,27,other,bit,({64}), a_key({}), a_not_null(true)) \
  _(36,28,other,bit,({64}), a_key({}), a_unique({})) \
  _(36,29,other,bit,({64}), a_key({}), a_not_null(true), a_unique({})) \
  _(36,30,other,bit,({10})) \
  _(36,31,other,bit,({10}), a_not_null(true)) \
  _(36,32,other,bit,({10}), a_not_null(false)) \
  _(36,33,other,bit,({10}), a_unique({})) \
  _(36,34,other,bit,({10}), a_not_null(true), a_unique({})) \
  _(36,35,other,bit,({10}), a_not_null(false), a_unique({})) \
  _(36,36,other,bit,({10}), a_key({})) \
  _(36,37,other,bit,({10}), a_key({}), a_not_null(true)) \
  _(36,38,other,bit,({10}), a_key({}), a_unique({})) \
  _(36,39,other,bit,({10}), a_key({}), a_not_null(true), a_unique({})) \

