/*
  Copyright (c) 2026 Matrix Origin.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2.0.

  MatrixOne end-to-end ODBC compatibility suite. The suite uses only public
  ODBC APIs so the same executable can test Unicode and ANSI drivers through
  a platform driver manager.
*/

#include <sql.h>
#include <sqlext.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

bool succeeded(SQLRETURN rc) {
  return rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO;
}

struct Diagnostic {
  std::string state;
  SQLINTEGER native_error = 0;
  std::string message;
};

std::vector<Diagnostic> diagnostics(SQLSMALLINT handle_type,
                                    SQLHANDLE handle) {
  std::vector<Diagnostic> result;
  for (SQLSMALLINT record = 1;; ++record) {
    SQLCHAR state[6] = {};
    SQLCHAR message[2048] = {};
    SQLINTEGER native_error = 0;
    SQLSMALLINT message_len = 0;
    SQLRETURN rc = SQLGetDiagRec(handle_type, handle, record, state,
                                 &native_error, message, sizeof(message),
                                 &message_len);
    if (rc == SQL_NO_DATA) break;
    if (!succeeded(rc)) break;
    result.push_back({reinterpret_cast<const char *>(state), native_error,
                      reinterpret_cast<const char *>(message)});
  }
  return result;
}

std::string diagnostic_text(SQLSMALLINT handle_type, SQLHANDLE handle) {
  std::ostringstream out;
  for (const auto &diag : diagnostics(handle_type, handle)) {
    out << " SQLSTATE=" << diag.state << " native=" << diag.native_error
        << " message=" << diag.message;
  }
  return out.str();
}

class Failure : public std::runtime_error {
 public:
  explicit Failure(const std::string &message) : std::runtime_error(message) {}
};

class KnownIssue : public std::runtime_error {
 public:
  explicit KnownIssue(const std::string &message)
      : std::runtime_error(message) {}
};

void expect(bool condition, const std::string &message) {
  if (!condition) throw Failure(message);
}

void check(SQLRETURN rc, SQLSMALLINT handle_type, SQLHANDLE handle,
           const std::string &operation) {
  if (!succeeded(rc)) {
    throw Failure(operation + " rc=" + std::to_string(rc) +
                  diagnostic_text(handle_type, handle));
  }
}

std::vector<SQLWCHAR> widen_ascii(const std::string &text) {
  std::vector<SQLWCHAR> wide(text.size() + 1, 0);
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c > 0x7f) {
      throw Failure("connection string must be ASCII for this test harness");
    }
    wide[i] = static_cast<SQLWCHAR>(c);
  }
  return wide;
}

class Database {
 public:
  explicit Database(const std::string &connection_string) {
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_);
    if (!succeeded(rc)) throw Failure("SQLAllocHandle ENV failed");
    try {
      check(SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION,
                          reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0),
            SQL_HANDLE_ENV, env_, "SQLSetEnvAttr ODBC3");
      check(SQLAllocHandle(SQL_HANDLE_DBC, env_, &dbc_), SQL_HANDLE_ENV, env_,
            "SQLAllocHandle DBC");
      check(SQLSetConnectAttr(dbc_, SQL_ATTR_LOGIN_TIMEOUT,
                              reinterpret_cast<SQLPOINTER>(uintptr_t{10}), 0),
            SQL_HANDLE_DBC, dbc_, "SQL_ATTR_LOGIN_TIMEOUT");

      std::vector<SQLWCHAR> connection = widen_ascii(connection_string);
      SQLWCHAR completed[4096] = {};
      SQLSMALLINT completed_len = 0;
      rc = SQLDriverConnectW(
          dbc_, nullptr, connection.data(), SQL_NTS, completed,
          sizeof(completed) / sizeof(completed[0]), &completed_len,
          SQL_DRIVER_NOPROMPT);
      check(rc, SQL_HANDLE_DBC, dbc_, "SQLDriverConnectW");
      connected_ = true;
    } catch (...) {
      release();
      throw;
    }
  }

  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  ~Database() { release(); }

  SQLHDBC handle() const { return dbc_; }

 private:
  void release() noexcept {
    if (dbc_ != SQL_NULL_HDBC) {
      if (connected_) SQLDisconnect(dbc_);
      SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
      dbc_ = SQL_NULL_HDBC;
      connected_ = false;
    }
    if (env_ != SQL_NULL_HENV) {
      SQLFreeHandle(SQL_HANDLE_ENV, env_);
      env_ = SQL_NULL_HENV;
    }
  }

  SQLHENV env_ = SQL_NULL_HENV;
  SQLHDBC dbc_ = SQL_NULL_HDBC;
  bool connected_ = false;
};

class Statement {
 public:
  explicit Statement(SQLHDBC dbc) {
    check(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt_), SQL_HANDLE_DBC, dbc,
          "SQLAllocHandle STMT");
  }

  Statement(const Statement &) = delete;
  Statement &operator=(const Statement &) = delete;

  ~Statement() {
    if (stmt_ != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, stmt_);
  }

  SQLHSTMT handle() const { return stmt_; }

 private:
  SQLHSTMT stmt_ = SQL_NULL_HSTMT;
};

