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

#ifndef ODBC_META_DATA_H
#define ODBC_META_DATA_H

/*
  This header is used by our unit tests that check meta-data reporting by
  the driver (my_meta_data.cc). It aggregates all auxiliary infrastructure
  needed by these tests to probe database meta-data.

  This infrastructure is also used in "meta_data_odbc.h" header which encodes
  meta-data expectations given by ODBC specifications.

  Note: This header uses "meta_data_constants.h" header which describes
  existing MySQL and ODBC types, available meta-data information bits etc.
*/

/*
  Note: Check that we use modern pre-processor for MSVC
  (the `/Zc:preprocessor` compiler flag).
*/

#if _MSVC_TRADITIONAL
  #error Old preprocessor
#endif

#include "meta_data_constants.h"
#include "odbc_util.h"

#include <cassert>
#include <variant>
#include <map>
#include <optional>


using std::map;
using std::variant;
using std::string;
using std::optional;
using ulen_t = uint64_t;
using unit_t = std::monostate;

using odbc::qstring;

using odbc::check_false;
using odbc::check_true;
using odbc::check_num;
using odbc::check_str;
using odbc::print;

namespace odbc
{
  using std::string;
  using std::optional;
}


/*
  # ODBC core enums & helpers
  ========================================

  Strongly-typed enums for ODBC data types plus small utilities (type grouping,
  exact-numeric test, pretty-printers).
*/

namespace odbc
{

  enum class type_group_enum
  {
    string, numeric, binary, datetime, interval, verbose, other
  };

  enum class type_enum
  {
    #undef ENUM_DEF
    #define ENUM_DEF(G,T,X,...) t_## T = X,
    ODBC_TYPE_LIST(ENUM_DEF)
  };


  constexpr
  type_group_enum type_group(type_enum t)
  {
    switch (t)
    {
      #undef TYPE_GROUP
      #define TYPE_GROUP(G,T,...)  case type_enum::t_##T: return type_group_enum::G;
      ODBC_TYPE_LIST(TYPE_GROUP)
    }
    return type_group_enum::other;
  }


  /*
    Determine if we are running against Unicode driver variant.
  */

  bool is_unicode()
  {
    if (unicode_driver < 0)
      throw odbc::Exception{"Driver Unicode/ANSI variant not known"};
    return unicode_driver > 0;
  }

  /*
    For given number of characters this function returns number of bytes sufficient to store a string with that many characters.
  */

  ulen_t char_to_octet_len(ulen_t n)
  {
    return n * (is_unicode() ? sizeof(SQLWCHAR) : sizeof(SQLCHAR));
  }


  // This tells whether given ODBC type is an exact numeric type.

  constexpr
  bool is_exact(type_enum t)
  {
    switch(t)
    {
      case type_enum::t_decimal:
      case type_enum::t_numeric:
      case type_enum::t_smallint:
      case type_enum::t_integer:
      case type_enum::t_tinyint:
      case type_enum::t_bigint:
        return true;

      // approx numeric types
      case type_enum::t_real:
      case type_enum::t_float:
      case type_enum::t_double:
      default:
        return false;
    }
  }


  template <>
  struct printer<type_enum>
  {
    static
    void print(std::ostream &out, type_enum t)
    {
      switch (t)
      {
        #undef ENUM_PRINT
        #define ENUM_PRINT(G,T,X,...)  case type_enum::t_##T: out << #T; break;
        ODBC_TYPE_LIST(ENUM_PRINT)
      }
    }
  };


  /*
    Macros below define enumerations for attribute values such as
    nullable_enum, searchable_enum etc. The possible enumeration values
    are given by the corresponding list macros defined in
    "meta_data_constants.h", such as NULLABLE_VAL_LIST(). The macros also
    define appropriate printer<> specializations for printing these enumeration
    values.
  */

  #undef ENUM_DEF
  #define ENUM_DEF(V,X,...) v_## V = X,

