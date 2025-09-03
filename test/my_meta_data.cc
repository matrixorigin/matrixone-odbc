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

/*
  This file implements tests that check that meta-data information reported
  by our driver via ODBC APIs is as expected.

  It runs over a set of MySQL types given by SAMPLE_LIST() macro defiend
  in meta_data_samples.h header. Each entry in SAMPLE_LIST() describes a table
  column -- its type, type parameters and column attributes (if any). Tests
  check if meta-data reported for such column is as expected. If this is not
  the case tests print diagnostics showing the differences.

  Current tests check information returned by SQLColumns() API and also
  cross-check it with general type information returned by SQLGetTypeInfo().
  The intention is that in the future these tests should be extended to cover
  other kinds of meta-data available through ODBC APIs (mainly result-set
  meta-data, SQLDescribeCol(), IRD etc.).

  The main logic for testing meta-data reported for MySQL type T by driver
  variant D (Unicode or ANSI) is implemented by static method
  `check_sample<T,D>::check()` defined below. This is used to process samples
  from SAMPLE_LIST() -- see test definitions at the end of this file.

  Test implementation uses tools defined in "meta_data.h" and
  "meta_data_odbc.h" headers. These rely on macros in "meta_data_constants.h"
  which describe known MySQL and ODBC types and various meta-data bits
  available through either MySQL or ODBC APIs.
*/


#include "odbc_util.h"
#include "meta_data.h"
#include "meta_data_odbc.h"
#include "meta_data_samples.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <optional>
#include <functional>
#include <numeric>
#include <vector>
#include <algorithm>


using std::vector;
using std::string;
using std::optional;

using col_t = odbc::col_t;     // stores data from SQLColumns()
using driver_variant = mysql::driver_variant_enum;


/*
  Structure `check_sample<T,D>` defines static method `check()` which performs
  the job of checking meta-data reported by ODBC driver variant D
  (Unicode/ANSI) for a MySQL table column of type T. Column attributes
  and column type parameters are passed as arguments to `check()`.
*/

template <mysql::type_enum T, driver_variant D>
struct check_sample
{
  using Col_spec  = mysql::Col_spec<T, D>;
  using param_t   = typename Col_spec::param_t;
  using attr_t    = mysql::attr_t;
  using type_info = odbc::Type_info;

  // Note: n-k is the sample number used for reporting.

  static
  bool check(
    SQLHSTMT stmt,
    size_t n, size_t k,
    param_t pp, attr_t aa
  )
  {
    bool pass = true;

    /*
      The `Col_spec` object `cs` represents the column used in the test table
      -- its type, type parameters (if any) and column attributes.

      This object also has `check_md()` method that performs the main job
      of verifying that the meta-data obtained from ODBC is as expected for
      the column described by it.
    */

    Col_spec cs{pp, aa};

    print("== CHECK", n, k, "with column: c", cs.str("c"));

    Test_table tbl{stmt, n, k, cs};  // creates test table.

    print("table created");

    /*
      The `cols` object obtains and stores information from ODBC `SQLColumns()`
      function. Our test column is the 1st one at `cols[1]`.
    */

    odbc::Columns cols{stmt, tbl.name()};
    auto& col = cols[1];

    print("got table meta-data");

    /*
      Here we fetch and store relevant data from I_S.COLUMNS table (for the
      1st column).
    */

    mysql::IS_Columns isc{stmt, "test", tbl.name(), 1};

    print("got I_S column information");

    // Convenience wrapper to execute multiple checks.

    auto check = [&pass](string name, std::function<bool()> t)
    {
      // print("/-- checking", name);
      bool res = t();
      // print("\\--", res ? "passed" : "FAILED");
      pass = pass && res;
    };

    /*
      The main job of checking that the meta-data is as expected for the given
      column is performed by `cs.check_md()` method. See below for its
      implementation.
    */

    check("main meta-data check", [&]() { return cs.check_md(isc, col); });

    /*
      Additional meta-data consistency checks starting with checking
      table/column names reported in ODBC meta-data
    */

    check("names consistency", [&]()
    {
      bool pass = true;

      ::print("checking type/object names");

      pass = check_str(tbl.name(), col.TABLE_NAME,
        "table column TABLE_NAME"
      ) && pass;

      pass = check_str(tbl.col_name(1), col.COLUMN_NAME,
        "table column COLUMN_NAME"
      ) && pass;

      return pass;
    });

    /*
      This check verifies that various ODBC meta-data bits agree with each
      other.
    */

    check("meta-data consistency", [&](){
      return odbc::check_md_consistency(col);
    });


    check("type info", [&]()
    {
      ::print("checking consistency with type information");

      /*
        Note: This returns SQLGetTypeInfo() records for the ODBC type to which
        column `cs` is mapped. We expect to find records corresponding to
        the column in that set.
      */

      type_info ti = cs.get_ti(stmt);
      short found = 0;

      /*
        Go through type info records finding ones that correspond to the table
        column (based on reported type name). Call `check_ti_consistency()`
        to verify that table column type information from `SQLColumns()`
        is consistent with generic information about the same type from
        `SQLGetTypeInfo()`.

        Note: Different MySQL types can map to the same ODBC type and their
        name is the only way to distinguish the corresponding records.
      */

      for (auto &i : ti)
      {
        if (i.TYPE_NAME != col.TYPE_NAME)
          continue;

        found++;
        pass = odbc::check_ti_consistency(i, col) && pass;
      }

      pass = check_true(found > 0,
        "no type info entry found for the column"
      ) && pass;

      pass = check_false(found > 1,
        "more than one type info entry found for the column"
      ) && pass;

      return pass;
    });

    return pass;
  }
};


/*
  # Meta-data expectations specific to given column type and attributes
  ===================================

  The basic checks that meta-data is as expected for a table column described
  by a `Col_spec<T,D>` object are defined by `Col_spec<T,D>::check_md()` method.

  Specializations `Col_spec<T,D>` are derived from `col_spec_base<G,T,U>` where
  `G` is the type group of the MySQL type `T` and `U` is the ODBC type which
  should be reported by ODBC driver for that column. The mapped ODBC type `U`
  for each MySQL type `T` is given by macro `MYSQL_ODBC_TYPE_MAP()` defined
  below.

  The `col_spec_base<G,T,U>` classes derive from `col_spec_common_base<T,U>`
  which defines meta-data checks that are common for all types and type groups.
  Specializations of `col_spec_base<G,T,U>` for various values of `G` and `T`
  can define additional checks specific for that type or type group.

  Note: Specializations of `Col_spec<T>` template based on
  `col_spec_base<G,T,U>` one are defined much later, when other required
  templates are fully defined.
*/