void execute(SQLHDBC dbc, const std::string &sql) {
  Statement stmt(dbc);
  SQLRETURN rc = SQLExecDirect(
      stmt.handle(), reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
      static_cast<SQLINTEGER>(sql.size()));
  check(rc, SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect: " + sql);
}

class FixtureCleanup {
 public:
  explicit FixtureCleanup(SQLHDBC dbc) : dbc_(dbc) {}
  FixtureCleanup(const FixtureCleanup &) = delete;
  FixtureCleanup &operator=(const FixtureCleanup &) = delete;
  ~FixtureCleanup() {
    try {
      execute(dbc_, "DROP DATABASE IF EXISTS mo_odbc_deep");
    } catch (...) {
    }
  }

 private:
  SQLHDBC dbc_;
};

int64_t scalar_int(SQLHDBC dbc, const std::string &sql) {
  Statement stmt(dbc);
  SQLRETURN rc = SQLExecDirect(
      stmt.handle(), reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
      static_cast<SQLINTEGER>(sql.size()));
  check(rc, SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect scalar: " + sql);
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch scalar");
  SQLBIGINT value = 0;
  SQLLEN indicator = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_SBIGINT, &value, sizeof(value),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData scalar");
  expect(indicator != SQL_NULL_DATA, "scalar query returned NULL: " + sql);
  return static_cast<int64_t>(value);
}

std::string scalar_text(SQLHDBC dbc, const std::string &sql) {
  Statement stmt(dbc);
  SQLRETURN rc = SQLExecDirect(
      stmt.handle(), reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
      static_cast<SQLINTEGER>(sql.size()));
  check(rc, SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect scalar: " + sql);
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch scalar");
  SQLCHAR value[4096] = {};
  SQLLEN indicator = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_CHAR, value, sizeof(value),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData scalar text");
  expect(indicator != SQL_NULL_DATA, "text query returned NULL: " + sql);
  return reinterpret_cast<const char *>(value);
}

std::string info_string(SQLHDBC dbc, SQLUSMALLINT key) {
  SQLCHAR value[1024] = {};
  SQLSMALLINT length = 0;
  check(SQLGetInfo(dbc, key, value, sizeof(value), &length), SQL_HANDLE_DBC,
        dbc, "SQLGetInfo " + std::to_string(key));
  return reinterpret_cast<const char *>(value);
}

SQLLEN count_rows(SQLHSTMT stmt);

void setup_fixture(SQLHDBC dbc) {
  execute(dbc, "DROP DATABASE IF EXISTS mo_odbc_deep");
  execute(dbc, "CREATE DATABASE mo_odbc_deep");
  execute(dbc, R"SQL(
CREATE TABLE mo_odbc_deep.type_matrix (
  id INT PRIMARY KEY,
  c_tiny TINYINT,
  c_small SMALLINT,
  c_int INT,
  c_big BIGINT,
  c_dec DECIMAL(20,6),
  c_float FLOAT,
  c_double DOUBLE,
  c_bool BOOLEAN,
  c_date DATE,
  c_time TIME,
  c_dt DATETIME,
  c_ts TIMESTAMP,
  c_char CHAR(8),
  c_varchar VARCHAR(128),
  c_text TEXT,
  c_binary BINARY(8),
  c_varbinary VARBINARY(128),
  c_blob BLOB,
  c_json JSON
))SQL");
  execute(dbc, R"SQL(
INSERT INTO mo_odbc_deep.type_matrix VALUES
  (1,-8,-32000,-2000000000,-9000000000000,1234567890.123456,1.25,-9.5,
   TRUE,'2026-08-04','12:34:56','2026-08-04 12:34:56',
   '2026-08-04 12:34:56','MO',
   CONVERT(x'e4b88ae6b5b720506f776572204249' USING utf8mb4),
   CONVERT(x'e995bfe69687e69cac' USING utf8mb4),
   x'0001020304050607',x'00ff104d4f',x'000102ff',
   CONVERT(x'7b2263697479223a22e4b88ae6b5b7227d' USING utf8mb4)),
  (2,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
   NULL,NULL,NULL,NULL,NULL))SQL");
  execute(dbc, "CREATE INDEX idx_varchar ON "
               "mo_odbc_deep.type_matrix(c_varchar)");
  execute(dbc, "CREATE VIEW mo_odbc_deep.type_view AS SELECT id,c_varchar "
               "FROM mo_odbc_deep.type_matrix");
  execute(dbc, "CREATE TABLE mo_odbc_deep.meta_under_score (id INT)");
  execute(dbc, "CREATE TABLE mo_odbc_deep.metaXunderXscore (id INT)");
  execute(dbc, R"SQL(
CREATE TABLE mo_odbc_deep.param_values (
  id BIGINT PRIMARY KEY,
  name VARCHAR(128),
  amount DECIMAL(20,6),
  event_date DATE,
  payload VARBINARY(128),
  nullable_value VARCHAR(32)
))SQL");
  execute(dbc, "CREATE TABLE mo_odbc_deep.tx_values (id INT PRIMARY KEY, "
               "value VARCHAR(64))");
  execute(dbc, "CREATE TABLE mo_odbc_deep.long_values (id INT PRIMARY KEY, "
               "payload TEXT, binary_payload BLOB)");
}

void test_connection_capabilities(SQLHDBC dbc) {
  const std::string dbms_name = info_string(dbc, SQL_DBMS_NAME);
  const std::string dbms_version = info_string(dbc, SQL_DBMS_VER);
  const std::string driver_name = info_string(dbc, SQL_DRIVER_NAME);
  const std::string driver_version = info_string(dbc, SQL_DRIVER_VER);
  expect(dbms_name == "MatrixOne",
         "SQL_DBMS_NAME should identify MatrixOne, got: " + dbms_name);
  expect(dbms_version.find("MatrixOne") != std::string::npos,
         "SQL_DBMS_VER does not identify MatrixOne: " + dbms_version);
  expect(!driver_name.empty(), "SQL_DRIVER_NAME is empty");
  expect(!driver_version.empty(), "SQL_DRIVER_VER is empty");

  for (SQLUSMALLINT api : {SQL_API_SQLBINDPARAMETER, SQL_API_SQLTABLES,
                           SQL_API_SQLCOLUMNS, SQL_API_SQLPRIMARYKEYS,
                           SQL_API_SQLSTATISTICS, SQL_API_SQLGETTYPEINFO}) {
    SQLUSMALLINT supported = SQL_FALSE;
    check(SQLGetFunctions(dbc, api, &supported), SQL_HANDLE_DBC, dbc,
          "SQLGetFunctions " + std::to_string(api));
    expect(supported == SQL_TRUE,
           "required ODBC API is not reported as supported: " +
               std::to_string(api));
  }

  SQLUSMALLINT txn_capable = SQL_TC_NONE;
  check(SQLGetInfo(dbc, SQL_TXN_CAPABLE, &txn_capable, sizeof(txn_capable),
                   nullptr),
        SQL_HANDLE_DBC, dbc, "SQLGetInfo SQL_TXN_CAPABLE");
  expect(txn_capable != SQL_TC_NONE,
         "driver reports that MatrixOne is not transaction capable");

  Statement type_info(dbc);
  check(SQLGetTypeInfo(type_info.handle(), SQL_ALL_TYPES), SQL_HANDLE_STMT,
        type_info.handle(), "SQLGetTypeInfo SQL_ALL_TYPES");
  const SQLLEN type_count = count_rows(type_info.handle());
  expect(type_count >= 20,
         "SQLGetTypeInfo returned too few rows: " +
             std::to_string(type_count));

  std::cout << "# DBMS=" << dbms_name << " version=" << dbms_version
            << " driver=" << driver_name << " driver_version="
            << driver_version << std::endl;
}

SQLLEN count_rows(SQLHSTMT stmt) {
  SQLLEN rows = 0;
  SQLRETURN rc;
  while (succeeded(rc = SQLFetch(stmt))) ++rows;
  if (rc != SQL_NO_DATA) {
    check(rc, SQL_HANDLE_STMT, stmt, "SQLFetch metadata");
  }
  return rows;
}

void test_catalog_metadata(SQLHDBC dbc) {
  const bool unicode_driver =
      info_string(dbc, SQL_DRIVER_NAME).find("9w") != std::string::npos;
  {
    Statement stmt(dbc);
    SQLCHAR catalog[] = "mo_odbc_deep";
    SQLCHAR table[] = "type_matrix";
    SQLCHAR type[] = "TABLE";
    check(SQLTables(stmt.handle(), catalog, SQL_NTS, nullptr, 0, table,
                    SQL_NTS, type, SQL_NTS),
          SQL_HANDLE_STMT, stmt.handle(), "SQLTables exact table");
    expect(count_rows(stmt.handle()) == 1,
           "SQLTables exact table did not return exactly one row");
  }
  {
    Statement stmt(dbc);
    SQLCHAR catalog[] = "mo_odbc_deep";
    SQLCHAR view[] = "type_view";
    SQLCHAR type[] = "VIEW";
    check(SQLTables(stmt.handle(), catalog, SQL_NTS, nullptr, 0, view, SQL_NTS,
                    type, SQL_NTS),
          SQL_HANDLE_STMT, stmt.handle(), "SQLTables exact view");
    expect(count_rows(stmt.handle()) == 1,
           "SQLTables exact view did not return exactly one row");
  }
  {
    Statement stmt(dbc);
    SQLCHAR catalog[] = "mo_odbc_deep";
    SQLCHAR table[] = "type_matrix";
    check(SQLColumns(stmt.handle(), catalog, SQL_NTS, nullptr, 0, table,
                     SQL_NTS, nullptr, 0),
          SQL_HANDLE_STMT, stmt.handle(), "SQLColumns type_matrix");
    std::map<std::string, SQLSMALLINT> types;
    SQLRETURN rc;
    while (succeeded(rc = SQLFetch(stmt.handle()))) {
      SQLCHAR name[256] = {};
      SQLCHAR data_type_text[32] = {};
      SQLSMALLINT data_type = 0;
      SQLLEN name_len = 0;
      SQLLEN type_len = 0;
      check(SQLGetData(stmt.handle(), 4, SQL_C_CHAR, name, sizeof(name),
                       &name_len),
            SQL_HANDLE_STMT, stmt.handle(), "SQLColumns COLUMN_NAME");
      check(SQLGetData(stmt.handle(), 5, SQL_C_CHAR, data_type_text,
                       sizeof(data_type_text), &type_len),
            SQL_HANDLE_STMT, stmt.handle(), "SQLColumns DATA_TYPE");
      data_type = static_cast<SQLSMALLINT>(std::stoi(
          reinterpret_cast<const char *>(data_type_text)));
      const std::string column_name(reinterpret_cast<const char *>(name));
      types[column_name] = data_type;
      std::cout << "# SQLColumns name=" << column_name
                << " data_type=" << data_type << std::endl;
    }
    if (rc != SQL_NO_DATA) {
      check(rc, SQL_HANDLE_STMT, stmt.handle(), "SQLColumns fetch");
    }
    expect(types.size() == 20,
           "SQLColumns expected 20 columns, got " +
               std::to_string(types.size()));
    expect(types["id"] == SQL_INTEGER,
           "id is not reported as SQL_INTEGER; got " +
               std::to_string(types["id"]));
    expect(types["c_big"] == SQL_BIGINT,
           "c_big is not reported as SQL_BIGINT");
    expect(types["c_dec"] == SQL_DECIMAL,
           "c_dec is not reported as SQL_DECIMAL");
    expect(types["c_date"] == SQL_TYPE_DATE,
           "c_date is not reported as SQL_TYPE_DATE");
    expect(types["c_time"] == SQL_TYPE_TIME,
           "c_time is not reported as SQL_TYPE_TIME");
    expect(types["c_dt"] == SQL_TYPE_TIMESTAMP,
           "c_dt is not reported as SQL_TYPE_TIMESTAMP");
    expect(types["c_char"] ==
               (unicode_driver ? SQL_WCHAR : SQL_CHAR),
           "c_char has the wrong ANSI/Unicode SQL type; got " +
               std::to_string(types["c_char"]));
    expect(types["c_varchar"] ==
               (unicode_driver ? SQL_WVARCHAR : SQL_VARCHAR),
           "c_varchar has the wrong ANSI/Unicode SQL type; got " +
               std::to_string(types["c_varchar"]));
    expect(types["c_text"] ==
               (unicode_driver ? SQL_WLONGVARCHAR : SQL_LONGVARCHAR),
           "c_text has the wrong ANSI/Unicode SQL type; got " +
               std::to_string(types["c_text"]));
    expect(types["c_varbinary"] == SQL_VARBINARY,
           "c_varbinary is not reported as SQL_VARBINARY");
  }
  {
    Statement stmt(dbc);
    SQLCHAR catalog[] = "mo_odbc_deep";
    SQLCHAR table[] = "type_matrix";
    check(SQLPrimaryKeys(stmt.handle(), catalog, SQL_NTS, nullptr, 0, table,
                         SQL_NTS),
          SQL_HANDLE_STMT, stmt.handle(), "SQLPrimaryKeys");
    check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
          "SQLPrimaryKeys fetch");
    SQLCHAR column[256] = {};
    SQLLEN length = 0;
    check(SQLGetData(stmt.handle(), 4, SQL_C_CHAR, column, sizeof(column),
                     &length),
          SQL_HANDLE_STMT, stmt.handle(), "SQLPrimaryKeys COLUMN_NAME");
    expect(std::string(reinterpret_cast<const char *>(column)) == "id",
           "SQLPrimaryKeys did not report id");
    expect(SQLFetch(stmt.handle()) == SQL_NO_DATA,
           "SQLPrimaryKeys returned more than one key column");
  }
  {
    Statement stmt(dbc);
    SQLCHAR catalog[] = "mo_odbc_deep";
    SQLCHAR table[] = "type_matrix";
    check(SQLStatistics(stmt.handle(), catalog, SQL_NTS, nullptr, 0, table,
                        SQL_NTS, SQL_INDEX_ALL, SQL_QUICK),
          SQL_HANDLE_STMT, stmt.handle(), "SQLStatistics");
    bool found_primary_id = false;
    std::string first_index;
    SQLRETURN rc;
    while (succeeded(rc = SQLFetch(stmt.handle()))) {
      SQLCHAR index_name[256] = {};
      SQLCHAR column_name[256] = {};
      SQLLEN length = 0;
      check(SQLGetData(stmt.handle(), 6, SQL_C_CHAR, index_name,
                       sizeof(index_name), &length),
            SQL_HANDLE_STMT, stmt.handle(), "SQLStatistics INDEX_NAME");
      check(SQLGetData(stmt.handle(), 9, SQL_C_CHAR, column_name,
                       sizeof(column_name), &length),
            SQL_HANDLE_STMT, stmt.handle(), "SQLStatistics COLUMN_NAME");
      if (first_index.empty()) {
        first_index = reinterpret_cast<const char *>(index_name);
      }
      found_primary_id =
          found_primary_id ||
          (std::string(reinterpret_cast<const char *>(index_name)) ==
               "PRIMARY" &&
           std::string(reinterpret_cast<const char *>(column_name)) == "id");
    }
    if (rc != SQL_NO_DATA) {
      check(rc, SQL_HANDLE_STMT, stmt.handle(), "SQLStatistics fetch");
    }
    expect(found_primary_id,
           "SQLStatistics did not report the primary-key index on id");
    expect(first_index == "PRIMARY",
           "SQLStatistics is not sorted according to the ODBC contract; "
           "first index=" + first_index);
  }
  {
    Statement stmt(dbc);
    SQLCHAR catalog[] = "mo_odbc_deep";
    SQLCHAR pattern[] = "meta%";
    SQLCHAR type[] = "TABLE";
    check(SQLTables(stmt.handle(), catalog, SQL_NTS, nullptr, 0, pattern,
                    SQL_NTS, type, SQL_NTS),
          SQL_HANDLE_STMT, stmt.handle(), "SQLTables wildcard");
    expect(count_rows(stmt.handle()) == 2,
           "SQLTables wildcard should return two metadata tables");
  }
}

void test_result_descriptors(SQLHDBC dbc) {
  Statement stmt(dbc);
  const bool unicode_driver =
      info_string(dbc, SQL_DRIVER_NAME).find("9w") != std::string::npos;
  const std::string sql =
      "SELECT id,c_big,c_dec,c_date,c_time,c_dt,c_varchar,c_varbinary "
      "FROM mo_odbc_deep.type_matrix ORDER BY id";
  check(SQLExecDirect(stmt.handle(),
                      reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
                      static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect descriptor query");
  SQLSMALLINT count = 0;
  check(SQLNumResultCols(stmt.handle(), &count), SQL_HANDLE_STMT, stmt.handle(),
        "SQLNumResultCols");
  expect(count == 8, "descriptor query expected 8 result columns");

  const SQLSMALLINT expected_types[] = {
      SQL_INTEGER, SQL_BIGINT, SQL_DECIMAL, SQL_TYPE_DATE,
      SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP,
      static_cast<SQLSMALLINT>(unicode_driver ? SQL_WVARCHAR : SQL_VARCHAR),
      SQL_VARBINARY};
  const SQLULEN expected_sizes[] = {10, 19, 20, 10, 8, 19, 128, 128};
  std::vector<std::string> known_size_mismatches;
  for (SQLUSMALLINT column = 1; column <= 8; ++column) {
    SQLCHAR name[256] = {};
    SQLSMALLINT name_len = 0;
    SQLSMALLINT data_type = 0;
    SQLULEN column_size = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;
    check(SQLDescribeCol(stmt.handle(), column, name, sizeof(name), &name_len,
                         &data_type, &column_size, &decimal_digits, &nullable),
          SQL_HANDLE_STMT, stmt.handle(), "SQLDescribeCol");
    expect(data_type == expected_types[column - 1],
           "unexpected descriptor type for column " +
               std::string(reinterpret_cast<const char *>(name)) + ": " +
               std::to_string(data_type));
    if (column_size != expected_sizes[column - 1]) {
      const std::string column_name(
          reinterpret_cast<const char *>(name));
      const bool known_matrixone_signature =
          (column_name == "c_varchar" && column_size == UINT32_MAX) ||
          (column_name == "c_varbinary" && column_size == 384);
      const std::string mismatch =
          column_name + " expected=" +
          std::to_string(expected_sizes[column - 1]) + " actual=" +
          std::to_string(column_size);
      if (!known_matrixone_signature) {
        throw Failure("unexpected descriptor size: " + mismatch);
      }
      known_size_mismatches.push_back(mismatch);
    }
  }
  if (!known_size_mismatches.empty()) {
    std::ostringstream message;
    message << "matrixorigin/matrixone#26683: COM_QUERY reports invalid "
               "declared lengths";
    for (const auto &mismatch : known_size_mismatches) {
      message << "; " << mismatch;
    }
    throw KnownIssue(message.str());
  }
}

void test_type_roundtrip(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql =
      "SELECT id,c_big,c_dec,c_bool,c_date,c_time,c_dt,c_varchar,c_binary,"
      "c_varbinary,c_blob,c_json FROM mo_odbc_deep.type_matrix ORDER BY id";
  check(SQLExecDirect(stmt.handle(),
                      reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
                      static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect type roundtrip");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch type row");

  SQLBIGINT id = 0;
  SQLBIGINT big = 0;
  SQLINTEGER boolean_value = 0;
  SQLLEN indicator = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_SBIGINT, &id, sizeof(id), &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData id");
  check(SQLGetData(stmt.handle(), 2, SQL_C_SBIGINT, &big, sizeof(big),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData bigint");
  expect(id == 1 && big == -9000000000000LL,
         "integer roundtrip mismatch");

  SQLCHAR decimal[64] = {};
  check(SQLGetData(stmt.handle(), 3, SQL_C_CHAR, decimal, sizeof(decimal),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData decimal");
  expect(std::string(reinterpret_cast<const char *>(decimal)) ==
             "1234567890.123456",
         "decimal roundtrip mismatch");
  check(SQLGetData(stmt.handle(), 4, SQL_C_SLONG, &boolean_value,
                   sizeof(boolean_value), &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData boolean");
  expect(boolean_value == 1, "boolean roundtrip mismatch");

  SQL_DATE_STRUCT date = {};
  SQL_TIME_STRUCT time = {};
  SQL_TIMESTAMP_STRUCT timestamp = {};
  check(SQLGetData(stmt.handle(), 5, SQL_C_TYPE_DATE, &date, sizeof(date),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData date");
  check(SQLGetData(stmt.handle(), 6, SQL_C_TYPE_TIME, &time, sizeof(time),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData time");
  check(SQLGetData(stmt.handle(), 7, SQL_C_TYPE_TIMESTAMP, &timestamp,
                   sizeof(timestamp), &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData timestamp");
  expect(date.year == 2026 && date.month == 8 && date.day == 4,
         "date roundtrip mismatch");
  expect(time.hour == 12 && time.minute == 34 && time.second == 56,
         "time roundtrip mismatch");
  expect(timestamp.year == 2026 && timestamp.month == 8 &&
             timestamp.day == 4 && timestamp.hour == 12,
         "timestamp roundtrip mismatch");

  SQLWCHAR wide[128] = {};
  check(SQLGetData(stmt.handle(), 8, SQL_C_WCHAR, wide, sizeof(wide),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData Unicode");
  expect(wide[0] == 0x4e0a && wide[1] == 0x6d77,
         "Unicode roundtrip mismatch");

  unsigned char binary[128] = {};
  check(SQLGetData(stmt.handle(), 9, SQL_C_BINARY, binary, sizeof(binary),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData BINARY");
  expect(indicator == 8 && binary[0] == 0 && binary[7] == 7,
         "BINARY roundtrip mismatch");
  std::memset(binary, 0, sizeof(binary));
  check(SQLGetData(stmt.handle(), 10, SQL_C_BINARY, binary, sizeof(binary),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData VARBINARY");
  expect(indicator == 5 && binary[0] == 0 && binary[1] == 0xff &&
             binary[4] == 0x4f,
         "VARBINARY roundtrip mismatch");
  std::memset(binary, 0, sizeof(binary));
  check(SQLGetData(stmt.handle(), 11, SQL_C_BINARY, binary, sizeof(binary),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData BLOB");
  expect(indicator == 4 && binary[3] == 0xff, "BLOB roundtrip mismatch");

  SQLCHAR json[256] = {};
  check(SQLGetData(stmt.handle(), 12, SQL_C_CHAR, json, sizeof(json),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData JSON");
  std::string json_text(reinterpret_cast<const char *>(json));
  expect(json_text.find("上海") != std::string::npos,
         "JSON Unicode roundtrip mismatch: " + json_text);

  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch NULL row");
  SQLCHAR nullable[32] = {};
  check(SQLGetData(stmt.handle(), 8, SQL_C_CHAR, nullable, sizeof(nullable),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData NULL");
  expect(indicator == SQL_NULL_DATA, "NULL indicator was not SQL_NULL_DATA");
  expect(SQLFetch(stmt.handle()) == SQL_NO_DATA,
         "type roundtrip returned an unexpected third row");
}

void test_prepared_parameters(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql =
      "INSERT INTO mo_odbc_deep.param_values VALUES (?,?,?,?,?,?)";
  check(SQLPrepare(stmt.handle(),
                   reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
                   static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare parameter insert");
  SQLSMALLINT parameter_count = 0;
  check(SQLNumParams(stmt.handle(), &parameter_count), SQL_HANDLE_STMT,
        stmt.handle(), "SQLNumParams");
  expect(parameter_count == 6,
         "SQLNumParams expected 6, got " + std::to_string(parameter_count));

  SQLBIGINT id = 7;
  SQLLEN id_len = 0;
  SQLWCHAR name[] = {0x53c2, 0x6570, 0x4e0a, 0x6d77, 0};
  SQLLEN name_len = SQL_NTS;
  SQLCHAR amount[] = "88.120000";
  SQLLEN amount_len = SQL_NTS;
  SQL_DATE_STRUCT date = {2026, 8, 5};
  SQLLEN date_len = sizeof(date);
  unsigned char payload[] = {0, 1, 0xfe, 0xff};
  SQLLEN payload_len = sizeof(payload);
  SQLLEN null_len = SQL_NULL_DATA;

  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                         SQL_BIGINT, 0, 0, &id, 0, &id_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter id");
  check(SQLBindParameter(stmt.handle(), 2, SQL_PARAM_INPUT, SQL_C_WCHAR,
                         SQL_WVARCHAR, 128, 0, name, sizeof(name), &name_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter Unicode name");
  check(SQLBindParameter(stmt.handle(), 3, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_DECIMAL, 20, 6, amount, sizeof(amount),
                         &amount_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter decimal");
  check(SQLBindParameter(stmt.handle(), 4, SQL_PARAM_INPUT, SQL_C_TYPE_DATE,
                         SQL_TYPE_DATE, 10, 0, &date, sizeof(date), &date_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter date");
  check(SQLBindParameter(stmt.handle(), 5, SQL_PARAM_INPUT, SQL_C_BINARY,
                         SQL_VARBINARY, sizeof(payload), 0, payload,
                         sizeof(payload), &payload_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter binary");
  check(SQLBindParameter(stmt.handle(), 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_VARCHAR, 32, 0, nullptr, 0, &null_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter NULL");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute parameter insert");

  expect(scalar_text(dbc,
                     "SELECT name FROM mo_odbc_deep.param_values WHERE id=7") ==
             "参数上海",
         "prepared Unicode parameter roundtrip mismatch");
  expect(scalar_text(dbc,
                     "SELECT amount FROM mo_odbc_deep.param_values WHERE id=7") ==
             "88.120000",
         "prepared decimal parameter roundtrip mismatch");
  expect(scalar_text(dbc,
                     "SELECT hex(payload) FROM mo_odbc_deep.param_values "
                     "WHERE id=7") == "0001FEFF",
         "prepared binary parameter roundtrip mismatch");
  expect(scalar_int(dbc,
                    "SELECT nullable_value IS NULL FROM "
                    "mo_odbc_deep.param_values WHERE id=7") == 1,
         "prepared NULL parameter roundtrip mismatch");

  Statement select_stmt(dbc);
  const std::string select_sql =
      "SELECT name FROM mo_odbc_deep.param_values WHERE id=?";
  check(SQLPrepare(
            select_stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(select_sql.data())),
            static_cast<SQLINTEGER>(select_sql.size())),
        SQL_HANDLE_STMT, select_stmt.handle(), "SQLPrepare parameter select");
  check(SQLBindParameter(select_stmt.handle(), 1, SQL_PARAM_INPUT,
                         SQL_C_SBIGINT, SQL_BIGINT, 0, 0, &id, 0, &id_len),
        SQL_HANDLE_STMT, select_stmt.handle(), "SQLBindParameter select id");
  check(SQLExecute(select_stmt.handle()), SQL_HANDLE_STMT, select_stmt.handle(),
        "SQLExecute parameter select");
  check(SQLFetch(select_stmt.handle()), SQL_HANDLE_STMT, select_stmt.handle(),
        "SQLFetch parameter select");
}

void test_prepared_floating_point(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql =
      "SELECT c_float,c_double FROM mo_odbc_deep.type_matrix WHERE id=1";
  check(SQLPrepare(stmt.handle(),
                   reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
                   static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare floating-point select");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute floating-point select");

  double float_value = 0;
  double double_value = 0;
  SQLLEN float_len = 0;
  SQLLEN double_len = 0;
  check(SQLBindCol(stmt.handle(), 1, SQL_C_DOUBLE, &float_value,
                   sizeof(float_value), &float_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindCol FLOAT as double");
  check(SQLBindCol(stmt.handle(), 2, SQL_C_DOUBLE, &double_value,
                   sizeof(double_value), &double_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindCol DOUBLE");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch floating-point row");
  expect(std::fabs(float_value - 1.25) < 0.000001,
         "prepared FLOAT lost precision: " + std::to_string(float_value));
  expect(std::fabs(double_value - (-9.5)) < 0.000000001,
         "prepared DOUBLE lost precision: " + std::to_string(double_value));
}

void test_prepared_boolean(SQLHDBC dbc) {
  execute(dbc, "DROP TABLE IF EXISTS mo_odbc_deep.bool_params");
  execute(dbc,
          "CREATE TABLE mo_odbc_deep.bool_params "
          "(id INT PRIMARY KEY, enabled BOOL)");
  execute(dbc, "INSERT INTO mo_odbc_deep.bool_params VALUES (1, TRUE)");

  Statement stmt(dbc);
  const std::string sql =
      "UPDATE mo_odbc_deep.bool_params SET enabled=? "
      "WHERE id=? AND enabled=?";
  check(SQLPrepare(stmt.handle(),
                   reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
                   static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare boolean update");

  SQLCHAR disabled = 0;
  SQLCHAR enabled = 1;
  SQLINTEGER id = 1;
  SQLLEN value_len = 0;
  SQLLEN id_len = 0;
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_BIT,
                         SQL_BIT, 1, 0, &disabled, 0, &value_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter boolean value");
  check(SQLBindParameter(stmt.handle(), 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &id, 0, &id_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter boolean id");
  check(SQLBindParameter(stmt.handle(), 3, SQL_PARAM_INPUT, SQL_C_BIT,
                         SQL_BIT, 1, 0, &enabled, 0, &value_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter boolean predicate");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute boolean update");
  expect(scalar_int(dbc,
                    "SELECT enabled FROM mo_odbc_deep.bool_params WHERE id=1") ==
             0,
         "prepared SQL_BIT update did not persist FALSE");
}

void test_varbinary_wide_conversion(SQLHDBC dbc) {
  execute(dbc, "DROP TABLE IF EXISTS mo_odbc_deep.binary_wide");
  execute(dbc,
          "CREATE TABLE mo_odbc_deep.binary_wide "
          "(id INT PRIMARY KEY, payload VARBINARY(32))");
  execute(dbc,
          "INSERT INTO mo_odbc_deep.binary_wide VALUES (1, 0xabcdef)");

  Statement stmt(dbc);
  const std::string sql =
      "SELECT payload FROM mo_odbc_deep.binary_wide WHERE id=1";
  check(SQLExecDirect(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect VARBINARY select");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch VARBINARY row");

  SQLWCHAR value[16] = {};
  SQLLEN value_len = 0;
  SQLRETURN rc = SQLGetData(stmt.handle(), 1, SQL_C_WCHAR, value,
                            sizeof(value), &value_len);
  if (!succeeded(rc)) {
    const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
    if (!records.empty() && records.front().state == "HY000" &&
        records.front().message.find(
            "Unknown failure when converting character from server character "
            "set") != std::string::npos) {
      throw KnownIssue(
          "matrixorigin/matrixone#26716: VARBINARY result metadata omits "
          "BINARY_FLAG, so SQL_C_WCHAR conversion fails");
    }
    check(rc, SQL_HANDLE_STMT, stmt.handle(),
          "SQLGetData VARBINARY as SQL_C_WCHAR");
  }
  const SQLWCHAR expected[] = {'A', 'B', 'C', 'D', 'E', 'F', 0};
  expect(std::equal(std::begin(expected), std::end(expected),
                    std::begin(value)),
         "VARBINARY SQL_C_WCHAR conversion did not return ABCDEF");
}

void test_unquoted_unicode_identifier(SQLHDBC dbc) {
  std::vector<SQLWCHAR> sql =
      widen_ascii("CREATE TABLE mo_odbc_deep.t_");
  sql.pop_back();
  sql.push_back(0x00e3);
  const std::string suffix = "g (id INT)";
  for (unsigned char c : suffix) sql.push_back(static_cast<SQLWCHAR>(c));
  sql.push_back(0);

  Statement stmt(dbc);
  SQLRETURN rc = SQLExecDirectW(stmt.handle(), sql.data(), SQL_NTS);
  if (!succeeded(rc)) {
    const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
    if (!records.empty() && records.front().state == "42000" &&
        records.front().native_error == 1064) {
      throw KnownIssue(
          "matrixorigin/matrixone#26715: the MySQL-compatible parser rejects "
          "valid unquoted Unicode identifiers");
    }
    check(rc, SQL_HANDLE_STMT, stmt.handle(),
          "SQLExecDirectW unquoted Unicode identifier");
  }
}

void test_transactions(SQLHDBC dbc) {
  execute(dbc, "DELETE FROM mo_odbc_deep.tx_values");
  check(SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
                          reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0),
        SQL_HANDLE_DBC, dbc, "disable autocommit");
  try {
    execute(dbc, "INSERT INTO mo_odbc_deep.tx_values VALUES (1,'rollback')");
    check(SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_ROLLBACK), SQL_HANDLE_DBC, dbc,
          "SQLEndTran rollback");
    expect(scalar_int(dbc, "SELECT COUNT(*) FROM mo_odbc_deep.tx_values") == 0,
           "rollback left a visible row");

    execute(dbc, "INSERT INTO mo_odbc_deep.tx_values VALUES (2,'commit')");
    check(SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT), SQL_HANDLE_DBC, dbc,
          "SQLEndTran commit");
    expect(scalar_int(dbc, "SELECT COUNT(*) FROM mo_odbc_deep.tx_values") == 1,
           "commit did not persist one row");
  } catch (...) {
    SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_ROLLBACK);
    SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
                      reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0);
    throw;
  }
  check(SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
                          reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0),
        SQL_HANDLE_DBC, dbc, "enable autocommit");
}

void test_transaction_isolation_known_issue(SQLHDBC dbc) {
  check(SQLSetConnectAttr(
            dbc, SQL_ATTR_TXN_ISOLATION,
            reinterpret_cast<SQLPOINTER>(uintptr_t{SQL_TXN_READ_UNCOMMITTED}),
            0),
        SQL_HANDLE_DBC, dbc, "SQL_ATTR_TXN_ISOLATION READ_UNCOMMITTED");
  std::string actual;
  try {
    actual = scalar_text(dbc, "SELECT @@transaction_isolation");
  } catch (...) {
    SQLSetConnectAttr(
        dbc, SQL_ATTR_TXN_ISOLATION,
        reinterpret_cast<SQLPOINTER>(uintptr_t{SQL_TXN_REPEATABLE_READ}), 0);
    throw;
  }
  check(SQLSetConnectAttr(
            dbc, SQL_ATTR_TXN_ISOLATION,
            reinterpret_cast<SQLPOINTER>(uintptr_t{SQL_TXN_REPEATABLE_READ}),
            0),
        SQL_HANDLE_DBC, dbc, "restore SQL_ATTR_TXN_ISOLATION");
  if (actual == "READ-UNCOMMITTED") return;
  if (actual == "REPEATABLE-READ") {
    throw KnownIssue(
        "matrixorigin/matrixone#26648: SET SESSION TRANSACTION ISOLATION "
        "LEVEL is accepted but ignored; actual=" + actual);
  }
  throw Failure("unexpected transaction isolation after setter: " + actual);
}

void test_streaming_and_chunked_getdata(SQLHDBC dbc) {
  const size_t text_size = 64 * 1024;
  std::string text;
  text.reserve(text_size);
  const std::string pattern = "MatrixOne-ODBC-";
  while (text.size() < text_size) text += pattern;
  text.resize(text_size);
  const std::vector<unsigned char> binary = {0, 1, 2, 3, 0xfc, 0xfd, 0xfe,
                                              0xff};

  Statement stmt(dbc);
  const std::string sql =
      "INSERT INTO mo_odbc_deep.long_values VALUES (?,?,?)";
  check(SQLPrepare(stmt.handle(),
                   reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
                   static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare data-at-execution insert");
  SQLINTEGER id = 1;
  SQLLEN id_len = 0;
  char text_token = 0;
  char binary_token = 0;
  SQLLEN text_len = SQL_LEN_DATA_AT_EXEC(static_cast<SQLLEN>(text.size()));
  SQLLEN binary_len =
      SQL_LEN_DATA_AT_EXEC(static_cast<SQLLEN>(binary.size()));
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &id, 0, &id_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter stream id");
  check(SQLBindParameter(stmt.handle(), 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_LONGVARCHAR, text.size(), 0, &text_token, 0,
                         &text_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter stream text");
  check(SQLBindParameter(stmt.handle(), 3, SQL_PARAM_INPUT, SQL_C_BINARY,
                         SQL_LONGVARBINARY, binary.size(), 0, &binary_token, 0,
                         &binary_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter stream binary");

  SQLRETURN rc = SQLExecute(stmt.handle());
  expect(rc == SQL_NEED_DATA,
         "SQLExecute data-at-execution did not return SQL_NEED_DATA");
  SQLPOINTER token = nullptr;
  while ((rc = SQLParamData(stmt.handle(), &token)) == SQL_NEED_DATA) {
    if (token == &text_token) {
      const size_t chunk_size = 4096;
      for (size_t offset = 0; offset < text.size(); offset += chunk_size) {
        size_t length = std::min(chunk_size, text.size() - offset);
        check(SQLPutData(stmt.handle(),
                         const_cast<char *>(text.data() + offset),
                         static_cast<SQLLEN>(length)),
              SQL_HANDLE_STMT, stmt.handle(), "SQLPutData text");
      }
    } else if (token == &binary_token) {
      check(SQLPutData(stmt.handle(),
                       const_cast<unsigned char *>(binary.data()),
                       static_cast<SQLLEN>(binary.size())),
            SQL_HANDLE_STMT, stmt.handle(), "SQLPutData binary");
    } else {
      throw Failure("SQLParamData returned an unknown parameter token");
    }
  }
  check(rc, SQL_HANDLE_STMT, stmt.handle(), "SQLParamData completion");

  expect(scalar_int(dbc,
                    "SELECT length(payload) FROM mo_odbc_deep.long_values "
                    "WHERE id=1") == static_cast<int64_t>(text.size()),
         "data-at-execution text length mismatch");
  expect(scalar_text(dbc,
                     "SELECT hex(binary_payload) FROM "
                     "mo_odbc_deep.long_values WHERE id=1") ==
             "00010203FCFDFEFF",
         "data-at-execution binary mismatch");

  Statement read_stmt(dbc);
  const std::string read_sql =
      "SELECT payload FROM mo_odbc_deep.long_values WHERE id=1";
  check(SQLExecDirect(
            read_stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(read_sql.data())),
            static_cast<SQLINTEGER>(read_sql.size())),
        SQL_HANDLE_STMT, read_stmt.handle(), "SQLExecDirect long read");
  check(SQLFetch(read_stmt.handle()), SQL_HANDLE_STMT, read_stmt.handle(),
        "SQLFetch long read");
  std::string fetched;
  for (;;) {
    SQLCHAR chunk[257] = {};
    SQLLEN indicator = 0;
    rc = SQLGetData(read_stmt.handle(), 1, SQL_C_CHAR, chunk, sizeof(chunk),
                    &indicator);
    if (rc == SQL_NO_DATA) break;
    if (!succeeded(rc)) {
      check(rc, SQL_HANDLE_STMT, read_stmt.handle(), "SQLGetData chunk");
    }
    fetched.append(reinterpret_cast<const char *>(chunk));
    expect(fetched.size() <= text.size(),
           "chunked SQLGetData exceeded the expected value size");
    if (rc == SQL_SUCCESS) break;
  }
  expect(fetched == text,
         "chunked SQLGetData content mismatch: expected " +
             std::to_string(text.size()) + " bytes, got " +
             std::to_string(fetched.size()));
}

Diagnostic expect_error(SQLHDBC dbc, const std::string &sql) {
  Statement stmt(dbc);
  SQLRETURN rc = SQLExecDirect(
      stmt.handle(), reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
      static_cast<SQLINTEGER>(sql.size()));
  expect(rc == SQL_ERROR, "statement unexpectedly succeeded: " + sql);
  auto diags = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  expect(!diags.empty(), "statement error has no ODBC diagnostics: " + sql);
  return diags.front();
}

void expect_error_class(SQLHDBC dbc, const std::string &sql,
                        const std::string &expected_class) {
  const Diagnostic diag = expect_error(dbc, sql);
  expect(diag.state.compare(0, expected_class.size(), expected_class) == 0,
         "expected SQLSTATE class " + expected_class + ", got " +
             diag.state + " native=" + std::to_string(diag.native_error) +
             " message=" + diag.message);
}

void test_sqlstates(SQLHDBC dbc) {
  expect_error_class(dbc, "SELEC 1", "42");
  expect_error_class(dbc,
                     "INSERT INTO mo_odbc_deep.type_matrix(id) VALUES (1)",
                     "23");
  const Diagnostic missing = expect_error(
      dbc, "SELECT * FROM mo_odbc_deep.table_that_does_not_exist");
  if (missing.state == "42000" && missing.native_error == 1064) {
    throw KnownIssue(
        "matrixorigin/matrixone#26684: missing table should be SQLSTATE "
        "42S02; actual=" +
        missing.state + " native=" + std::to_string(missing.native_error));
  }
  expect(missing.state == "42S02",
         "unexpected missing-table diagnostic: state=" + missing.state +
             " native=" + std::to_string(missing.native_error));
}

void test_concurrent_connections(const std::string &connection_string) {
  const int thread_count = 6;
  const int iterations = 12;
  std::atomic<int> failures{0};
  std::mutex messages_mutex;
  std::vector<std::string> messages;
  std::vector<std::thread> threads;
  for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
    threads.emplace_back([&, thread_id] {
      try {
        Database db(connection_string);
        for (int i = 0; i < iterations; ++i) {
          int64_t count = scalar_int(
              db.handle(), "SELECT COUNT(*) FROM mo_odbc_deep.type_matrix");
          if (count != 2) {
            throw Failure("concurrent query returned " +
                          std::to_string(count) + " rows");
          }
        }
      } catch (const std::exception &error) {
        ++failures;
        std::lock_guard<std::mutex> guard(messages_mutex);
        messages.push_back("thread " + std::to_string(thread_id) + ": " +
                           error.what());
      }
    });
  }
  for (auto &thread : threads) thread.join();
  if (failures != 0) {
    std::ostringstream out;
    out << failures << " concurrent workers failed";
    for (const auto &message : messages) out << "; " << message;
    throw Failure(out.str());
  }
}

void test_query_timeout_known_issue(SQLHDBC dbc) {
  Statement stmt(dbc);
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_QUERY_TIMEOUT,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{1}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_QUERY_TIMEOUT=1");
  const auto start = std::chrono::steady_clock::now();
  const std::string sql = "SELECT sleep(2)";
  SQLRETURN rc = SQLExecDirect(
      stmt.handle(), reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
      static_cast<SQLINTEGER>(sql.size()));
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  const std::string execute_diagnostics =
      rc == SQL_ERROR ? diagnostic_text(SQL_HANDLE_STMT, stmt.handle()) : "";
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_QUERY_TIMEOUT,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{0}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "restore SQL_ATTR_QUERY_TIMEOUT");
  if (rc == SQL_ERROR && seconds >= 0.5 && seconds < 1.8) return;
  if (succeeded(rc) && seconds >= 1.8) {
    throw KnownIssue(
        "matrixorigin/matrixone#26678: max_execution_time is accepted but not "
        "enforced; rc=" +
        std::to_string(rc) + " elapsed=" + std::to_string(seconds) + "s");
  }
  throw Failure("unexpected query-timeout behavior: rc=" +
                std::to_string(rc) + " elapsed=" +
                std::to_string(seconds) + "s" + execute_diagnostics);
}

void test_cancel(const std::string &connection_string) {
  Database db(connection_string);
  Statement stmt(db.handle());
  std::atomic<int> execute_rc{SQL_STILL_EXECUTING};
  const auto start = std::chrono::steady_clock::now();
  std::thread query([&] {
    const std::string sql = "SELECT sleep(5)";
    execute_rc.store(SQLExecDirect(
        stmt.handle(),
        reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
        static_cast<SQLINTEGER>(sql.size())));
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  SQLRETURN cancel_rc = SQLCancel(stmt.handle());
  query.join();
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  if (!succeeded(cancel_rc) || execute_rc.load() != SQL_ERROR || seconds > 4.0) {
    throw Failure("SQLCancel did not interrupt SELECT sleep(5): cancel_rc=" +
                  std::to_string(cancel_rc) + " execute_rc=" +
                  std::to_string(execute_rc.load()) + " elapsed=" +
                  std::to_string(seconds) + "s" +
                  diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
  }
}

struct TestCase {
  std::string name;
  std::function<void()> run;
};

}  // namespace

int main() {
  const char *raw_connection_string =
      std::getenv("MO_ODBC_CONNECTION_STRING");
  if (!raw_connection_string || !*raw_connection_string) {
    std::cerr << "MO_ODBC_CONNECTION_STRING is required" << std::endl;
    return 2;
  }
  const std::string connection_string(raw_connection_string);

  int passed = 0;
  int failed = 0;
  int xfailed = 0;
  try {
    Database db(connection_string);
    FixtureCleanup fixture_cleanup(db.handle());
    setup_fixture(db.handle());
    std::vector<TestCase> tests = {
        {"connection capabilities",
         [&] { test_connection_capabilities(db.handle()); }},
        {"catalog metadata", [&] { test_catalog_metadata(db.handle()); }},
        {"result descriptors", [&] { test_result_descriptors(db.handle()); }},
        {"type roundtrip", [&] { test_type_roundtrip(db.handle()); }},
        {"prepared parameters",
         [&] { test_prepared_parameters(db.handle()); }},
        {"prepared floating-point precision",
         [&] { test_prepared_floating_point(db.handle()); }},
        {"prepared boolean parameters",
         [&] { test_prepared_boolean(db.handle()); }},
        {"VARBINARY wide conversion",
         [&] { test_varbinary_wide_conversion(db.handle()); }},
        {"unquoted Unicode identifier",
         [&] { test_unquoted_unicode_identifier(db.handle()); }},
        {"transactions", [&] { test_transactions(db.handle()); }},
        {"transaction isolation",
         [&] { test_transaction_isolation_known_issue(db.handle()); }},
        {"streaming and chunked SQLGetData",
         [&] { test_streaming_and_chunked_getdata(db.handle()); }},
        {"SQLSTATE classes", [&] { test_sqlstates(db.handle()); }},
        {"concurrent connections",
         [&] { test_concurrent_connections(connection_string); }},
        {"query timeout", [&] { test_query_timeout_known_issue(db.handle()); }},
        {"statement cancellation", [&] { test_cancel(connection_string); }},
    };

    std::cout << "1.." << tests.size() << std::endl;
    for (size_t i = 0; i < tests.size(); ++i) {
      try {
        tests[i].run();
        ++passed;
        std::cout << "ok " << (i + 1) << " - " << tests[i].name
                  << std::endl;
      } catch (const KnownIssue &issue) {
        ++xfailed;
        std::cout << "ok " << (i + 1) << " - " << tests[i].name
                  << " # XFAIL " << issue.what() << std::endl;
      } catch (const std::exception &error) {
        ++failed;
        std::cout << "not ok " << (i + 1) << " - " << tests[i].name
                  << std::endl;
        std::cerr << "# " << error.what() << std::endl;
      }
    }
  } catch (const std::exception &error) {
    std::cerr << "Bail out! fixture or connection failure: " << error.what()
              << std::endl;
    return 1;
  }

  std::cout << "# summary passed=" << passed << " xfailed=" << xfailed
            << " failed=" << failed << std::endl;
  return failed == 0 ? 0 : 1;
}
