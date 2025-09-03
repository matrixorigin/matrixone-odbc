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

#ifndef ODBC_META_DATA_ODBC_H
#define ODBC_META_DATA_ODBC_H

/*
  This header encodes meta-data expectations given by ODBC specifications
  [1,2]. As such it encodes our understanding of the specifications in that
  respect. It is based on AI interpretation of the spec documents.

  The meta-data expectations are divided into two groups. General expectations
  that are not specific to any type are checked by function
  `check_ti_consistency()` and friends.

  Type specific meta-data expectations are given by specializations
  of `type_traits<T>` template and in particular the static `check_md()`
  function there.

  ODBC Specifications used:

  [1] ISO / IEC 9075-3 — SQL:2023 (and identical wording in the 2016 and 2011 editions) “Information technology — Database languages — SQL — Part 3: Call-Level Interface (CLI)” https://www.iso.org/standard/76586.html (https://www.wiscorp.com/sql/standard/SC32-WG3-N0875_CD_9075-3_(SQL-CLI)_Final.pdf)

  [2] ODBC 4.0 Core Specification (draft, Microsoft / ISO SC32 WG3 liaison, 2023) https://learn.microsoft.com/en-us/sql/connect/odbc/download-odbc-4-specification

*/

#include "odbc_util.h"
#include "meta_data.h"


/*
  # Generic ODBC meta-data consistency checks
  ========================================

  These checks do not depend on the type of the data. They check consistency
  between meta-data obtained from different sources.
*/


namespace odbc
{
  /*
    Check consistency of the general type information `ti` as reported
    by SQLGetTypeInfo() with table column meta-data `col` (from SQLColumns())
    assuming they correspond to each other (i.e, the type info entry is for
    the type of the column described by `col`).
  */