/*
  Macro that defines mapping of MySQL types into the corresponding ODBC types.
  An entry in the list has the form `_(T, O)` where `T` is a MySQL type
  (corresponding to `mysql::type_enum::t_T` constant) and `O` is
  the corresponding ODBC type. The mapping has two variants -- for the Unicode
  and ANSI driver.

  Note: After long discussion with AI the final interpretation here is that
  ANSI driver should report character columns as `SQL_CHAR` (and map
  `SQL_C_DEFAULT` to `SQL_CHAR`) while Unicode driver should report them
  as `SQL_WCHAR` (and map `SQL_C_DEFAULT` to `SQL_WCHAR`). This should
  be independent from the actual encoding used to store data in the server --
  the driver performs conversions to the indicated types.
*/

#define MYSQL_ODBC_TYPE_MAP_U(_) \
  MYSQL_ODBC_STRING_MAP_U(_) \
  MYSQL_ODBC_NUMERIC_MAP(_) \
  MYSQL_ODBC_TEMPORAL_MAP(_) \
  MYSQL_ODBC_GEOMETRY_MAP(_) \
  MYSQL_ODBC_OTHER_MAP(_) \

#define MYSQL_ODBC_TYPE_MAP_A(_) \
  MYSQL_ODBC_STRING_MAP_A(_) \
  MYSQL_ODBC_NUMERIC_MAP(_) \
  MYSQL_ODBC_TEMPORAL_MAP(_) \
  MYSQL_ODBC_GEOMETRY_MAP(_) \
  MYSQL_ODBC_OTHER_MAP(_) \


/*
  Note: By default MySQL treats FLOAT as single-precision REAL number. This
  is different from ODBC standard which assumes FLOAT = DOUBLE in some places.
*/

#define MYSQL_ODBC_NUMERIC_MAP(_) \
  _(integer,    integer) \
  _(smallint,   smallint) \
  _(tinyint,    tinyint) \
  _(mediumint,  integer) \
  _(bigint,     bigint) \
  _(decimal,    decimal) \
  _(double,     double) \
  _(float,      real) \

#define MYSQL_ODBC_STRING_MAP_U(_) \
  _(char,       wchar) \
  _(varchar,    wvarchar) \
  _(text,       wlongvarchar) \
  _(tinytext,   wlongvarchar) \
  _(mediumtext, wlongvarchar) \
  _(longtext,   wlongvarchar) \
  _(set,        wchar) \
  _(enum,       wchar) \
  MYSQL_ODBC_BIN_MAP(_)

#define MYSQL_ODBC_STRING_MAP_A(_) \
  _(char,       char) \
  _(varchar,    varchar) \
  _(text,       longvarchar) \
  _(tinytext,   longvarchar) \
  _(mediumtext, longvarchar) \
  _(longtext,   longvarchar) \
  _(set,        char) \
  _(enum,       char) \
  MYSQL_ODBC_BIN_MAP(_)

#define MYSQL_ODBC_BIN_MAP(_) \
  _(binary,     binary) \
  _(varbinary,  varbinary) \
  _(blob,       longvarbinary) \
  _(tinyblob,   longvarbinary) \
  _(mediumblob, longvarbinary) \
  _(longblob,   longvarbinary) \

#define MYSQL_ODBC_TEMPORAL_MAP(_) \
  _(date,       type_date) \
  _(time,       type_time) \
  _(datetime,   type_date) \
  _(timestamp,  type_timestamp) \
  _(year,       tinyint) \

#define MYSQL_ODBC_GEOMETRY_MAP(_) \
  _(geometry,           longvarbinary) \
  _(geometrycollection, longvarbinary) \
  _(point,              longvarbinary) \
  _(multipoint,         longvarbinary) \
  _(linestring,         longvarbinary) \
  _(multilinestring,    longvarbinary) \
  _(polygon,            longvarbinary) \
  _(multipolygon,       longvarbinary) \

#define MYSQL_ODBC_OTHER_MAP(_) \
  _(bit,    bit) \
  _(json,   longvarchar) \
  _(vector, varbinary) \


// Check that all known MySQL types are covered by MYSQL_ODBC_TYPE_MAP()

constexpr
bool is_mapped_type(mysql::type_enum t)
{
  #undef CHECK
  #define CHECK(X,Y,...)  || (t == mysql::type_enum::t_##X)
  return false MYSQL_ODBC_TYPE_MAP_U(CHECK);
}