  #undef DECLARE_ENUM
  #define DECLARE_ENUM(E, LIST) \
    enum class E##_enum {LIST(ENUM_DEF) }; \
    ENUM_PRINTER(E##_enum, LIST)

  #undef ENUM_PRINT
  #define ENUM_PRINT(V,X,...) case type(X): out << #V; break;

  #undef ENUM_PRINTER
  #define ENUM_PRINTER(E,LIST) \
  template <> struct printer<E> \
  { using type = E; \
    static void print(std::ostream &out, type val) \
    { switch (val) { LIST(ENUM_PRINT) \
      default: out << "!! unknown enum id " #E " :" << int(val); } } \
  }; \

  DECLARE_ENUM(nullable, NULLABLE_VAL_LIST)
  DECLARE_ENUM(searchable, NULLABLE_VAL_LIST)
  DECLARE_ENUM(updatable, NULLABLE_VAL_LIST)
  DECLARE_ENUM(unnamed, NULLABLE_VAL_LIST)

  /*
    Note: We add extra enum constant `t_none` to represent value 0 that is used
    when datetime/interval code is not relevant for a type.
  */

  #define INTERVAL_CODE_TYPE_EXT(_) \
    INTERVAL_CODE_TYPE(_) _(none,0)
  #define INTERVAL_CODE_VAL_LIST_EXT(_) \
    INTERVAL_CODE_VAL_LIST(_) _(none,0)

  DECLARE_ENUM(interval_code_type, INTERVAL_CODE_TYPE_EXT)
  DECLARE_ENUM(datetime_interval_code, INTERVAL_CODE_VAL_LIST_EXT)

}  // odbc


/*
  # Low-level ODBC wrappers and traits
  ========================================

  `md_type` plus `md_type_traits` specializations that map meta-data value
  types to  C++ types and handle SQLGetData extraction.
*/

namespace odbc
{
  /*
    ODBC meta-data uses a couple of basic types to represent meta-data
    information. Often this information is given as a result-set returned
    by ODBC meta-data APIs with column types among the ones represented
    by `md_type` constants defined below.
  */

  enum class md_type
  {
    t_string, t_integer, t_ulen, t_len, t_smallint, t_bool, t_enum
  };

  /*
    Meta-data value type traits.

    Usage
    ```
    // Traits for odbc meta-data values of type `bool` that can be null.

    using val_traits = MD_TYPE_TRAITS(bool, null);

    // C++ type to store such values (in this case std::optional<bool>)

    using val_t = val_traits::type;

    // Read value from the 1st column of the given result
    // (assuming the column is of the correct type).

    val_t v = val_traits::get_col(stmt, 1);
    ```

    Meta-data value type specification (T,U) consists of type T (corresponding
    to enum constant `md_type::t_T`) and nullability specifier U which can
    be `null` or not `null`. In case of enumeration values type T is of
    the form `enum(E)` referring to enumeration `E_enum`.

    Examples:

    - (string, null) -- optional string values (can be null)

    - (enum(searchable), not_null)  -- values of enumeration type
                                       `searchable_enum` that can not be null.

    For such meta-data value type specification (T,U) macro MD_TYPE_TRAITS(T,U)
    names a structure which defines the following static members:

    - `type` -- a C++ type suitable for storing values of the specified type;

    - `get_col()`  -- function to read values of the specified type from
                      a resultset column.
  */

  #define MD_TYPE_TRAITS(T,U) \
    detail::md_type_traits<detail::md_opt::U, MDT_##T>

  /*
    Note: These dispatcher macros are used to add enumeration name
    to the template parameter list in case of enum(E) type (for other types
    the enumeration name is skipped)
  */

  #define MDT_enum(E)   md_type::t_enum, E##_enum
  #define MDT_string    md_type::t_string
  #define MDT_ulen      md_type::t_ulen
  #define MDT_len       md_type::t_len
  #define MDT_integer   md_type::t_integer
  #define MDT_smallint  md_type::t_smallint
  #define MDT_bool      md_type::t_bool


  namespace detail
  {
    enum md_opt
    {
      null = true, not_null = false
    };


    /*
      Base for the md_type_traits<> template specializations that define
      how to read values of each type from a result set (get_col() function).
    */

    template <md_type> struct md_type_traits_base;


    template<>
    struct md_type_traits_base<md_type::t_string>
    {
      using type = string;

      static
      optional<type> get_col(SQLHSTMT stmt, size_t col)
      {
        char buf[512] = {0};
        SQLLEN len;

        ok_stmt(stmt, SQLGetData(stmt, col, SQL_CHAR, buf, sizeof(buf), &len));

        if(SQL_NULL_DATA == len)
          return {};
        return string{ buf, (size_t)len };
      }
    };


    template<>
    struct md_type_traits_base<md_type::t_ulen>
    {
      using type = uint64_t;

      static
      optional<type> get_col(SQLHSTMT stmt, size_t col)
      {
        long unsigned int val = 0;
        SQLLEN len;

        ok_stmt(stmt,
          SQLGetData(stmt, col, SQL_C_ULONG, &val, sizeof(val), &len)
        );

        if(SQL_NULL_DATA == len)
          return {};
        return val;
      }
    };

    template<>
    struct md_type_traits_base<md_type::t_len>
    : md_type_traits_base<md_type::t_ulen>
    {};


    template<>
    struct md_type_traits_base<md_type::t_bool>
    {
      using type = bool;

      static
      optional<type> get_col(SQLHSTMT stmt, size_t col)
      {
        unsigned char val = 0;
        SQLLEN len;

        ok_stmt(stmt, SQLGetData(stmt, col, SQL_C_UTINYINT, &val, sizeof(val), &len));

        if(SQL_NULL_DATA == len)
          return {};
        return val;
      }
    };

    template<>
    struct md_type_traits_base<md_type::t_integer>
    {
      using type = int64_t;

      static
      optional<type> get_col(SQLHSTMT stmt, size_t col)
      {
        long int val = 0;
        SQLLEN len;

        ok_stmt(stmt,
          SQLGetData(stmt, col, SQL_C_SLONG, &val, sizeof(val), &len)
        );

        if(SQL_NULL_DATA == len)
          return {};
        return val;
      }
    };

    template<>
    struct md_type_traits_base<md_type::t_smallint>
    {
      using type = short int;

      static
      optional<type> get_col(SQLHSTMT stmt, size_t col)
      {
        short int val = 0;
        SQLLEN len;

        ok_stmt(stmt,
          SQLGetData(stmt, col, SQL_C_SHORT, &val, sizeof(val), &len)
        );

        if(SQL_NULL_DATA == len)
          return {};

        return val;
      }
    };


    /*
      Note: The ENUM parameter is relevant only for specializations with 2nd
      parameter md_type::t_enum and it names the enumeration type to be used.
      For other specializations it is ignored.
    */

    template <md_opt, md_type, typename ENUM = void> struct md_type_traits;


    template <md_type T>
    struct md_type_traits<md_opt::not_null, T, void>
    : md_type_traits_base<T>
    {
      static constexpr bool nullable = false;

      using Base = md_type_traits_base<T>;
      using typename Base::type;

      /*
        Note: The `get_col()` method from the base class returns optional
        value. Here we know that the value must be present and can not be null
        so we can extract it from the optional.
      */

      static type get_col(SQLHSTMT stmt, size_t col)
      {
        auto v = Base::get_col(stmt, col);
        assert(v.has_value());
        return *v;
      }
    };


    template <md_type T, typename ET>
    struct md_type_traits<md_opt::null, T, ET>
    : md_type_traits<md_opt::not_null, T, ET>
    {
      static constexpr bool nullable = true;

      using Base = md_type_traits<md_opt::not_null, T, ET>;
      using type = optional<typename Base::type>;

      /*
        Note: We use the `get_col()` method from the base traits because that
        one can handle null values (it returns optional<> value)
      */

      using md_type_traits_base<T>::get_col;
    };


    /*
      Special case of md_type::t_enum where the C++ type is an enumeration type
      (given as the last template parameter in that case).

      In a result-set such enum values are stored as small integers which here
      are converted to the enum values.
    */

    template <typename ENUM>
    struct md_type_traits<md_opt::null, md_type::t_enum, ENUM>
    : md_type_traits_base<md_type::t_smallint>
    {
      using Base = md_type_traits_base<md_type::t_smallint>;
      using type = optional<ENUM>;

      static type get_col(SQLHSTMT stmt, size_t col)
      {
        auto v = Base::get_col(stmt, col);
        if (!v.has_value())
          return {};
        return ENUM(*v);
      }
    };

    template <typename ENUM>
    struct md_type_traits<md_opt::not_null, md_type::t_enum, ENUM>
    : md_type_traits<md_opt::null, md_type::t_enum, ENUM>
    {
      using Base = md_type_traits<md_opt::null, md_type::t_enum, ENUM>;
      using type = ENUM;

      static type get_col(SQLHSTMT stmt, size_t col)
      {
        auto v = Base::get_col(stmt, col);
        assert(v.has_value());
        return *v;
      }
    };

  } // detail

}  // odbc


/*
  # ODBC meta-data helpers
  ========================================

  Lightweight wrappers around `SQLColumns()` and `SQLGetTypeInfo()`, storing
  the full result in easy-to-iterate C++ structs (`Columns`, `Type_info`).
*/

namespace odbc
{
  /*
    SQLColumns() result set

    Note: SQL_DATETIME_SUB is interpreted as interval type for datetime types
    or as inteval code value for interval types. Here we pick the first
    interpretation because MySQL does not use interval types.

    Note: Using ulen for length columns.

    See: https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function?view=sql-server-ver16
  */

  #define COLUMNS_COLS(_) \
    _(1,  TABLE_CAT,          string,         null) \
    _(2,  TABLE_SCHEM,        string,         null) \
    _(3,  TABLE_NAME,         string,         not_null) \
    _(4,  COLUMN_NAME,        string,         not_null) \
    _(5,  DATA_TYPE,          enum(type),     not_null) \
    _(6,  TYPE_NAME,          string,         not_null) \
    _(7,  COLUMN_SIZE,        ulen,           null) \
    _(8,  BUFFER_LENGTH,      ulen,           null) \
    _(9,  DECIMAL_DIGITS,     smallint,       null) \
    _(10, NUM_PREC_RADIX,     smallint,       null) \
    _(11, NULLABLE,           enum(nullable), not_null) \
    _(12, REMARKS,            string,         null) \
    _(13, COLUMN_DEF,         string,         null) \
    _(14, SQL_DATA_TYPE,      enum(type),     not_null) \
    _(15, SQL_DATETIME_SUB,   enum(interval_code_type), null) \
    _(16, CHAR_OCTET_LENGTH,  ulen,           null) \
    _(17, ORDINAL_POSITION,   ulen,           not_null) \
    _(18, IS_NULLABLE,        string,         null) \


  /*
    A `Columns` object stores meta-data information about table columns
    obtained from `SQLColumns()` API. It fetches the data upon construction.

    Given object `cols` of type `columns` data for column with ordinal position
    N  is returned by `cols[N]`. This is a reference to `col_t` structure that
    has members of appropriate type for each `SQLColumns()` result-set column,
    as specified by `COLUMNS_COLS()` macro. Column information can be also
    inspected using range-for loop.
  */

  struct Columns
  {
    Columns(SQLHSTMT, string tname);

    struct entry_t
    {
      #undef DEF_MEMBER
      #define DEF_MEMBER(X,N,T,U,...) typename MD_TYPE_TRAITS(T,U)::type N;

      COLUMNS_COLS(DEF_MEMBER)

      // Create entry from the current row in SQLColumns() resultset.
      entry_t(SQLHSTMT stmt);
    };

    entry_t const& operator[](size_t pos)
    {
      for (size_t i = 0; i < data.size(); ++i)
        if (pos == data[i].ORDINAL_POSITION)
          return data[i];
      throw std::out_of_range{"Columns[]"};
    }

    using data_t = std::vector<entry_t>;
    using iterator = data_t::const_iterator;

    iterator begin() const { return data.begin(); }
    iterator end() const { return data.end(); }

   private:

    data_t data;
 };

  using col_t = Columns::entry_t;


  Columns::Columns(SQLHSTMT stmt, string tname)
  {
    ok_stmt(stmt, SQLColumns(
      stmt,
      (SQLCHAR*)"test", 4,  // catalog
      nullptr, 0,           // schema
      (SQLCHAR*)tname.c_str(), tname.length(),
      nullptr, 0            // return all columns
    ));

    do
    {
      auto rc = SQLFetch(stmt);
      if (SQL_NO_DATA == rc) break;
      data.emplace_back(stmt);
    }
    while (true);

    SQLCloseCursor(stmt);
  }


  Columns::entry_t::entry_t(SQLHSTMT stmt)
  {
    #undef  COL_GET
    #define COL_GET(N,C,T,U,...) \
      this->C = MD_TYPE_TRAITS(T,U)::get_col(stmt, N);

    COLUMNS_COLS(COL_GET)
  }


  /*
    Columns of a result set returned by GetTypeInfo().

    Note: The enum columns are specified in [1] as smallint values. However,
    from the description it follows that they should be actually
    the enumeration constant values.

    Note: Using ulen for length columns.

    [1] https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgettypeinfo-function?view=sql-server-ver16
  */

  #define TYPE_INFO_COLS(_) \
    _(1,  TYPE_NAME,          string,   not_null) \
    _(2,  DATA_TYPE,          enum(type), not_null) \
    _(3,  COLUMN_SIZE,        ulen,     null) \
    _(4,  LITERAL_PREFIX,     string,   null) \
    _(5,  LITERAL_SUFFIX,     string,   null) \
    _(6,  CREATE_PARAMS,      string,   null) \
    _(7,  NULLABLE,           enum(nullable), null) \
    _(8,  CASE_SENSITIVE,     bool,     null) \
    _(9,  SEARCHABLE,         enum(searchable), not_null) \
    _(10, UNSIGNED_ATTRIBUTE, bool,     null) \
    _(11, FIXED_PREC_SCALE,   smallint, not_null) \
    _(12, AUTO_UNIQUE_VALUE,  smallint, null) \
    _(13, LOCAL_TYPE_NAME,    string,   null) \
    _(14, MINIMUM_SCALE,      smallint, null) \
    _(15, MAXIMUM_SCALE,      smallint, null) \
    _(16, SQL_DATA_TYPE,      enum(type), not_null) \
    _(17, SQL_DATETIME_SUB,   enum(interval_code_type), null) \
    _(18, NUM_PREC_RADIX,     smallint, null) \
    _(19, INTERVAL_PRECISION, smallint, null) \


  /*
    Given statement handle and ODBC type a `Type_info` object fetches
    and stores type information records returned by SQLGetTypeInfo() method.
    One can iterate over these records using range-for loop.

    Single type information record is stored in Type_info::entry_t struct which
    has member variable CCC of appropriate type for each type info column CCC
    declared by TYPE_INFO_COLS() list.
  */

  struct Type_info
  {
    Type_info(SQLHSTMT hstmt, type_enum ty);

    struct entry_t
    {
      #define TI_MEMBER(X,N,T,U,...) typename MD_TYPE_TRAITS(T,U)::type N;

      TYPE_INFO_COLS(TI_MEMBER)

      // Create entry from the current row in SQLGetTypeInfo() resultset.
      entry_t(SQLHSTMT hstmt);
    };

    using data_t = std::vector<entry_t>;
    using iterator = data_t::const_iterator;

    iterator begin() const { return data.begin(); }
    iterator end() const { return data.end(); }

   private:

    data_t data;
  };


  Type_info::Type_info(SQLHSTMT hstmt, type_enum ty)
  {
    ok_stmt(hstmt, SQLGetTypeInfo(hstmt, (SQLSMALLINT)ty));
    SQLSMALLINT n;
    ok_stmt(hstmt, SQLNumResultCols(hstmt, &n));

    do
    {
      auto rc = SQLFetch(hstmt);
      if (SQL_NO_DATA == rc)
        break;
      data.emplace_back(hstmt);
      //print("- got type info for:", data.back().TYPE_NAME);
    }
    while (true);

    ok_stmt(hstmt, SQLCloseCursor(hstmt));
  }

  Type_info::entry_t::entry_t(SQLHSTMT hstmt)
  {
    #define TI_GET(C,N,T,U,...) \
      this->N = MD_TYPE_TRAITS(T,U)::get_col(hstmt, C);

    TYPE_INFO_COLS(TI_GET)
  }

}  // odbc


/*
  # MySQL meta-data support
  ========================================

  MySQL-specific enums (type, type group, column attributes), class for storing
  and manipulating table column attributes (`attr_t`), and a helper for
  extracting information from INFORMATION_SCHEMA.COLUMNS (`IS_Columns`).
*/


namespace mysql
{

  enum class type_group_enum
  {
    string, numeric, temporal, spatial, other
  };

  enum class type_enum
  {
    #undef  ENUM_DEF
    #define ENUM_DEF(G,T,...)  t_##T,
    MYSQL_TYPE_LIST(ENUM_DEF)
  };


  const char* type_name(type_enum t)
  {
    switch (t)
    {
      #undef  ENUM_NAME
      #define ENUM_NAME(G,T,...)  case type_enum::t_##T: return #T;
      MYSQL_TYPE_LIST(ENUM_NAME)
      default:
        throw std::invalid_argument{"type_name: unknown MySQL type"};
    }
  }

  constexpr
  type_group_enum type_group(type_enum t)
  {
    switch (t)
    {
      #undef TYPE_GROUP
      #define TYPE_GROUP(G,T,...) \
        case type_enum::t_##T: return type_group_enum::G;

      MYSQL_TYPE_LIST(TYPE_GROUP)
    }
    return type_group_enum::other;
  }


  /*
    MySQL column attributes
    [1] https://dev.mysql.com/doc/refman/en/create-table.html
    [2] https://dev.mysql.com/doc/refman/en/data-types.html
  */

  enum class coll_attr_enum
  {
    #undef ENUM_DEF
    #define ENUM_DEF(A,...) a_##A,
    MYSQL_ATTR_LIST(ENUM_DEF)
  };

  namespace detail
  {
    /*
      For attribute A its value is represented by C++ type
      `attr_traits<A>::type` as specified by `MYSQL_ATTR_LIST()` macro.

      Note that many attributes are either present or not, but if present then
      they don't have any specific value (for example the unsigned attribute
      for numeric columns). For such attributes we use C++ type `unit_t` (aka
      `std::monostate`) which has no values.
    */

    template <coll_attr_enum> struct attr_traits;

    #undef ATTR_TRAITS
    #define ATTR_TRAITS(A,T,...) \
    template <> struct attr_traits<coll_attr_enum::a_##A> \
    { using type = T; };

    MYSQL_ATTR_LIST(ATTR_TRAITS)

  } // detail

  /*
    Object of type `attr_t` stores attributes of MySQL table column.

    Initially there are no attributes until `set<A>(V)` method is called to set
    attribute A to value V. For attributes without values (that can be present
    or not) use `set<A>()` or `set<A>({})` to set the attribute.
  */

  struct attr_t
  {
    template <coll_attr_enum A>
    void set(typename detail::attr_traits<A>::type val = {})
    {
      data.emplace(A, val_t{std::in_place_index<size_t(A)>, val});
    }

    template <coll_attr_enum A>
    typename detail::attr_traits<A>::type get() const
    {
      return std::get<size_t(A)>(data.at(A));
    }

    bool has(coll_attr_enum a) const
    {
      return 0 < data.count(a);
    }

    /*
      Print attributes using MySQL syntax (to be used in column definitions
      of a TABLE CREATE statement).
    */

    void print(string col, std::ostream &out) const
    {
      if (has(coll_attr_enum::a_unsigned))
      {
        out << (get<coll_attr_enum::a_unsigned>() ? " UNSIGNED" : " SIGNED" );
      }

      if (has(coll_attr_enum::a_not_null))
      {
        if (get<coll_attr_enum::a_not_null>())  out << " NOT";
        out << " NULL";
      }

      if (has(coll_attr_enum::a_auto_increment))
        out << " AUTO_INCREMENT";

      if (has(coll_attr_enum::a_key))
      {
        auto size = get<coll_attr_enum::a_key>();

        /*
          Note: We add a separate "KEY ..." declaration to be able to specify
          key width (it can not be done with "KEY" phrase added to the column
          declaration):
          ```
            c TYPE ..., KEY key_c (c(N))
          ```
          Fortunately column and key declarations can be mixed.
        */

        out << ", ";

        if (has(coll_attr_enum::a_unique))
          out << "UNIQUE ";

        out
          << "KEY key_" << col
          << " (" << col;

        if (size.has_value())
        {
          out << "(" << *size << "))";
        }
        else
        {
          out << ")";
        }
      }
      else if (has(coll_attr_enum::a_unique))
        out << " UNIQUE";

    }

    #define ATTR_TYPE(A,...)  detail::attr_traits<coll_attr_enum::a_##A>::type,
    using val_t = variant<
      MYSQL_ATTR_LIST(ATTR_TYPE) unit_t
    >;

    map<mysql::coll_attr_enum, val_t> data;
  };


  using odbc::md_type;
  namespace detail
  {
    using odbc::detail::md_type_traits;
    using odbc::detail::md_opt;
  }


  /*
    Columns of I_S.COLUMNS table.

    Note: All these columns are nullable.

    [1] https://dev.mysql.com/doc/refman/9.3/en/information-schema-columns-table.html
  */

  #define IS_COLUMNS(_) \
    _(COLUMN_NAME, string) \
    _(DATA_TYPE, string) \
    _(COLUMN_TYPE, string) \
    _(IS_NULLABLE, string) \
    _(CHARACTER_MAXIMUM_LENGTH, ulen) \
    _(CHARACTER_OCTET_LENGTH, ulen) \
    _(NUMERIC_PRECISION, smallint) \
    _(NUMERIC_SCALE, smallint) \
    _(DATETIME_PRECISION, smallint) \
    _(CHARACTER_SET_NAME, string) \
    _(COLLATION_NAME, string) \
    _(COLUMN_KEY, string) \
    _(EXTRA, string) \
    _(PRIVILEGES, string) \

  /*
    Object of type `IS_Columns` fetches and stores information about a table
    column given in `INFORMATION_SCHEMA.COLUMNS` view. It has member variable
    of appropriate type for each column of I_S.COLUMNS view as given
    by the `IS_COLUMNS()` macro. All column values are optional and all columns
    are of optional types.
  */

  struct IS_Columns
  {
    #define COL_VAR(N,T,...) typename MD_TYPE_TRAITS(T,null)::type N;
    IS_COLUMNS(COL_VAR)

    IS_Columns(SQLHSTMT stmt, string schema, string tbl, size_t col)
    {
      string sql = "SELECT ";

      #undef COL_ADD
      #define COL_ADD(C,...)  sql += #C; sql += ", ";
      IS_COLUMNS(COL_ADD)

      // TODO: Bind parameters instead?

      sql += "null FROM INFORMATION_SCHEMA.COLUMNS";
      sql += " WHERE TABLE_SCHEMA = '" + schema + "'";
      sql += " AND TABLE_NAME = '" + tbl + "'";
      sql += " AND ORDINAL_POSITION = " + std::to_string(col);

      odbc::sql(stmt, sql);

      auto rc = SQLFetch(stmt);
      assert(SQL_NO_DATA != rc);
      assert(SQL_IS_SUCCESS(rc));

      #undef  COL_GET
      #define COL_GET(C,T,...) \
        this->C = MD_TYPE_TRAITS(T,null)::get_col(stmt, col_pos::c_##C);

      IS_COLUMNS(COL_GET)

      ok_stmt(stmt, SQLCloseCursor(stmt));
    }

   private:

    /*
      Note: This enum is used to get 1-based position of each column declared
      by IS_COLUMNS() macro.
    */

    enum col_pos
    {
      c_zero = 0,
      #define COLS_ENUM(C,...) c_##C,
      IS_COLUMNS(COLS_ENUM)
    };
  };

}  // mysql


// Printing MySQL type constants

namespace odbc
{
  template <>
  struct printer<mysql::type_enum>
  {
    static
    void print(std::ostream &out, mysql::type_enum t)
    {
      switch (t)
      {
        #undef ENUM_PRINT
        #define ENUM_PRINT(G,T,...)  case mysql::type_enum::t_##T: out << #T; break;
        MYSQL_TYPE_LIST(ENUM_PRINT)
      }
    }
  };
}


/*
  # Generic macro utilities / integrity checks
  ========================================

  Wrappers for check_T() functions that simplify failure reporting
  and accumulate pass/fail state.

  Usage:
  ```
    CHECK_str(eA, eB, "values do not match", "valA:", "valB:");
  ```
  This will call `check_str()` function for expressions `eA` and `eB` setting
  the fail message to:
  ```
    values do not match, valA: {A}, valB: {B}
  ```
  where {A} and {B} are the computed values of the expressions. The result
  of the check is accumulated in Boolean `pass` variable that must be
  in the scope.
*/


#undef  CHECK_
#undef  CHECK_str
#undef  CHECK_num
#undef  CHECK_val
#undef  CHECK_bool

#define CHECK_(T,A,B,N,NA,NB) \
  pass = check_##T(A, B, N, ",", NA, A, ",", NB, B) && pass;
#define CHECK_str(A,B,N,NA,NB) \
  pass = check_str(A, B, N, ",", NA, qstring{A}, ",", NB, qstring{B}) && pass;
#define CHECK_num(A,B,N,NA,NB)  CHECK_(num,A,B,N,NA,NB)
#define CHECK_val(A,B,N,NA,NB)  CHECK_(val,A,B,N,NA,NB)
#define CHECK_bool(A,B,N,NA,NB)  CHECK_(bool,A,B,N,NA,NB)

#undef  CHECK_NOT_EXPECTED
#define CHECK_NOT_EXPECTED(X,M) \
if (!check_false(X.has_value(), M " not expected for this type")) \
{ pass = false; ::print("-- " M ":", *X); }


/*
  # Test table fixture
  ========================================

  `Test_table` helper that creates a throw-away table (name derived from
  the test case) with a single column defined by a `mysql::Col_spec`;
  used by tests to exercise metadata handling.
*/


namespace mysql
{
  enum driver_variant_enum { Unicode, ANSI };
  template <type_enum, driver_variant_enum> struct Col_spec;
}

//  Note: The table is created on object construction.

struct Test_table
: odbc::table
{
  // Note: n-k is the sample number and it determines table name

  template <mysql::type_enum T, mysql::driver_variant_enum D>
  Test_table(SQLHSTMT, size_t n, size_t k, mysql::Col_spec<T,D> const&);

  string name() const
  {
    return odbc::table::table_name;
  }

  size_t col_cnt() const;
  string col_name(size_t) const;

 private:

  static string get_name(size_t, size_t);
  template <mysql::type_enum T, mysql::driver_variant_enum D>
  static string get_col_def(mysql::Col_spec<T,D> const&);
};


#endif