  bool check_ti_consistency(
    Type_info::entry_t const &ti,
    Columns::entry_t const &col
  )
  {
    bool pass = true;

    /*
      Check that non-optional field X has the same value in type info and
      column info.
    */

    #undef  CHECK
    #define CHECK(T,X)  \
      CHECK_##T(ti.X, col.X, "table column meta-data inconsistency: " #X, "tyi:", "col:")

    /*
      Check that field X is present in both type info and column info or
      in none of them.
    */

    #undef  CHECK_OPT_PRESENT
    #define CHECK_OPT_PRESENT(X) \
      if (!check_false(ti.X.has_value() && !col.X.has_value(), "table column meta-data inconsistency: ?" #X, "present in type info but not in table column meta-data")) \
      { pass = false; odbc::print("-- value:", *ti.X); } \
      if (!check_false(!ti.X.has_value() && col.X.has_value(), "table column meta-data inconsistency: ?" #X, "present in table column meta-data but not in type info")) \
      { pass = false; odbc::print("-- value:", *col.X); }

    /*
      Check that optional field X has the same value in type info and column
      info if present or is not present in both.
    */

    #undef  CHECK_OPT
    #define CHECK_OPT(T,X) CHECK_OPT_PRESENT(X) \
      if (ti.X.has_value() && col.X.has_value()) \
        CHECK_##T(*ti.X, *col.X, "table column meta-data inconsistency: ?" #X, "tyi:", "col:")

    /*
      Note: Here we check the fields that are common for `SQLTypeInfo()`
      and `SQLColumns()` records.
    */

    CHECK(val, DATA_TYPE);
    CHECK(val, SQL_DATA_TYPE);
    CHECK_OPT(val, SQL_DATETIME_SUB);
    CHECK_OPT(num, NUM_PREC_RADIX);

    /*
      For COLUMN_SIZE type info gives the maximum possible value while column
      meta data gives the actual value.
    */

    CHECK_OPT_PRESENT(COLUMN_SIZE)
    if (ti.COLUMN_SIZE.has_value() && col.COLUMN_SIZE.has_value())
    {
      pass = check_true(*col.COLUMN_SIZE <= *ti.COLUMN_SIZE,
        "table column size", *col.COLUMN_SIZE,
        "bigger than the maximum from type info:", *ti.COLUMN_SIZE
      ) && pass;
    }

    /*
      Values for NULLABLE attribute: NULLABLE, NO_NULLS, NULLABLE_UNKNOWN

      The only possible inconsistency for NULLABLE attribute is that type info
      specifies the type nullability as NO_NULLS while table column of that
      type is NULLABLE.

      If table info does not specify type nullability or specifies it
      as NULLABLE the table column of that type can be either nullable or not
      (the latter if NOT NULL attribute was specified for the column).

      Note: The NULLABLE field in type info record is optional, but not
      in `SQLColumns()` one.
    */

    if (ti.NULLABLE.has_value())
      pass = check_false(
        nullable_enum::v_no_nulls == *ti.NULLABLE
        && nullable_enum::v_nullable == col.NULLABLE,
        "table column declared as NULLABLE"
        " while its type is declared as NO_NULLS by type info"
      ) && pass;

    if (ti.MINIMUM_SCALE && col.DECIMAL_DIGITS)
      pass = check_true(*col.DECIMAL_DIGITS >= *ti.MINIMUM_SCALE,
        "table column meta-data inconsistency: table column decimal digits",
        *col.DECIMAL_DIGITS, "lower than the minimum from type info",
        *ti.MINIMUM_SCALE
      ) && pass;

    if (ti.MAXIMUM_SCALE && col.DECIMAL_DIGITS)
      pass = check_true(*col.DECIMAL_DIGITS <= *ti.MAXIMUM_SCALE,
        "table column meta-data inconsistency: table column decimal digits",
        *col.DECIMAL_DIGITS, "bigger than the maximum from type info",
        *ti.MAXIMUM_SCALE
      ) && pass;

    return pass;
  }


  /*
    Note: Not much happens in this function right now but later, when we also
    cover result set meta-data, there will be more consistency checks to do.
  */

  bool check_md_consistency(Columns::entry_t const &col)
  {
    bool pass = true;

    /*
      Consistency of size information:

      - If CHAR_OCTET_LENGTH is NOT NULL, then BUFFER_LENGTH = CHAR_OCTET_LENGTH
      - For every row: (COLUMN_SIZE is NULL) => (BUFFER_LENGTH is NULL)
    */

    if (col.CHAR_OCTET_LENGTH.has_value())
    {
      if (check_true(col.BUFFER_LENGTH.has_value(), "BUFFER_LENGTH not defined for table column with CHAR_OCTET_LENGTH:", *col.CHAR_OCTET_LENGTH))
      {
        CHECK_num(*col.BUFFER_LENGTH, *col.CHAR_OCTET_LENGTH,
          "buffer length differs from octet length for table column",
          "BUFFER_LENGTH:", "CHAR_OCTET_LENGTH"
        );
      }
      else
        pass = false;
    }

    if (!col.COLUMN_SIZE.has_value())
    {
      pass = check_false(col.BUFFER_LENGTH.has_value(),
        "table column COLUMN_SIZE not defined but BUFFER_LENGTH is present:",
        *col.BUFFER_LENGTH
      ) && pass;
    }

    // Check consistency of NULLABLE/IS_NULLABLE column info.

    if (col.IS_NULLABLE.has_value())
    {
      /*
        Check that IS_NULLABLE value corresponds to NULLABLE one
        -- they should both present the same information.
      */

      auto v = nullable_enum::v_unknown;
      if ("YES" == *col.IS_NULLABLE)  v = nullable_enum::v_nullable;
      if ("NO" == *col.IS_NULLABLE)   v = nullable_enum::v_no_nulls;

      pass = check_val(v, col.NULLABLE,
        "inconsistent column nullability information"
        ", IS_NULLABLE:", *col.IS_NULLABLE,
        ", NULLABLE:", col.NULLABLE
      ) && pass;
    }

    return pass;
  }

}  // odbc



/*
  # ODBC type specific meta-data checks
  ========================================

  Structure `odbc::type_traits<T>` describes ODBC type  T  with its parameters
  (if any) and defines method `check_md()` which checks if meta-data reported
  for a column of that type is as expected.

  The structure defines type `param_t` to represent type parameters. If given
  type does not have any parameters then `param_t` is defined as `unit_t` type
  which has no values.

  Notes on implementation of `odbc::type_traits<T>` specializations.

  Example inheritance hierarchy
  ```
    type_traits<t_decimal>
    type_traits_base<t_decimal, numeric>
    type_traits_numeric_base<t_decimal>
    type_traits_common_base<t_decimal>
  ```

  The `check_md()` method defined in the common base can be extended in derived
  classes by adding checks and can be also customized by overriding virtual
  methods used in its implementation.

  The base class `type_traits_common_base<T>` implements checks that are common
  for all types via customizable virtual methods such as `check_type()`
  and `check_size()`. The derived `type_traits_numeric_base<T>` adds additional
  checks for meta-data bits that are relevant only for numeric types.
  The `type_traits_base<t_decimal, numeric>` customizes virtual method
  `check_digits()` defined by `*_numeric_base<>` and others to add checks that
  are valid only for the DECIMAL type that has additional scale/precision
  parameters. The detour from `type_traits<T>` to `type_traits_base<T,G>`
  is mainly to add type group parameter to the template (but can also be
  an additional customization point if needed).
*/

namespace odbc
{
  template <type_enum> struct type_traits_common_base;
  template <type_enum, type_group_enum> struct type_traits_base;


  template <type_enum T> struct type_traits
  : type_traits_base<T, type_group(T)>
  {
    using Base = type_traits_base<T, type_group(T)>;
    using param_t = typename Base::param_t;
    using Base::Base;
  };


  /*
    This class is for use by type_traits_base<T,G> specializations and defines
    checks that are common for most types. Specializations can override
    or extend these checks as appropriate.
  */

  template <type_enum T>
  struct type_traits_common_base
  {
    /*
      Phony `param_t` to be used by types which do not have any
      parameters.

      Note: The monostate constructor can be used to construct
      an instance as `pp({})`.
    */

    using param_t = unit_t;

    type_traits_common_base(param_t) {}
    type_traits_common_base() {}


    bool check_md(col_t const &col)
    {
      bool pass = true;

      /*
        Checks below correspond to the main groups of column meta-data
        information reported by SQLColumns():

        - type information

          DATA_TYPE,          enum(type)  (not_null)
          SQL_DATA_TYPE,      enum(type)  (not_null)
          SQL_DATETIME_SUB,   enum(datetime_interval_code)  (null)

        - size information

          COLUMN_SIZE,        integer   (null)
          BUFFER_LENGTH,      integer   (null)
          CHAR_OCTET_LENGTH,  integer   (null)

        - numeric precision

          DECIMAL_DIGITS,     smallint  (null)
          NUM_PREC_RADIX,     smallint  (null)

        Other meta-data groups that are checked elsewhere:

        - other

          NULLABLE,           enum(nullable)  (not_null)
          IS_NULLABLE,        string    (null)
          REMARKS,            string    (null)
          COLUMN_DEF,         string    (null)
          ORDINAL_POSITION,   integer   (not_null)

        - names

          TABLE_CAT,          string  (null)
          TABLE_SCHEM,        string  (null)
          TABLE_NAME,         string  (not_null)
          COLUMN_NAME,        string  (not_null)
          TYPE_NAME,          string  (not_null)
      */

      pass = check_type(col) && pass;
      pass = check_size(col) && pass;
      pass = check_precision(col) && pass;

      return pass;
    }


    virtual bool check_type(col_t const& col)
    {
      bool pass = true;

      pass = check_val(T, col.DATA_TYPE, "table column DATA_TYPE") && pass;

      /*
        Note: DATA_TYPE differs from SQL_DATA_TYPE only for datetime/interval
        types.
      */

      CHECK_val(col.DATA_TYPE, col.SQL_DATA_TYPE,
        "table column DATA_TYPE and SQL_DATA_TYPE differ",
        "DATA_TYPE:", "SQL_DATA_TYPE:"
      );

      /*
        Note: SQL_DATETIME_SUB applies only to datetime/interval types and
        for other types it should not be defined.
      */

      pass = check_false(col.SQL_DATETIME_SUB.has_value(),
        "SQL_DATETIME_SUB defined for table column of non-temporal type:",
        *col.SQL_DATETIME_SUB
      ) && pass;

      return pass;
    }


    virtual bool check_size(col_t const& col)
    {
      bool pass = true;

      /*
        Note: Even though not strictly required by the standard we always
        provide column size and buffer length information.

        The expected values are given by virtual methods size() and buf_size()
        to be overriden by specializations.
      */

      if (check_true(
        col.COLUMN_SIZE.has_value(), "table column COLUMN_SIZE not defined"
      ))
      {
        pass =
          check_num(size(), *col.COLUMN_SIZE, "table column COLUMN_SIZE")
          && pass;
      }
      else
        pass = false;

      if (check_true(
        col.BUFFER_LENGTH.has_value(),
        "table column BUFFER_LENGTH not defined"
      ))
      {
        pass =
          check_num(
            buf_size(), *col.BUFFER_LENGTH, "table column BUFFER_LENGTH"
          ) && pass;
      }
      else
        pass = false;

      pass = check_octet_length(col) && pass;

      return pass;
    }


    /*
      By default we do not expect octet length to be reported (overriden
      for types for which it is not the case).
    */

    virtual bool check_octet_length(col_t const& col)
    {
      bool pass = true;

      CHECK_NOT_EXPECTED(
        col.CHAR_OCTET_LENGTH, "table column CHAR_OCTET_LENGTH"
      );

      return pass;
    }


    /*
      By default we do not expect precision to be reported (overriden for types
      for which it is not the case).
    */

    virtual bool check_precision(col_t const& col)
    {
      bool pass = true;

      CHECK_NOT_EXPECTED(col.DECIMAL_DIGITS, "table column DECIMAL_DIGITS");
      CHECK_NOT_EXPECTED(col.NUM_PREC_RADIX, "table column NUM_PREC_RADIX");

      return pass;
    }


    virtual ulen_t size() const = 0;

    virtual ulen_t buf_size() const
    {
      if (type_group_enum::binary == type_group(T))
        return size();
      else
      {
        /*
          Note: For most types size() is the size of the default character
          representation of a value. Here we return size of a buffer that
          is needed to store that character representation.

          This is overriden by types that need something else (e.g., numeric
          types).
        */

        return char_to_octet_len(size());
      }
    }
  };


  /*
    ## String and binary types
    -------------------------------------------------------------------------
  */

  // Tell if given ODBC string type has the length parameter.

  constexpr bool has_param(type_enum t)
  {
    switch (t)
    {
      /*
        ODBC string types without parameter: LONGVARCHAR/WVARCHAR/VARBINARY.
        Remaining ODBC string types have one: CHAR/WCHAR/BINARY,
        VARCHAR/WVARCHAR/VARBINARY.

        Note: In MySQL string/binary types the size parameter is compulsory
        only for VARCHAR/VARBINARY. For CHAR/BINARY and TEXT/BLOB it
        is optional (defaults to 1). Not used for TINY/MEDIUM/LONGXXX types.
      */

    case type_enum::t_longvarchar:
    case type_enum::t_wlongvarchar:
    case type_enum::t_longvarbinary:
      return false;

    default:
      return true;
    }
  }


  /*
    Template parameter P determines whether the string type has length
    parameter or not. The default template definition is for types without
    length parameter. Specialization for types with parameter is given below.

    Note: For string types without explicit length, the size is the maximum
    possible length (which is determined by the MySQL type of the column).
    Specializations should override size() accordingly.
  */

  template <type_enum T, bool P = has_param(T)>
  struct type_traits_string_base
  : type_traits_common_base<T>
  {
    using Base = type_traits_common_base<T>;
    using Base::Base;

    using Base::buf_size;

    /*
      Note: String/binary types are the only ones for which CHAR_OCTET_LENGTH
      is compulsory.
    */

    bool check_octet_length(col_t const& col) override
    {
      bool pass = true;

      if (check_true(
        col.CHAR_OCTET_LENGTH.has_value(),
        "CHAR_OCTET_LENGTH not specified for table column of string/binary type"
      ))
      {
        pass =
          check_num(
            buf_size(), *col.CHAR_OCTET_LENGTH, "table column CHAR_OCTET_LENGTH"
          ) && pass;
      }
      else
        pass = false;

      return pass;
    }
  };

  // Specialization with length parameter.

  template <type_enum T>
  struct type_traits_string_base<T, true>
  : type_traits_string_base<T, false>
  {
    using Base = type_traits_string_base<T, false>;
    using param_t = ulen_t;

    param_t len;

    type_traits_string_base(param_t len)
    : len{len}
    {}

    ulen_t size() const override
    {
      return len;
    }
  };


  template <type_enum T>
  struct type_traits_base<T, type_group_enum::string>
  : type_traits_string_base<T>
  {
    using Base = type_traits_string_base<T>;
    using Base::Base;
  };

  template <type_enum T>
  struct type_traits_base<T, type_group_enum::binary>
  : type_traits_string_base<T>
  {
    using Base = type_traits_string_base<T>;
    using Base::Base;
  };


  /*
    ## Numeric types
    -------------------------------------------------------------------------
  */

  /*
    Fixed precision and precision radix values for numeric types as specified
    by the standard.
  */

  template <type_enum T>
  struct num_traits
  {
    // Note: Precision is always given in decimal digits.

    static ulen_t precision()
    {
      switch (T)
      {
        case type_enum::t_tinyint:  return 3;
        case type_enum::t_smallint: return 5;
        case type_enum::t_integer:  return 10;
        case type_enum::t_bigint:   return 19;

        case type_enum::t_real:     return 7;
        case type_enum::t_double:   return 15;

        /*
          Note: Precision of FLOAT type is implementation-defined. We do not
          use that type but map FLOAT(p) columns to either REAL or DOUBLE
          depending on precision p.
        */

        default:
          ::print("num_traits::precision() called for invalid type:", T);
          assert(false); // Should not be called for other numeric types
          return 0;      // Keep compiler happy
      }
    }

    static ulen_t octet_length()
    {
      switch (T)
      {
        case type_enum::t_tinyint:  return 1;
        case type_enum::t_smallint: return 2;
        case type_enum::t_integer:  return 4;
        case type_enum::t_bigint:   return 8;

        case type_enum::t_real:     return 4;
        case type_enum::t_double:   return 8;

        default:
          ::print("num_traits::octet_length() called for invalid type:", T);
          assert(false); // Should not be called for other numeric types
          return 0;      // Keep compiler happy
      }
    }

    static ulen_t radix()
    {
      switch (T)
      {
        case type_enum::t_real:
        case type_enum::t_float:
        case type_enum::t_double:
          return 2;

        default:
          return 10;
      }
    }
  };


  template <type_enum T>
  struct type_traits_numeric_base
  : type_traits_common_base<T>
  {
    using Base = type_traits_common_base<T>;
    using param_t = bool;

    param_t us;

    type_traits_numeric_base(param_t us)
    : us{us}
    {}

    bool check_precision(col_t const &col) override
    {
      bool pass = true;

      switch(T)
      {
        // Note: DECIMAL_DIGITS should be NULL for the approximate types

        case type_enum::t_real:
        case type_enum::t_float:
        case type_enum::t_double:

          CHECK_NOT_EXPECTED(col.DECIMAL_DIGITS, "table column DECIMAL_DIGITS");
          break;

        default:

          if (check_true(
            col.DECIMAL_DIGITS.has_value(),
            "table column DECIMAL_DIGITS is not defined"
          ))
          {
            pass =
              check_num(
                scale(), *col.DECIMAL_DIGITS, "table column DECIMAL_DIGITS"
              ) && pass;

            // Additional consistency check

            if (col.COLUMN_SIZE.has_value())
              pass = check_true(*col.DECIMAL_DIGITS <= *col.COLUMN_SIZE,
                "there can not be more decimal digits than precision, "
                "DECIMAL_DIGITS:", *col.DECIMAL_DIGITS,
                "COLUMN_SIZE:", *col.COLUMN_SIZE
              ) && pass;
          }
          else
            pass = false;

      }

      // Note: precision radix should be reported for all numeric types

      if (check_true(
        col.NUM_PREC_RADIX.has_value(),
        "table column NUM_PREC_RADIX is not defined"
      ))
      {
        pass =
          check_num(
            num_traits<T>::radix(), *col.NUM_PREC_RADIX,
            "table column NUM_PREC_RADIX"
          ) && pass;
      }
      else
        pass = false;

      return pass;
    }


    bool is_unsigned() const
    {
      return us;
    }

    ulen_t size() const override
    {
      return num_traits<T>::precision();
    }

    ulen_t buf_size() const override
    {
      return num_traits<T>::octet_length();
    }

    /*
      By default we expect the scale to be 0 (this is the case for all integer
      types).
    */

    virtual ulen_t scale() const
    {
      return 0;
    }

    using Base::Base;

   protected:

    // Note: For derived classes who want to ignore the common parameter
    type_traits_numeric_base()
    : us{false}
    {}
  };


  template <type_enum T>
  struct type_traits_base<T, type_group_enum::numeric>
  : type_traits_numeric_base<T>
  {
    using Base = type_traits_numeric_base<T>;
    using Base::Base;
  };


  /*
    Specialization for DECIMAL type checks that reported precision and scale
    agree with type parameters.

    Note: No specialization for NUMERIC type because we are not using it
    to represent MySQL types.
  */

  template <>
  struct type_traits_base<type_enum::t_decimal, type_group_enum::numeric>
  : type_traits_numeric_base<type_enum::t_decimal>
  {
    using Base = type_traits_numeric_base<type_enum::t_decimal>;

    struct param_t
    {
      unsigned   precision;
      short int  scale;
    };

    param_t pp;

    type_traits_base(param_t pp)
    : pp{pp}
    {}

    ulen_t scale() const override
    {
      return pp.scale;
    }

    ulen_t size() const override
    {
      return pp.precision;
    }

    ulen_t buf_size() const override
    {
      /*
        Following [1] we return size required to store character representation
        of the number, which adds 2 chars for sign and decimal point.

        Note: This is different from other numeric types where buffer size
        is the size of the binary representation.

        [1] https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/transfer-octet-length?view=sql-server-ver17
      */

      return pp.precision ? char_to_octet_len(2 + pp.precision) : 0;
    }

  };


  /*
    ## Datetime types
    -------------------------------------------------------------------------
  */

  template <type_enum T>
  struct type_traits_base<T, type_group_enum::datetime>
  : type_traits_common_base<T>
  {
    using Base = type_traits_common_base<T>;

    using param_t = optional<ulen_t>;

    param_t sec_digits;

    type_traits_base(param_t pp)
    : sec_digits{pp}
    {}


    bool check_type(col_t const& col) override
    {
      bool pass = true;

      /*
        Note: Since we are in datetime type group we know that T
        (the "concise type") is one of: `t_type_date`, `t_type_time`
        or `t_type_timestamp`. Since we do not support interval types
        SQL_DATA_TYPE must be DATETIME.
      */

      pass = check_val(T, col.DATA_TYPE, "table column DATA_TYPE") && pass;

      pass = check_val(
        type_enum::t_datetime, col.SQL_DATA_TYPE,
        "table column SQL_DATA_TYPE"
      ) && pass;

      // interval code checks

      if (!check_true(
        col.SQL_DATETIME_SUB.has_value(),
        "DATETIME_SUB not defined for datetime column"
      ))
        return false;

      switch (T)
      {
        case type_enum::t_type_date:
          CHECK_val(interval_code_type_enum::v_date, *col.SQL_DATETIME_SUB,
            "wrong interval code type for TYPE_DATE column",
            "expected:", "SQL_DATETIME_SUB:"
          );
          break;

        case type_enum::t_type_time:
          CHECK_val(interval_code_type_enum::v_time, *col.SQL_DATETIME_SUB,
            "wrong interval code type for TYPE_TIME column",
            "expected:", "SQL_DATETIME_SUB:"
          );
          break;

        case type_enum::t_type_timestamp:
          CHECK_val(interval_code_type_enum::v_timestamp, *col.SQL_DATETIME_SUB,
            "wrong interval code type for TYPE_TIMESTAMP column",
            "expected:", "SQL_DATETIME_SUB:"
          );
          break;

        default:
          ::print(
            "type_traits<datetime>::check_type() called for invalid type:", T
          );
          assert(false); // This should not happen
      }

      return pass;
    }


    bool check_precision(col_t const& col) override
    {
      bool pass = true;

      switch (T)
      {
        case type_enum::t_type_date:
          CHECK_NOT_EXPECTED(col.DECIMAL_DIGITS, "table column DECIMAL_DIGITS");
          break;

        default:
          if (check_true(
            col.DECIMAL_DIGITS.has_value(),
            "table column DECIMAL_DIGITS is not defined"
          ))
          {
            pass =
              check_num(
                scale(), *col.DECIMAL_DIGITS, "table column DECIMAL_DIGITS"
              ) && pass;
          }
          else
            pass = false;
      }

      CHECK_NOT_EXPECTED(col.NUM_PREC_RADIX, "table column NUM_PREC_RADIX");

      return pass;
    }

    /*
      Column size is the number of characters in the literal representation
      defined by the standard:

        SQL_TYPE_DATE 10 (YYYY-MM-DD)
        SQL_TYPE_TIME 8 + scale
        SQL_TYPE_TIMESTAMP 19 + scale

      See also [1] and [2].

      [1] https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size

      [2] https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/display-size
    */

    ulen_t size() const override
    {
      auto add_scale = [this](ulen_t x)
      {
        if (sec_digits.has_value())
          // Note: +1 for decimal point
          return x + 1 + *sec_digits;
        else
          return x;
      };

      switch (T)
      {
        case type_enum::t_type_date:      return 10;
        case type_enum::t_type_time:      return add_scale(8);
        case type_enum::t_type_timestamp: return add_scale(19);
        default:
          ::print("type_traits<datetime>::size() called for invalid type:", T);
          assert(false);  // should not be called for other types
          return 0;       // keep compiler happy
      }
    }

    unsigned scale() const
    {
      return sec_digits.has_value() ? *sec_digits : 0;
    }
  };


  /*
    ## Other types
    -------------------------------------------------------------------------
  */

  template <type_enum T>
  struct type_traits_base<T, type_group_enum::other>
  : type_traits_common_base<T>
  {
    using Base = type_traits_common_base<T>;
    using Base::Base;

    ulen_t size() const override
    {
      switch (T)
      {
        case type_enum::t_bit:  return 1;
        case type_enum::t_guid: return 36;
        default:
          ::print("type_traits<other>::size() called for invalid type:", T);
          assert(false); // should not be called for other types
          return 0;      // keep compiler happy
      }
    }

    ulen_t buf_size() const override
    {
      switch (T)
      {
        case type_enum::t_bit:  return 1;
        case type_enum::t_guid: return 16;
        default:
          ::print("type_traits<other>::buf_size() called for invalid type:", T);
          assert(false); // should not be called for other types
          return 0;      // keep compiler happy
      }
    }
  };


}   // odbc

#endif