#undef CHECK_MAPPED
#define CHECK_MAPPED(G,T) \
static_assert(is_mapped_type(mysql::type_enum::t_##T) \
  , "MySQL type '" #T "' not mapped to ODBC -- update MYSQL_ODBC_TYPE_MAP()");

MYSQL_TYPE_LIST(CHECK_MAPPED)


/*
  Below specializations of `col_spec_base<G,T,U>` are defined which implement
  meta-data checks specific to given MySQL type `T` in type group `G` that
  is mapped to ODBC type `U`.

  Also specializations of accompanying template `type_param<G,T,U>` are
  defined. These are used by `col_spec_base<G,T,U>` to map parameters of MySQL
  type `T` (if any) to the parameters of the mapped ODBC type `U` (if any).
*/


namespace mysql
{
  /*
    These templates are used by `col_spec_common_base<>` -- see below.
  */

  template <type_group_enum, type_enum> struct col_spec_attr;
  template <type_group_enum, type_enum, odbc::type_enum> struct type_param;


  /*
    Class `col_spec_common_base<T,U>` is a common base for
    `col_spec_base<G,T,U>` classes which in turn are used to define
    `Col_spec<T,D>` specializations. It describes a table column of MySQL type
    `T` that is reported as ODBC type `U`, including column attributes
    and column type parameters (if type `T` has any).

    The `check_md()` method in this class implements meta-data checks that
    are common to any column regardless of its MySQL or ODBC type. These
    are extended and modified by `col_spec_base<G,T,U>` specializations that
    derive from this base class.

    Apart from `check_md()` method this class also defines:

    - storage for column attributes and column type parameters (if any);
    - methods to get string with SQL definition of the column;
    - method to get corresponding SQLTypeInfo() records.
  */

  template <type_enum T, odbc::type_enum U>
  struct col_spec_common_base
  : odbc::type_traits<U>
  {
    /*
      Template specialization `odbc_traits` describes the mapped ODBC type `U`
      and, among other things, defines meta-data checks for that type.
    */

    using odbc_traits = odbc::type_traits<U>;

    /*
      Template specialization `type_param` defines a C++ type representing
      parameters of MySQL type `T` (if any) and determines how these
      are converted to parameters of the corresponding ODBC type `U`.
    */

    using type_param = mysql::type_param<type_group(T), T, U>;
    using param_t = typename type_param::type;

    /*
      Template specialization `attr_check` defines checks for consistency
      of ODBC meta-data with MySQL table column attributes.
    */

    using attr_check = col_spec_attr<type_group(T), T>;

    param_t pp;             // type parameters (if any)
    attr_t attr;            // column attributes

    /*
      Note: Static method `type_param::get()` converts MySQL type parameters
      `pp` and column attributes `aa` into parameters of the mapped ODBC type
      `U` -- these are passed to the `odbc_traits` constructor.
    */

    col_spec_common_base(param_t pp, attr_t aa)
    : odbc_traits{type_param::get(pp,aa)}
    , pp{pp}, attr{aa}
    {}

    /*
      Check meta-data for a MySQL table column of type described by this
      `Col_spec` instance.
    */

    bool check_md(
      IS_Columns const &isc,  // From I_S.COLUMNS       (table)
      col_t const &col        // From SQLColumns()      (table)
    )
    {
      bool pass = true;

      // Checks related to the mapped ODBC type.
      ::print("checking meta-data expectations for mapped ODBC type:", U);
      pass = odbc_traits::check_md(col) && pass;

      /*
        Check that the ODBC table column meta-data is consistent with MySQL
        column attributes.
      */

      ::print("checking column attributes");
      pass = attr_check::check(attr, col) && pass;

      ::print("checking consistency with INFORMATION_SCHEMA");

      /*
        Check that the ODBC table column meta-data is consistent with
        information from I_S.COLUMNS table. To be precise, if I_S.COLUMNS gives
        a relevant bit of information (rather than leaving it NULL) then
        we check that ODBC meta-data reports the same information, if
        I_S.COLUMNS bit is NULL then that does not put any constraints on ODBC
        meta-data.

        Here we check only type name and IS_NULLABLE bit that is common to all
        types -- checks for other I_S.COLUMNS fields are type-specific and
        are defined below in relevant col_spec_base<> specializations.
      */

      #undef CHECK_IS_OPT
      #define CHECK_IS_OPT(T,X,Y) \
        if (isc.X.has_value()) { \
          pass = check_true(col.Y.has_value(), "table column ?" #Y, ", present in I_S.COLUMNS but not in table column meta-data:", *isc.X) && pass; \
          if (col.Y.has_value()) \
            CHECK_##T(*isc.X, *col.Y, "table column ?" #X, "I_S:", "col:") }

      #undef CHECK_IS
      #define CHECK_IS(T,X,Y) \
        if (isc.X.has_value()) { CHECK_##T(*isc.X, col.Y, "table column " #X, "I_S:", "col:") }

      CHECK_IS_OPT(str, IS_NULLABLE, IS_NULLABLE);
      CHECK_IS(str, DATA_TYPE, TYPE_NAME);

      /*
        Additionally check that I_S.DATA_TYPE is defined -- we expect it
        to be not null even though the I_S column is nullable.
      */

      pass = check_true(isc.DATA_TYPE.has_value(),
        "I_S.COLUMNS does not define DATA_TYPE for the column"
      ) && pass;

      return pass;
    }


    /*
      Return `SQLTypeInfo()` records corresponding to the mapped ODBC type `U`.
    */

    auto get_ti(SQLHSTMT stmt) -> odbc::Type_info
    {
      return odbc::Type_info{stmt, U};
    }

    /*
      Produce string with MySQL table column specification corresponding
      to the data in this `Col_spec` instance. This can be used in
      CREATE TABLE statement.
    */

    virtual void print(string col, std::ostream &out) const
    {
      out << type_name();
      type_param::print(pp, out);
      attr.print(col, out);
    }

    string str(string col) const
    {
      std::stringstream out;
      this->print(col, out);
      return out.str();
    }

    /*
      This is the MySQL type name to be used in table column definition.
    */

    virtual const char* type_name() const
    {
      return mysql::type_name(T);
    }
  };


  /*
    Default definition of `col_spec_base<G,T,U>` performs only the checks
    of `col_spec_common_base<T,U>` that are common to all types. Below we
    define additional specializations that require other checks.
  */

  template <type_group_enum G, type_enum T, odbc::type_enum U>
  struct col_spec_base
  : col_spec_common_base<T,U>
  {
    using Base = col_spec_common_base<T,U>;
    using Base::Base;
  };


  /*
    This helper template is used to define most common specializations
    of `type_param<G,T,U>` template. Member type `type` is the C++ type used
    to represent parameters of MySQL type `T`. Static function get() converts
    MySQL type parameters and column attributes to the corresponding parameters
    of the mapped ODBC type.

    The variant with  LEN = false  assumes that MySQL type has the same
    parameters as the ODBC type (represented by the same C++ type
    `odbc::type_traits<U>::param_t`). The variant with  LEN = true  assumes
    that the MySQL type has optional length or precision parameter (and that
    it also applies to the mapped ODBC type)
  */

  template <odbc::type_enum U, bool LEN = false>
  struct type_param_base
  {
    using odbc_traits = odbc::type_traits<U>;
    using odbc_param_t = typename odbc_traits::param_t;

    // For MySQL type use the same parameters as for the ODBC type
    using type = odbc_param_t;

    static
    auto get(type pp, attr_t) -> odbc_param_t
    {
      // Note: column attributes are ignored
      return pp;
    }

    static
    void print(type, std::ostream&)
    {}
  };

  template <odbc::type_enum U>
  struct type_param_base<U, true>
  : type_param_base<U, false>
  {
    using typename type_param_base<U, false>::odbc_param_t;
    using type = optional<ulen_t>;  // optional length/precision parameter

    static
    auto get(type pp, attr_t) -> odbc_param_t
    {
      /*
        Note: column attributes are ignored and we assume that the ODBC type
        parameters can be initialized by an `optional<ulen_t>` value.
      */
      return pp;
    }

    static
    void print(type pp, std::ostream& out)
    {
      if (pp.has_value())
        out << "(" << *pp << ")";
    }
  };

  /*
    By default we assume MySQL type has the same parameters as the mapped ODBC
    type. This also covers the case when both types have no parameters. Other
    cases are handled by specializations below.
  */

  template <type_group_enum, type_enum, odbc::type_enum U>
  struct type_param
  : type_param_base<U>
  {};



  /*
    ## String types
    -----------------------------------

    `col_spec_base<>` specializations for string types derive from
    `col_spec_string_base<>` which adds meta-data checks specific for string
    types.
  */


  template <type_enum T, odbc::type_enum U>
  struct col_spec_string_base
  : col_spec_common_base<T,U>
  {
    using Base = col_spec_common_base<T,U>;
    using Base::Base;

    bool check_md(
      IS_Columns const &isc,  // From I_S.COLUMNS       (table)
      col_t const &col        // From SQLColumns()      (table)
    )
    {
      bool pass = Base::check_md(isc, col);

      /*
        Note: These I_S.COLUMNS fields related to character types do not have
        representation in ODBC meta-data and are ignored: CHARACTER_SET_NAME,
        COLLATION_NAME.
      */

      //                I_S.COLUMNS               SQLColumns()
      // -------------- ------------------------- ------------------
      CHECK_IS_OPT(num, CHARACTER_MAXIMUM_LENGTH, COLUMN_SIZE);
      CHECK_IS_OPT(num, CHARACTER_OCTET_LENGTH,   CHAR_OCTET_LENGTH);

      return pass;
    }

    /*
      Note: These sizes are given in [1]. They are used in internal function
      `buf_size_()` defined below.

      [1] https://dev.mysql.com/doc/refman/9.4/en/string-type-syntax.html
    */

    static constexpr ulen_t tiny_size  = ((ulen_t)1<<8) - 1;
    static constexpr ulen_t plain_size  = ((ulen_t)1<<16) - 1;
    static constexpr ulen_t medium_size = ((ulen_t)1<<24) - 1;
    static constexpr ulen_t long_size = ((ulen_t)1<<32) - 1;


    ulen_t buf_size() const override
    {
      if constexpr (buf_size_().has_value())
        return *buf_size_();
      else
      {
        /*
          Note: The inherited methods calculates buffer size  from the column
          size which should be given explicitly as type length parameter.
        */

        return Base::buf_size();
      }
    }

    ulen_t size() const override
    {
      /*
        Column size is the maximum possible length (in characters) of a string
        stored in column of type T.
      */

      if constexpr (buf_size_().has_value())
      {
        /*
          For string types for which buffer size is explicitly defined above
          we return that buffer size because in the worst case we can have
          a string with 1-byte characters (assuming encoding like UTF-8).

          For example a `TINYTEXT` column with buffer size of 255 can store
          maximum of 255 characters.

          Note: For true mb encoding we would need to divide the binary
          length by mb width.
        */

        return *buf_size_();
      }
      else
      {
        /*
          Remaining string types have length parameter and for them `size()`
          is overriden to return that length.
        */

        return Base::size();
      }
    }

   private:

    static constexpr optional<ulen_t> buf_size_()
    {
     if constexpr (T == type_enum::t_tinytext || T == type_enum::t_tinyblob)
       return tiny_size;
     else
     if constexpr (T == type_enum::t_text || T == type_enum::t_blob)
       return plain_size;
     else
     if constexpr (T == type_enum::t_mediumtext || T == type_enum::t_mediumblob)
       return medium_size;
     else
     if constexpr (T == type_enum::t_longtext || T == type_enum::t_longblob)
       return long_size;
     else
       return {};
    }
  };

  template <type_enum T, odbc::type_enum U>
  struct col_spec_base<type_group_enum::string, T, U>
  : col_spec_string_base<T, U>
  {
    using Base = col_spec_string_base<T, U>;
    using Base::Base;
  };


  // For VARCHAR/VARBINARY types the length parameter is compulsory

  template <odbc::type_enum U>
  struct type_param<type_group_enum::string, type_enum::t_varchar, U>
  : type_param_base<U>
  {
    using type = ulen_t;

    static
    void print(type pp, std::ostream& out)
    {
      out << "(" << pp << ")";
    }
  };

  template <odbc::type_enum U>
  struct type_param<type_group_enum::string, type_enum::t_varbinary, U>
  : type_param<type_group_enum::string, type_enum::t_varchar, U>
  {};


  /*
    CHAR/BINARY also have length parameter - if not specified then the default
    is 1.
  */

  template <odbc::type_enum U>
  struct type_param<type_group_enum::string, type_enum::t_char, U>
  : type_param_base<U, true> // optional length parameter
  {
    using Base = type_param_base<U,true>;
    using typename Base::type;
    using typename Base::odbc_param_t;

    static
    auto get(type pp, attr_t) -> odbc_param_t
    {
      if (!pp.has_value())  return 1;
      return *pp;
    }
  };

  template <odbc::type_enum U>
  struct type_param<type_group_enum::string, type_enum::t_binary, U>
  : type_param<type_group_enum::string, type_enum::t_char, U>
  {};


  /*
    Special case of TEXT/BLOB type.

    These types can have a length parameter and if present then
    `TEXT(N)`/`BLOB(N)` is an alias for `TINY/MEDIUM/LONG` type variant long
    enough to hold N characters/bytes (see [1]).

    Note: Below the mapping like `TEXT(N) -> TINY/MEDIUM/LONGTEXT` affects only
    reported buffer/column size but perhaps all meta-data checks should
    be affected? Leaving as is for now.

    [1] https://dev.mysql.com/doc/refman/9.4/en/string-type-syntax.html
  */

  template <odbc::type_enum U>
  struct type_param<type_group_enum::string, type_enum::t_text, U>
  : type_param_base<U, true> // optional length parameter
  {
    using Base = type_param_base<U, true>;
    using typename Base::type;
    using typename Base::odbc_param_t;

    static
    auto get(type pp, attr_t) -> odbc_param_t
    {
      /*
        Note: Ignore parameter value - the mapped ODBC type should have
        no parameters
      */
      return {};
    }
  };

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::string, type_enum::t_text, U>
  : col_spec_string_base<type_enum::t_text, U>
  {
    using Base = col_spec_string_base<type_enum::t_text, U>;
    using Base::Base;
    using Base::pp;

    ulen_t buf_size() const override
    {
      if (!pp.has_value())
        return Base::buf_size();

      for (ulen_t size : {
        Base::tiny_size, Base::plain_size, Base::medium_size, Base::long_size
      })
        if (*pp <= size)
          return size;

      assert(false);
      return 0;
    }

    ulen_t size() const override
    {
      /*
        Note: For example type TEXT(1) is an alias for TINYTEXT and therefore
        it can store up to 255 characters.
      */
      return buf_size();
    }
  };


  template <odbc::type_enum U>
  struct type_param<type_group_enum::string, type_enum::t_blob, U>
  : type_param<type_group_enum::string, type_enum::t_text, U>
  {};

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::string, type_enum::t_blob, U>
  : col_spec_string_base<type_enum::t_blob, U>
  {
    using Base = col_spec_string_base<type_enum::t_blob, U>;
    using Base::Base;
    using Base::pp;

    ulen_t buf_size() const override
    {
      if (!pp.has_value())
        return Base::size();

      for (ulen_t size : {
        Base::tiny_size, Base::plain_size, Base::medium_size, Base::long_size
      })
        if (*pp <= size)
          return size;

      assert(false);
      return 0;
    }

    ulen_t size() const override
    {
      return buf_size();
    }
  };


  /*
    Special case of SET/ENUM type.

    These types have a parameter that is a list of possible values.
  */

  template <odbc::type_enum U>
  struct type_param<type_group_enum::string, type_enum::t_enum, U>
  : type_param_base<U>
  {
    using typename type_param_base<U>::odbc_param_t;
    using type = vector<string>;

    static
    auto get(type pp, attr_t) -> odbc_param_t
    {
      // Note: Underlying ODBC string length is the size of the longest value.

      if (pp.empty())
        return 0;

      // Note: This computation assumes plain ASCII values (1 byte/char)

      return std::max_element(
        pp.begin(), pp.end(),
        [](const string& a, const string& b)
        {
          return a.size() < b.size();
        }
      )->size();
    }

    static
    void print(type pp, std::ostream& out)
    {
      bool first = true;
      out << "(";
      for (auto const &v : pp)
      {
        out << (first ? "": ", ") << "\"" << v << "\"";
        first = false;
      }
      out << ")";
    }
  };

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::string, type_enum::t_enum, U>
  : col_spec_common_base<type_enum::t_enum, U>
  {
    using Base = col_spec_common_base<type_enum::t_enum, U>;
    using typename Base::param_t;
    using Base::Base;
    using Base::pp;

    ulen_t size() const override
    {
      /*
        Note: `pp` stores the list of possible values and `type_param::get()`
        computes the length of the longest value (in characters) which is our
        column size in this case.
      */
      return Base::type_param::get(pp, {});
    }
  };


  template <odbc::type_enum U>
  struct type_param<type_group_enum::string, type_enum::t_set, U>
  : type_param<type_group_enum::string, type_enum::t_enum, U>
  {
    using Base = type_param<type_group_enum::string, type_enum::t_enum, U>;
    using typename Base::type;
    using typename Base::odbc_param_t;

    static
    auto get(type pp, attr_t) -> odbc_param_t
    {
      /*
        Note: Underlying ODBC string size is the length of the longest possible
        value which is the concatenation of all possible SET strings with
        commas.
      */

      if (pp.empty())
        return 0;

      // Note: This computation assumes plain ASCII values (1 byte/char).

      return std::accumulate(
        pp.begin(), pp.end(), (ulen_t)0,
        [](size_t sum, const std::string& str)
        {
          return sum + str.size() + 1; // Note: +1 for separating comma
        }
      ) - 1; // Note: subtract leading comma
    }
  };

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::string, type_enum::t_set, U>
  : col_spec_common_base<type_enum::t_set, U>
  {
    using Base = col_spec_common_base<type_enum::t_set, U>;
    using typename Base::param_t;
    using Base::Base;
    using Base::pp;

    ulen_t size() const override
    {
      /*
        Note: `pp` stores the list of possible values and `type_param::get()`
        computes the length of a string representing the set of all these
        values (in characters) which is our column size in this case.
      */
      return Base::type_param::get(pp, {});
    }
  };


  /*
    ## Numeric types
    -----------------------------------

    `col_spec_base<>` specializations for numeric types derive from
    `col_spec_numeric_base<>` which adds meta-data checks specific for
    numeric types.
  */


  template <type_enum T, odbc::type_enum U>
  struct col_spec_numeric_base
  : col_spec_common_base<T,U>
  {
    using Base = col_spec_common_base<T,U>;
    using Base::Base;

    bool check_md(
      IS_Columns const &isc,  // From I_S.COLUMNS       (table)
      col_t const &col        // From SQLColumns()      (table)
    )
    {
      bool pass = Base::check_md(isc, col);

      //                I_S.COLUMNS         SQLColumns()
      // -------------- ------------------- ------------------
      CHECK_IS_OPT(num, NUMERIC_PRECISION,  COLUMN_SIZE);
      CHECK_IS_OPT(num, NUMERIC_SCALE,      DECIMAL_DIGITS);

      return pass;
    }
  };

  template <type_enum T, odbc::type_enum U>
  struct col_spec_base<type_group_enum::numeric, T, U>
  : col_spec_numeric_base<T, U>
  {
    using Base = col_spec_numeric_base<T, U>;
    using Base::Base;
  };


  /*
    ODBC numeric types other than DECIMAL/NUMERIC have a boolean parameter
    telling whether the type is unsigned. This is determined by column
    attributes (not a type parameter - MySQL numeric types have no parameters
    by default).
  */

  template <type_enum T, odbc::type_enum U>
  struct type_param<type_group_enum::numeric, T, U>
  : type_param_base<U>
  {
    using typename type_param_base<U>::odbc_param_t;
    using type = unit_t;

    static
    auto get(type, attr_t aa) -> odbc_param_t
    {
      return aa.has(coll_attr_enum::a_unsigned);
    }

    static
    void print(type, std::ostream&)
    {}
  };


  /*
    For ODBC DECIMAL/NUMERIC types the parameters are precision and scale,
    the same as for the corresponding MySQL types.
  */

  template <odbc::type_enum U>
  struct type_param<type_group_enum::numeric, type_enum::t_decimal, U>
  : type_param_base<U>
  {
    using typename type_param_base<U>::type;

    static
    void print(type pp, std::ostream& out)
    {
      out << "(" << pp.precision;
      out << "," << pp.scale;
      out << ")";
    }
  };


  /*
    Special case of FLOAT type.

    FLOAT is special because it maps to REAL or DOUBLE depending on
    the specified precision (if no precision is given it maps to REAL).
  */

  template <odbc::type_enum U>
  struct type_param<type_group_enum::numeric, type_enum::t_float, U>
  : type_param_base<U, true>
  {
    using Base = type_param_base<U,true>;
    using typename Base::type;
    using typename Base::odbc_param_t;

    static
    auto get(type, attr_t) -> odbc_param_t
    {
      return false; // Note: map to signed ODBC type
    }
  };

  /*
    Meta-data checks for DOUBLE type.

    Note: This is defined later when all `Col_spec<>` specializations
    are available.
  */

  bool check_double(IS_Columns const& isc, col_t const& col);

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::numeric, type_enum::t_float, U>
  : col_spec_numeric_base<type_enum::t_float, U>
  {
    using Base = col_spec_numeric_base<type_enum::t_float, U>;
    using typename Base::param_t;
    using Base::Base;
    using Base::pp;

    col_spec_base(param_t pp, attr_t aa)
    : Base{pp, aa}
    {}

    bool check_md(
      IS_Columns const &isc,  // From I_S.COLUMNS       (table)
      col_t const &col        // From SQLColumns()      (table)
    )
    {
      if (pp.has_value() && *pp > 24)
      {
        return check_double(isc, col);
      }

      return Base::check_md(isc, col);
    }
  };


  /*
    ## Temporal types
    -----------------------------------

    Most MySQL temporal types have optional seconds precision parameter except
    `DATE` and `YEAR`. The latter maps to numeric `TINYINT` type (unsigned).
  */

  template <type_enum T, odbc::type_enum U>
  struct type_param<type_group_enum::temporal, T, U>
  : type_param_base<U, true> // optional precision parameter
  {};


  template <type_enum T, odbc::type_enum U>
  struct col_spec_base<type_group_enum::temporal, T, U>
  : col_spec_common_base<T,U>
  {
    using Base = col_spec_common_base<T,U>;
    using Base::Base;

    bool check_md(
      IS_Columns const &isc,  // From I_S.COLUMNS       (table)
      col_t const &col        // From SQLColumns()      (table)
    )
    {
      bool pass = Base::check_md(isc, col);

      //                I_S.COLUMNS         SQLColumns()
      // -------------- ------------------- ------------------
      CHECK_IS_OPT(num, DATETIME_PRECISION, DECIMAL_DIGITS);

      return pass;
    }
  };

  // DATE and YEAR types have no precision parameter.

  template <odbc::type_enum U>
  struct type_param<type_group_enum::temporal, type_enum::t_date, U>
  : type_param_base<U>
  {};

  template <odbc::type_enum U>
  struct type_param<type_group_enum::temporal, type_enum::t_year, U>
  : type_param_base<U>
  {
    using typename type_param_base<U>::odbc_param_t;
    using typename type_param_base<U>::type;

    static
    auto get(type, attr_t) -> odbc_param_t
    {
      return true; // Note: We use unsigned ODBC type
    }
  };

  /*
    `YEAR` type is mapped to small integer.
  */

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::temporal, type_enum::t_year, U>
  : col_spec_numeric_base<type_enum::t_year, U>
  {
    using Base = col_spec_numeric_base<type_enum::t_year, U>;
    using Base::Base;

    /*
      Note: Disable checks for precision information which is not relevant
      for that type.
    */

    bool check_precision(col_t const &col) override
    {
      bool pass = true;
      CHECK_NOT_EXPECTED(col.DECIMAL_DIGITS, "table column DECIMAL_DIGITS");
      return pass;
    }
  };


  /*
    ## Spatial types
    -----------------------------------

    For these types meta-data checks are the same as for LONGBLOB type.
  */

  template <type_enum T, odbc::type_enum U>
  struct col_spec_base<type_group_enum::spatial, T, U>
  : col_spec_base<type_group_enum::string, type_enum::t_longblob, U>
  {
    using Base = col_spec_base<
      type_group_enum::string, type_enum::t_longblob, U
    >;

    using Base::Base;
    using Base::attr;

    const char* type_name() const override
    {
      return mysql::type_name(T);
    }

    void print(string col, std::ostream& out) const override
    {
      /*
        Note: Add SRID attribute to avoid the following warning when indexing
        spatial columns:

        > The spatial index on column 'c' will not be used by the query
        > optimizer since the column does not have an SRID attribute. Consider
        > adding an SRID attribute to the column.
      */

      out << type_name() << " SRID 4326";  // Note: GPS coordinates
      attr.print(col, out);
    }
  };


  /*
    ## Other types
    -----------------------------------
  */

  /*
    Special case of BIT type.

    It is special because the ODBC mapped type depends on the size parameter
    of the `BIT` type: for `BIT(1)` we map to ODBC type `BIT` but for `BIT(N)`
    with  N > 1  we map to ODBC type `BIGINT` (see bug#16590994).
  */

  template <odbc::type_enum U>
  struct type_param<type_group_enum::other, type_enum::t_bit, U>
  : type_param_base<U, true> // optional length parameter
  {
    using Base = type_param_base<U, true>;
    using typename Base::type;
    using typename Base::odbc_param_t;

    static
    auto get(type, attr_t) -> odbc_param_t
    {
      // Note: When mapped to numeric type, use unsigned one.

      if constexpr (odbc::type_group_enum::numeric == odbc::type_group(U))
        return true;
      else
        return {};
    }
  };

  /*
    Note: Template parameter `U` is the ODBC type used for "plain" `BIT` type.
    For `BIT(N)` with  N>1  we fix the mapped ODBC type to `BIGINT`.
  */

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::other, type_enum::t_bit, U>
  : col_spec_common_base<type_enum::t_bit, odbc::type_enum::t_bigint>
  {
    using Base = col_spec_common_base<
      type_enum::t_bit, odbc::type_enum::t_bigint
    >;

    using typename Base::param_t;
    using Base::pp;

    col_spec_base(param_t pp, attr_t aa)
    : Base{pp, aa}
    {}

    bool check_md(
      IS_Columns const &isc,  // From I_S.COLUMNS       (table)
      col_t const &col        // From SQLColumns()      (table)
    )
    {
      static col_spec_common_base<type_enum::t_bit, U> bit_spec{{},{}};

      if (pp.has_value() && *pp > 1)
        return Base::check_md(isc, col);
      else
        return bit_spec.check_md(isc, col);
    }

    ulen_t size() const override
    {
      return pp.has_value() ? *pp : 1;
    }

    bool check_precision(col_t const &col) override
    {
      bool pass = true;

      CHECK_NOT_EXPECTED(col.DECIMAL_DIGITS, "table column DECIMAL_DIGITS");

      // Expect precision radix to be 2 (to correctly interpret column size)

      if (check_true(col.NUM_PREC_RADIX.has_value(),
        "table column NUM_PREC_RADIX is not defined"
      ))
      {
        pass = check_num(2, *col.NUM_PREC_RADIX, "table column NUM_PREC_RADIX")
          && pass;
      }
      else
        pass = false;

      return pass;
    }
  };

  /*
    Special case of JSON type.

    Make the expectations identical as for LONGTEXT type.
  */

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::other, type_enum::t_json, U>
  : col_spec_base<type_group_enum::string, type_enum::t_longtext, U>
  {
    using Base = col_spec_base<
      type_group_enum::string, type_enum::t_longtext, U
    >;
    using Base::Base;
  };


  /*
    Special case of VECTOR type.

    We report `VECTOR(N)` column as `VARBINARY(L)` where  L = 4*N  is
    the length of the binary representation of a vector. The VECTOR length
    parameter is optional and if not given it defaults to 2048.

    [1] https://dev.mysql.com/doc/refman/9.4/en/vector.html
  */

  template <odbc::type_enum U>
  struct type_param<type_group_enum::other, type_enum::t_vector, U>
  : type_param_base<U, true> // optional length parameter
  {
    using Base = type_param_base<U,true>;
    using typename Base::type;
    using typename Base::odbc_param_t;

    static
    auto get(type pp, attr_t) -> odbc_param_t
    {
      return 4 * len_(pp);
    }

    static
    ulen_t len_(type pp)
    {
      return pp.has_value() ? *pp : 2048;
    }
  };

  template <odbc::type_enum U>
  struct col_spec_base<type_group_enum::other, type_enum::t_vector, U>
  : col_spec_common_base<type_enum::t_vector, U>
  {
    using Base = col_spec_common_base<type_enum::t_vector, U>;
    using typename Base::param_t;
    using typename Base::type_param;
    using Base::check_md;

    ulen_t len;

    col_spec_base(param_t pp, attr_t aa)
    : Base{pp, aa}
    , len{type_param::len_(pp)}
    {}

    ulen_t size() const override
    {
      return len;
    }

    ulen_t buf_size() const override
    {
      return 4 * len;
    }
  };


  /*
    ## Define `Col_spec<T>` specializations
    ---------------------------------

    For each MySQL type `T` and driver type `D` derive specialization
    `Col_spec<T,D>` from `col_spec_base<G,T,U>` where `G` is the type group
    of `T` and `U` is the corresponding ODBC type (for driver type `D`)
    as given by `MYSQL_ODBC_TYPE_MAP_*()` macro.
  */

  #define COL_SPEC_DEF_U(T,O,...) \
  template <> struct Col_spec<type_enum::t_##T, mysql::Unicode> \
  : col_spec_base<type_group(type_enum::t_##T), type_enum::t_##T, odbc::type_enum::t_##O> { using col_spec_base::col_spec_base; };

  #define COL_SPEC_DEF_A(T,O,...) \
  template <> struct Col_spec<type_enum::t_##T, mysql::ANSI> \
  : col_spec_base<type_group(type_enum::t_##T), type_enum::t_##T, odbc::type_enum::t_##O> { using col_spec_base::col_spec_base; }; \

  MYSQL_ODBC_TYPE_MAP_U(COL_SPEC_DEF_U)
  MYSQL_ODBC_TYPE_MAP_A(COL_SPEC_DEF_A)


  /*
    This can be defined now when all `Col_spec<>` specializations are available.
  */

  bool check_double(IS_Columns const& isc, col_t const& col)
  {
    static Col_spec<type_enum::t_double, mysql::Unicode> double_spec{{},{}};
    return double_spec.check_md(isc, col);
  }

}


namespace mysql
{
  /*
    # Column attribute checks
    ===================================

    Template specialization `col_spec_attr<G,T>` defines static member
    `check()` which checks if ODBC meta-data is consistent with given
    attributes for column of MySQL type `T` in type group `G`. See macro
    `MYSQL_ATTR_LIST()` in `meta_data_constants.h` for a list of recognized
    column attributes.

    Note: Currently only the `not_null` attribute has bearing on table column
    meta-data. But when we add result set meta-data checks in the future there
    can be more constraints.

    Note: For numeric types the `unsigned` attribute translates to the unsigned
    parameter of the underlying ODBC numeric type -- see
    `type_param<numeric,T,U>::get()` method.
  */

  struct attr_base
  {
    static
    bool check(attr_t const &aa, col_t const &col)
    {
      bool pass = true;

      if (aa.has(coll_attr_enum::a_not_null))
      {
        if (aa.get<coll_attr_enum::a_not_null>())
        {
          pass = check_false(odbc::nullable_enum::v_nullable == col.NULLABLE,
            "table column reported as nullable"
            " while column is declared as NOT NULL"
          ) && pass;
        }
        else
        {
          pass = check_false(odbc::nullable_enum::v_no_nulls == col.NULLABLE,
            "table column reported as no_nulls"
            " while column is declared as NULL"
          ) && pass;
        }
      }

      return pass;
    }
  };

  // For now only common checks

  template <type_group_enum, type_enum>
  struct col_spec_attr
  : public attr_base
  {};

}   // mysql


/*
  # `Test_table` class implementation
  ===================================
*/

template <mysql::type_enum T, driver_variant D>
Test_table::Test_table(SQLHSTMT stmt, size_t n, size_t k, mysql::Col_spec<T,D> const &cs)
: odbc::table{stmt, "test", get_name(n,k), get_col_def(cs)}
{}

string Test_table::get_name(size_t n, size_t k)
{
  std::stringstream name;
  name << "t_" << n << "_" << k;
  return name.str();
}

template <mysql::type_enum T, driver_variant D>
string Test_table::get_col_def(mysql::Col_spec<T,D> const &cs)
{
  /*
    Note: We add an explicit primary key column to avoid making our test column
    `c` the primary key. We want to test scenarios where `c` is indexed but
    it should not be the primary key as there are restrictive constraints
    on possible types of a primary key column.
  */

  string cols = "c " + cs.str("c") + ", k int PRIMARY KEY";
  return cols;
}


size_t Test_table::col_cnt() const
{
  return 2;
}

string Test_table::col_name(size_t) const
{
  return "c";
}


/*
  # Test definitions
  ===================================

  Macros `SAMPLE_LIST_G(_)` for different type groups  G  describe sample mysql
  table column definitions that are used to test meta-data for different types.
  Example entry in a list:
  ```
  _(5,9, numeric, decimal, ({6,2}), a_auto_increment({}), a_not_null(true))
  ```

  This entry describes sample #5-9 with table column of numeric type `DECIMAL`
  with the following definition:
  ```
    DECIMAL(6,2) NOT NULL AUTO_INCREMENT
  ```

  In general sample entries are of the form `_(N,K,G,T,PP,AA...)` where  N-K
  is the sample number, T  is mysql type in type group  G. PP  is
  an initializer for type parameters (if any) and  AA...  is a (possibly empty)
  list of attributes.

  Macro `CHECK_SAMPLES(G)` uses these sample lists to generate test code for
  samples in group G.
*/

#define CHECK_SAMPLES(G) \
    if (odbc::is_unicode()) { SAMPLE_LIST_##G(CHECK_SAMPLE_U) } \
    else { SAMPLE_LIST_##G(CHECK_SAMPLE_A) }


/*
  In test definitions the sample entries are processed using `CHECK_SAMPLE_D()`
  macro (where  D  is U/A for Unicode/ANSI driver) which creates appropriate
  instance of `check_sample<>` template for the driver variant  D  and calls
  the static `check()` method defined by it, passing required arguments:

   - ODBC statement handle
   - sample numbers
   - type parameters
   - an `attr_t` instance describing column attributes given by `AA...` list

   For example, for the sample entry above macro `CHECK_SAMPLE_U()` will
   generate the following code
   ```
    {
      using check_t = check_sample<mysql::type_enum::t_decimal, mysql::Unicode>;
      using namespace detail::attr_setters;
      mysql::attr_t aa;
      detail::apply_attr_setters(aa, a_auto_increment({}), a_not_null(true));
      try {
        pass = check_t::check(hstmt, 5, 9, {6,2}, std::move(aa)) && pass;
      }
      catch (odbc::Exception &e) { print("!! CHECK exception:", e.msg); }
      catch (odbc::SimpleException &e)
      { print("!! CHECK exception:", e.m_msg); }
      catch (std::exception &e) { print("!! CHECK exception:", e.what()); }
    }
   ```
*/

#define CHECK_SAMPLE_U(N,K,G,T,PP,...) \
  CHECK_SAMPLE_(mysql::Unicode,N,K,G,T,PP, ##__VA_ARGS__)
#define CHECK_SAMPLE_A(N,K,G,T,PP,...) \
  CHECK_SAMPLE_(mysql::ANSI,N,K,G,T,PP, ##__VA_ARGS__)

#define CHECK_SAMPLE_(D,N,K,G,T,PP,...) \
  { using check_t = check_sample<mysql::type_enum::t_##T, D>; \
    using namespace detail::attr_setters; \
    mysql::attr_t aa; detail::apply_attr_setters(aa, ##__VA_ARGS__); \
    try { pass = check_t::check(hstmt, N, K, UNWRAP_(PP), std::move(aa)) && pass; } \
    catch (odbc::Exception &e) { pass = false; print("!! CHECK exception:", e.msg); } \
    catch (odbc::SimpleException &e) { pass = false; print("!! CHECK exception:", e.m_msg); } \
    catch (std::exception &e) { pass = false; print("!! CHECK std exception:", e.what()); }}


/*
  Note: Since type parameters `PP` can contain commas they must be put
  in parentheses like in this example:
  ```
    CHECK_SAMPLE_D(5,9,numeric,decimal,({6,2}), ...)
  ```
  However, when passed to the `check()` method we need to remove
  the parentheses:
  ```
    check(hstmt, 5, 9, {6,2}, ...)
  ```
  Macro `UNWRAP_()` removes the parentheses from its argument.
*/

#define UNWRAP_(X) UNWRAP_IMPL X
#define UNWRAP_IMPL(...) __VA_ARGS__


namespace detail
{
  /*
    Apply to the given `attr_t` instance a list of attribute setters. Such
    setter is a lambda that sets an individual attribute.
  */

  template<typename... Setters>
  void apply_attr_setters(mysql::attr_t& a, Setters&&... fn)
  {
      if constexpr (sizeof...(fn) > 0)  // ⋯ handles empty pack
          (fn(a), ...);  // C++17 fold
  }

  // Generate lambda for setting given attribute value

  template <mysql::coll_attr_enum A, typename V = unit_t>
  auto attr_setter(V&& v = {})
  {
    return [val = std::forward<V>(v)](mysql::attr_t& a)
    { a.template set<A>(val); };
  }

  namespace attr_setters
  {
    /*
      For each attribute AA define a setter named a_AA that sets this attribute
      to the given value.
    */

    #undef  DEF_SETTER
    #define DEF_SETTER(AA, T, ...) \
      constexpr auto a_##AA = attr_setter<mysql::coll_attr_enum::a_##AA, T>;

    MYSQL_ATTR_LIST(DEF_SETTER)

  } // namespace attr_setters

} // namespace detail


/*
  Uncomment next line to run only the single `t_try` test case that can be used
  for ad-hoc testing.
*/

//#define TRY
#ifdef TRY

DECLARE_TEST(t_try)
{
  bool pass = true;
  CHECK_SAMPLE_U(0, 0, other, bit, ({10}))
  CHECK_SAMPLE_U(0, 0, other, bit, ({}))
  return pass ? OK : FAIL;
}

#endif


DECLARE_TEST(t_vector)
{
  if (!mysql_min_version(hdbc, "9.0.0", 5))
    skip("Test requires at least server version 9.0.0");

  bool pass = true;
  CHECK_SAMPLES(VECTOR)
  return pass ? OK : FAIL;
}


DECLARE_TEST(t_string)
{
  bool pass = true;
  CHECK_SAMPLES(STRING)
  return pass ? OK : FAIL;
}


DECLARE_TEST(t_numeric)
{
  bool pass = true;
  CHECK_SAMPLES(NUMERIC)
  return pass ? OK : FAIL;
}


DECLARE_TEST(t_datetime)
{
  bool pass = true;
  CHECK_SAMPLES(TEMPORAL)
  return pass ? OK : FAIL;
}


DECLARE_TEST(t_spatial)
{
  bool pass = true;
  CHECK_SAMPLES(SPATIAL)
  return pass ? OK : FAIL;
}


DECLARE_TEST(t_other)
{
  bool pass = true;
  CHECK_SAMPLES(OTHER)
  return pass ? OK : FAIL;
}


/*
  Note: ADD_TOFIX() lines below should be changed to ADD_TEST() once tests
  are passing.
*/

BEGIN_TESTS
#ifdef TRY
  ADD_TEST(t_try)
#else
  ADD_TOFIX(t_vector)
  ADD_TOFIX(t_string)
  ADD_TOFIX(t_numeric)
  ADD_TOFIX(t_datetime)
  ADD_TOFIX(t_spatial)
  ADD_TOFIX(t_other)
#endif
END_TESTS

DISABLE_TEST_MODULE

RUN_TESTS