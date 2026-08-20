/*
  Copyright (c) 2026 Matrix Origin.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2.0.

  MatrixOne end-to-end ODBC compatibility suite. The suite uses only public
  ODBC APIs so the same executable can test Unicode and ANSI drivers through
  a platform driver manager.
*/

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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
#include <iterator>
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
  execute(dbc, "CREATE TABLE mo_odbc_deep.fk_parent (id INT PRIMARY KEY)");
  execute(dbc, R"SQL(
CREATE TABLE mo_odbc_deep.fk_child (
  id INT PRIMARY KEY,
  parent_id INT,
  CONSTRAINT fk_deep_parent FOREIGN KEY (parent_id)
    REFERENCES mo_odbc_deep.fk_parent(id)
))SQL");
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
  execute(dbc, R"SQL(
CREATE TABLE mo_odbc_deep.pbi_sales (
  id INT PRIMARY KEY,
  category VARCHAR(32),
  amount DECIMAL(12,2),
  event_time DATETIME(6),
  nullable_note VARCHAR(64),
  active BOOL
))SQL");
  execute(dbc, R"SQL(
INSERT INTO mo_odbc_deep.pbi_sales VALUES
  (1,'A',10.00,'2026-01-02 00:00:00.000001','keep',TRUE),
  (2,'A',20.00,'2026-01-03 00:00:00.000002',NULL,TRUE),
  (3,'B',30.00,'2026-01-04 00:00:00.000003','keep',TRUE),
  (4,'B',100.00,'2025-12-31 23:59:59.999999','keep',TRUE),
  (5,'C',40.00,'2026-01-05 00:00:00.000004','keep',FALSE)
)SQL");
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

  for (SQLUSMALLINT api : {SQL_API_SQLBINDPARAMETER, SQL_API_SQLDESCRIBEPARAM,
                           SQL_API_SQLNUMPARAMS, SQL_API_SQLTABLES,
                           SQL_API_SQLCOLUMNS, SQL_API_SQLPRIMARYKEYS,
                           SQL_API_SQLFOREIGNKEYS, SQL_API_SQLSTATISTICS,
                           SQL_API_SQLGETTYPEINFO}) {
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
    expect(types["c_bool"] == SQL_BIT,
           "c_bool is not reported as SQL_BIT; got " +
               std::to_string(types["c_bool"]));
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
    const std::string sql =
        "SELECT c_tiny,c_bool FROM mo_odbc_deep.type_matrix LIMIT 1";
    check(SQLExecDirect(
              stmt.handle(),
              reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
              static_cast<SQLINTEGER>(sql.size())),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLExecDirect TINYINT and BOOL descriptors");
    SQLSMALLINT tiny_type = 0;
    SQLSMALLINT bool_type = 0;
    check(SQLDescribeCol(stmt.handle(), 1, nullptr, 0, nullptr, &tiny_type,
                         nullptr, nullptr, nullptr),
          SQL_HANDLE_STMT, stmt.handle(), "SQLDescribeCol TINYINT");
    check(SQLDescribeCol(stmt.handle(), 2, nullptr, 0, nullptr, &bool_type,
                         nullptr, nullptr, nullptr),
          SQL_HANDLE_STMT, stmt.handle(), "SQLDescribeCol BOOL");
    expect(tiny_type == SQL_TINYINT,
           "TINYINT result descriptor is not SQL_TINYINT");
    expect(bool_type == SQL_BIT,
           "BOOL result descriptor is not SQL_BIT; got " +
               std::to_string(bool_type));
  }
  {
    Statement stmt(dbc);
    SQLCHAR catalog[] = "mo_odbc_deep";
    SQLCHAR table[] = "metaxunderxscore";
    check(SQLColumns(stmt.handle(), catalog, SQL_NTS, nullptr, 0, table,
                     SQL_NTS, nullptr, 0),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLColumns table without primary key");
    std::vector<std::string> columns;
    SQLRETURN rc;
    while (succeeded(rc = SQLFetch(stmt.handle()))) {
      SQLCHAR column_name[256] = {};
      SQLLEN length = 0;
      check(SQLGetData(stmt.handle(), 4, SQL_C_CHAR, column_name,
                       sizeof(column_name), &length),
            SQL_HANDLE_STMT, stmt.handle(),
            "SQLColumns table without primary key COLUMN_NAME");
      columns.emplace_back(reinterpret_cast<const char *>(column_name));
    }
    if (rc != SQL_NO_DATA) {
      check(rc, SQL_HANDLE_STMT, stmt.handle(),
            "SQLColumns table without primary key fetch");
    }
    expect(columns == std::vector<std::string>{"id"},
           "SQLColumns exposed unexpected columns on a table without a "
           "primary key");
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
    SQLCHAR table[] = "fk_child";
    check(SQLForeignKeys(stmt.handle(), nullptr, 0, nullptr, 0, nullptr, 0,
                         catalog, SQL_NTS, nullptr, 0, table, SQL_NTS),
          SQL_HANDLE_STMT, stmt.handle(), "SQLForeignKeys");
    SQLRETURN foreign_key_fetch = SQLFetch(stmt.handle());
    if (foreign_key_fetch == SQL_NO_DATA &&
        scalar_int(
            dbc,
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA='mo_odbc_deep' AND TABLE_NAME='fk_child' "
            "AND REFERENCED_TABLE_NAME IS NOT NULL") == 0) {
      throw KnownIssue(
          "older MatrixOne releases do not expose foreign-key rows through "
          "INFORMATION_SCHEMA.KEY_COLUMN_USAGE; matrixorigin/matrixone#25103");
    }
    check(foreign_key_fetch, SQL_HANDLE_STMT, stmt.handle(),
          "SQLForeignKeys fetch");

    auto get_text = [&](SQLUSMALLINT column) {
      SQLCHAR value[256] = {};
      SQLLEN length = 0;
      check(SQLGetData(stmt.handle(), column, SQL_C_CHAR, value, sizeof(value),
                       &length),
            SQL_HANDLE_STMT, stmt.handle(),
            "SQLForeignKeys column " + std::to_string(column));
      return std::string(reinterpret_cast<const char *>(value));
    };
    SQLSMALLINT key_seq = 0;
    SQLLEN key_seq_length = 0;
    expect(get_text(3) == "fk_parent",
           "SQLForeignKeys reported the wrong primary-key table");
    expect(get_text(4) == "id",
           "SQLForeignKeys reported the wrong primary-key column");
    expect(get_text(7) == "fk_child",
           "SQLForeignKeys reported the wrong foreign-key table");
    expect(get_text(8) == "parent_id",
           "SQLForeignKeys reported the wrong foreign-key column");
    check(SQLGetData(stmt.handle(), 9, SQL_C_SSHORT, &key_seq,
                     sizeof(key_seq), &key_seq_length),
          SQL_HANDLE_STMT, stmt.handle(), "SQLForeignKeys KEY_SEQ");
    expect(key_seq == 1, "SQLForeignKeys KEY_SEQ should be 1");
    expect(get_text(12) == "fk_deep_parent",
           "SQLForeignKeys reported the wrong foreign-key name");
    expect(SQLFetch(stmt.handle()) == SQL_NO_DATA,
           "SQLForeignKeys returned more than one key column");
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
  bool utf8mb4_varchar_length_regression = false;
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
          (column_name == "c_varbinary" && column_size == 384) ||
          (column_name == "c_varchar" && column_size == 96);
      if (column_name == "c_varchar" && column_size == 96) {
        utf8mb4_varchar_length_regression = true;
      }
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
    if (utf8mb4_varchar_length_regression) {
      message << "matrixorigin/matrixone#26967: utf8mb4 VARCHAR COM_QUERY "
                 "length still uses three bytes per character";
    } else {
      message << "matrixorigin/matrixone#26683: COM_QUERY reports invalid "
                 "declared lengths";
    }
    for (const auto &mismatch : known_size_mismatches) {
      message << "; " << mismatch;
    }
    throw KnownIssue(message.str());
  }
}

void test_odbc_function_escapes(SQLHDBC dbc) {
  expect(scalar_int(dbc, "SELECT {fn ascii('EWR')}") == 69,
         "ODBC ASCII function escape was not rewritten");
  expect(scalar_text(
             dbc,
             "SELECT {fn substring('Newark Airport', 4, -1)}") == "",
         "ODBC SUBSTRING function escape returned the wrong value");
  expect(scalar_text(
             dbc,
             "SELECT {fn concat({fn ucase('a')}, 'b')}") == "Ab",
         "nested ODBC function escapes were not rewritten");
  expect(scalar_int(dbc, "SELECT {fn year({d '2023-02-01'})}") == 2023,
         "rewriting a function escape damaged a nested date escape");
  expect(scalar_int(dbc, "SELECT {fn length('{fn fake()}')}") == 11,
         "rewriting a function escape modified a quoted brace");
}

void test_json_descriptor(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql =
      "SELECT c_json FROM mo_odbc_deep.type_matrix WHERE id=1";
  check(SQLExecDirect(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect JSON descriptor query");
  SQLLEN case_sensitive = SQL_FALSE;
  check(SQLColAttribute(stmt.handle(), 1, SQL_DESC_CASE_SENSITIVE, nullptr, 0,
                        nullptr, &case_sensitive),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_DESC_CASE_SENSITIVE JSON");
  expect(case_sensitive == SQL_TRUE,
         "JSON SQL_DESC_CASE_SENSITIVE should be SQL_TRUE, actual=" +
             std::to_string(case_sensitive));
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
  SQLWCHAR name[128] = {0x53c2, 0x6570, 0x4e0a, 0x6d77, 0};
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

  const std::string prepared_name_hex = scalar_text(
      dbc, "SELECT hex(name) FROM mo_odbc_deep.param_values WHERE id=7");
  expect(prepared_name_hex == "E58F82E695B0E4B88AE6B5B7",
         "prepared Unicode parameter roundtrip mismatch; stored_hex=" +
             prepared_name_hex);
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

void test_directquery_shape(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql = R"SQL(
SELECT q.category, SUM(q.amount) AS total_amount
FROM (
  SELECT category, amount, event_time, nullable_note, active
  FROM mo_odbc_deep.pbi_sales
  WHERE event_time >= ?
) AS q
WHERE COALESCE(q.nullable_note, '') <> ? AND q.active = ?
GROUP BY q.category
HAVING SUM(q.amount) > ?
ORDER BY q.category
LIMIT ? OFFSET ?)SQL";
  check(SQLPrepare(stmt.handle(),
                   reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
                   static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare DirectQuery-shaped query");

  SQLSMALLINT parameter_count = 0;
  check(SQLNumParams(stmt.handle(), &parameter_count), SQL_HANDLE_STMT,
        stmt.handle(), "SQLNumParams DirectQuery-shaped query");
  expect(parameter_count == 6,
         "DirectQuery-shaped query expected 6 parameters, got " +
             std::to_string(parameter_count));

  for (SQLUSMALLINT parameter = 1; parameter <= 6; ++parameter) {
    SQLSMALLINT data_type = 0;
    SQLULEN parameter_size = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;
    check(SQLDescribeParam(stmt.handle(), parameter, &data_type,
                           &parameter_size, &decimal_digits, &nullable),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLDescribeParam " + std::to_string(parameter));
  }

  SQL_TIMESTAMP_STRUCT start = {2026, 1, 1, 0, 0, 0, 0};
  SQLLEN start_len = sizeof(start);
  SQLCHAR excluded_note[] = "skip";
  SQLLEN excluded_note_len = SQL_NTS;
  SQLCHAR active = 1;
  SQLLEN active_len = 0;
  SQLCHAR minimum_total[] = "20.00";
  SQLLEN minimum_total_len = SQL_NTS;
  SQLINTEGER limit = 1;
  SQLINTEGER offset = 1;
  SQLLEN integer_len = 0;

  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT,
                         SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 26, 6,
                         &start, sizeof(start), &start_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter DirectQuery time");
  check(SQLBindParameter(stmt.handle(), 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_VARCHAR, sizeof(excluded_note) - 1, 0,
                         excluded_note, sizeof(excluded_note),
                         &excluded_note_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter DirectQuery text");
  check(SQLBindParameter(stmt.handle(), 3, SQL_PARAM_INPUT, SQL_C_BIT,
                         SQL_BIT, 1, 0, &active, 0, &active_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter DirectQuery bool");
  check(SQLBindParameter(stmt.handle(), 4, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_DECIMAL, 12, 2, minimum_total,
                         sizeof(minimum_total), &minimum_total_len),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQLBindParameter DirectQuery decimal");
  check(SQLBindParameter(stmt.handle(), 5, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &limit, 0, &integer_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter DirectQuery limit");
  check(SQLBindParameter(stmt.handle(), 6, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &offset, 0, &integer_len),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQLBindParameter DirectQuery offset");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute DirectQuery-shaped query");

  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch DirectQuery-shaped query");
  SQLCHAR category[32] = {};
  SQLLEN category_len = 0;
  double total = 0;
  SQLLEN total_len = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_CHAR, category, sizeof(category),
                   &category_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData DirectQuery category");
  check(SQLGetData(stmt.handle(), 2, SQL_C_DOUBLE, &total, sizeof(total),
                   &total_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData DirectQuery total");
  expect(std::string(reinterpret_cast<const char *>(category)) == "B",
         "DirectQuery-shaped pagination returned the wrong category");
  expect(std::fabs(total - 30.0) < 0.000001,
         "DirectQuery-shaped aggregation returned the wrong total");
  expect(SQLFetch(stmt.handle()) == SQL_NO_DATA,
         "DirectQuery-shaped LIMIT returned more than one row");
}

void test_offset_without_limit_known_issue(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql =
      "SELECT id FROM mo_odbc_deep.pbi_sales ORDER BY id OFFSET ?";
  SQLRETURN rc = SQLPrepare(
      stmt.handle(), reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
      static_cast<SQLINTEGER>(sql.size()));
  if (!succeeded(rc)) {
    const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
    if (!records.empty() && records.front().state == "42000" &&
        records.front().native_error == 1064) {
      throw KnownIssue(
          "matrixorigin/matrixone#26769: Power BI LimitOffset folding "
          "requires OFFSET without LIMIT");
    }
    check(rc, SQL_HANDLE_STMT, stmt.handle(),
          "SQLPrepare offset without limit");
  }

  SQLINTEGER offset = 1;
  SQLLEN offset_len = 0;
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &offset, 0, &offset_len),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQLBindParameter offset without limit");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute offset without limit");
  for (SQLINTEGER expected_id = 2; expected_id <= 5; ++expected_id) {
    check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
          "SQLFetch offset without limit");
    SQLINTEGER actual_id = 0;
    SQLLEN actual_id_len = 0;
    check(SQLGetData(stmt.handle(), 1, SQL_C_SLONG, &actual_id,
                     sizeof(actual_id), &actual_id_len),
          SQL_HANDLE_STMT, stmt.handle(), "SQLGetData offset without limit");
    expect(actual_id == expected_id,
           "offset without limit returned the wrong row");
  }
  expect(SQLFetch(stmt.handle()) == SQL_NO_DATA,
         "offset without limit returned an unexpected row");
}

void test_varbinary_wide_conversion(SQLHDBC dbc) {
  execute(dbc, "DROP TABLE IF EXISTS mo_odbc_deep.binary_wide");
  execute(dbc,
          "CREATE TABLE mo_odbc_deep.binary_wide "
          "(id INT PRIMARY KEY, payload VARBINARY(32), "
          "medium_payload MEDIUMBLOB)");
  execute(dbc,
          "INSERT INTO mo_odbc_deep.binary_wide VALUES "
          "(1, 0xabcdef, 0x0123ff)");

  Statement stmt(dbc);
  const std::string sql =
      "SELECT payload,medium_payload FROM mo_odbc_deep.binary_wide WHERE id=1";
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
          "matrixorigin/matrixone#26716: VARBINARY result metadata uses "
          "MYSQL_TYPE_VARCHAR, so SQL_C_WCHAR binary-to-hex conversion "
          "fails");
    }
    check(rc, SQL_HANDLE_STMT, stmt.handle(),
          "SQLGetData VARBINARY as SQL_C_WCHAR");
  }
  const SQLWCHAR expected[] = {'A', 'B', 'C', 'D', 'E', 'F', 0};
  if (!std::equal(std::begin(expected), std::end(expected),
                  std::begin(value))) {
    throw KnownIssue(
        "matrixorigin/matrixone#26716: VARBINARY result metadata uses "
        "MYSQL_TYPE_VARCHAR, so SQL_C_WCHAR conversion did not return "
        "ABCDEF");
  }
  std::fill(std::begin(value), std::end(value), 0);
  value_len = 0;
  check(SQLGetData(stmt.handle(), 2, SQL_C_WCHAR, value, sizeof(value),
                   &value_len),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQLGetData MEDIUMBLOB as SQL_C_WCHAR");
  const SQLWCHAR expected_medium[] = {'0', '1', '2', '3', 'F', 'F', 0};
  expect(std::equal(std::begin(expected_medium), std::end(expected_medium),
                    std::begin(value)) &&
             value_len == 6 * sizeof(SQLWCHAR),
         "MEDIUMBLOB SQL_C_WCHAR conversion did not return 0123FF");
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

void test_transaction_isolation(SQLHDBC dbc) {
  check(SQLSetConnectAttr(
            dbc, SQL_ATTR_TXN_ISOLATION,
            reinterpret_cast<SQLPOINTER>(uintptr_t{SQL_TXN_READ_COMMITTED}),
            0),
        SQL_HANDLE_DBC, dbc, "SQL_ATTR_TXN_ISOLATION READ_COMMITTED");
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
  if (actual == "READ-COMMITTED") return;
  if (actual == "REPEATABLE-READ") {
    throw KnownIssue(
        "matrixorigin/matrixone#26648: SET SESSION TRANSACTION ISOLATION "
        "LEVEL is accepted but ignored; actual=" + actual);
  }
  throw Failure("unexpected transaction isolation after setter: " + actual);
}

void test_streaming_and_chunked_getdata(SQLHDBC dbc) {
  const size_t text_size = 1024 * 1024;
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

void test_parameter_arrays_and_partial_failure(SQLHDBC dbc) {
  execute(dbc, "DROP TABLE IF EXISTS mo_odbc_deep.param_array");
  execute(dbc, "CREATE TABLE mo_odbc_deep.param_array "
               "(id INT PRIMARY KEY, name VARCHAR(32), amount INT)");

  Statement stmt(dbc);
  const std::string sql =
      "INSERT INTO mo_odbc_deep.param_array VALUES (?,?,?)";
  check(SQLPrepare(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare parameter array insert");

  SQLINTEGER ids[4] = {1, 2, 3, 4};
  SQLCHAR names[4][16] = {"one", "two", "three", "four"};
  SQLINTEGER amounts[4] = {10, 20, 30, 40};
  SQLLEN id_indicators[4] = {0, 0, 0, 0};
  SQLLEN name_indicators[4] = {SQL_NTS, SQL_NTS, SQL_NTS, SQL_NTS};
  SQLLEN amount_indicators[4] = {0, 0, 0, 0};
  SQLUSMALLINT statuses[4] = {SQL_PARAM_UNUSED, SQL_PARAM_UNUSED,
                              SQL_PARAM_UNUSED, SQL_PARAM_UNUSED};
  SQLULEN processed = 0;

  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAM_BIND_TYPE,
                       reinterpret_cast<SQLPOINTER>(SQL_PARAM_BIND_BY_COLUMN),
                       0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_PARAM_BIND_TYPE");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAMSET_SIZE,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{4}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_PARAMSET_SIZE=4");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAMS_PROCESSED_PTR,
                       &processed, 0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_PARAMS_PROCESSED_PTR");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAM_STATUS_PTR, statuses, 0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_PARAM_STATUS_PTR");
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, ids, sizeof(ids[0]),
                         id_indicators),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter array id");
  check(SQLBindParameter(stmt.handle(), 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_VARCHAR, 32, 0, names, sizeof(names[0]),
                         name_indicators),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter array name");
  check(SQLBindParameter(stmt.handle(), 3, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, amounts, sizeof(amounts[0]),
                         amount_indicators),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter array amount");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute parameter array insert");
  expect(processed == 4,
         "parameter array reported processed=" + std::to_string(processed));
  for (size_t i = 0; i < 4; ++i) {
    expect(statuses[i] == SQL_PARAM_SUCCESS ||
               statuses[i] == SQL_PARAM_SUCCESS_WITH_INFO,
           "parameter array item " + std::to_string(i) +
               " has status=" + std::to_string(statuses[i]));
  }
  expect(scalar_int(dbc,
                    "SELECT COUNT(*) FROM mo_odbc_deep.param_array") == 4,
         "parameter array did not insert four rows");

  check(SQLFreeStmt(stmt.handle(), SQL_CLOSE), SQL_HANDLE_STMT, stmt.handle(),
        "SQL_CLOSE before partial-failure batch");
  SQLINTEGER duplicate_ids[3] = {5, 2, 6};
  SQLCHAR duplicate_names[3][16] = {"five", "duplicate", "six"};
  SQLINTEGER duplicate_amounts[3] = {50, 200, 60};
  SQLLEN duplicate_id_indicators[3] = {0, 0, 0};
  SQLLEN duplicate_name_indicators[3] = {SQL_NTS, SQL_NTS, SQL_NTS};
  SQLLEN duplicate_amount_indicators[3] = {0, 0, 0};
  SQLUSMALLINT duplicate_statuses[3] = {SQL_PARAM_UNUSED, SQL_PARAM_UNUSED,
                                        SQL_PARAM_UNUSED};
  processed = 0;
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAMSET_SIZE,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{3}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_PARAMSET_SIZE=3");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAM_STATUS_PTR,
                       duplicate_statuses, 0),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQL_ATTR_PARAM_STATUS_PTR partial failure");
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, duplicate_ids,
                         sizeof(duplicate_ids[0]), duplicate_id_indicators),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter duplicate id");
  check(SQLBindParameter(stmt.handle(), 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_VARCHAR, 32, 0, duplicate_names,
                         sizeof(duplicate_names[0]), duplicate_name_indicators),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter duplicate name");
  check(SQLBindParameter(stmt.handle(), 3, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, duplicate_amounts,
                         sizeof(duplicate_amounts[0]),
                         duplicate_amount_indicators),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter duplicate amount");

  const SQLRETURN rc = SQLExecute(stmt.handle());
  expect(rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO,
         "mixed valid/duplicate parameter array unexpectedly returned rc=" +
             std::to_string(rc));
  const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  expect(!records.empty() && records.front().state.compare(0, 2, "23") == 0,
         "duplicate parameter set should report integrity SQLSTATE" +
             diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
  expect(processed > 0 && processed <= 3,
         "partial-failure batch reported invalid processed=" +
             std::to_string(processed));
  expect(std::find(std::begin(duplicate_statuses),
                   std::end(duplicate_statuses), SQL_PARAM_ERROR) !=
             std::end(duplicate_statuses),
         "partial-failure batch did not mark any parameter set as error");

  check(SQLFreeStmt(stmt.handle(), SQL_CLOSE), SQL_HANDLE_STMT, stmt.handle(),
        "SQL_CLOSE after partial-failure batch");
  check(SQLFreeStmt(stmt.handle(), SQL_RESET_PARAMS), SQL_HANDLE_STMT,
        stmt.handle(), "SQL_RESET_PARAMS after partial-failure batch");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAMSET_SIZE,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{1}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "restore SQL_ATTR_PARAMSET_SIZE");
  const std::string recovery_sql = "SELECT COUNT(*) FROM mo_odbc_deep.param_array";
  check(SQLExecDirect(
            stmt.handle(), reinterpret_cast<SQLCHAR *>(
                               const_cast<char *>(recovery_sql.data())),
            static_cast<SQLINTEGER>(recovery_sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "reuse statement after batch error");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "fetch after batch error");
}

void test_row_array_fetch(SQLHDBC dbc) {
  Statement stmt(dbc);
  SQLINTEGER ids[3] = {};
  SQLCHAR names[3][16] = {};
  SQLLEN id_indicators[3] = {};
  SQLLEN name_indicators[3] = {};
  SQLUSMALLINT row_statuses[3] = {};
  SQLULEN rows_fetched = 0;

  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_ROW_ARRAY_SIZE,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{3}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_ROW_ARRAY_SIZE=3");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_ROWS_FETCHED_PTR, &rows_fetched,
                       0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_ROWS_FETCHED_PTR");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_ROW_STATUS_PTR, row_statuses, 0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_ROW_STATUS_PTR");
  check(SQLBindCol(stmt.handle(), 1, SQL_C_SLONG, ids, sizeof(ids[0]),
                   id_indicators),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindCol row-array id");
  check(SQLBindCol(stmt.handle(), 2, SQL_C_CHAR, names, sizeof(names[0]),
                   name_indicators),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindCol row-array name");

  const std::string sql =
      "SELECT id,name FROM mo_odbc_deep.param_array WHERE id<=4 ORDER BY id";
  check(SQLExecDirect(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect row-array query");
  check(SQLFetchScroll(stmt.handle(), SQL_FETCH_NEXT, 0), SQL_HANDLE_STMT,
        stmt.handle(), "SQLFetchScroll first row array");
  expect(rows_fetched == 3, "first row array should contain three rows");
  for (size_t i = 0; i < 3; ++i) {
    expect(ids[i] == static_cast<SQLINTEGER>(i + 1),
           "row-array id mismatch at index " + std::to_string(i));
    expect(row_statuses[i] == SQL_ROW_SUCCESS ||
               row_statuses[i] == SQL_ROW_SUCCESS_WITH_INFO,
           "row-array status mismatch at index " + std::to_string(i));
  }
  check(SQLFetchScroll(stmt.handle(), SQL_FETCH_NEXT, 0), SQL_HANDLE_STMT,
        stmt.handle(), "SQLFetchScroll second row array");
  expect(rows_fetched == 1 && ids[0] == 4,
         "second row array should contain id=4 only");
  expect(SQLFetchScroll(stmt.handle(), SQL_FETCH_NEXT, 0) == SQL_NO_DATA,
         "row-array query should be exhausted");
}

void test_parameter_array_select(SQLHDBC dbc) {
  Statement stmt(dbc);
  SQLINTEGER values[3] = {3, 1, 2};
  SQLLEN indicators[3] = {sizeof(SQLINTEGER), sizeof(SQLINTEGER),
                          sizeof(SQLINTEGER)};
  SQLUSMALLINT statuses[3] = {SQL_PARAM_UNUSED, SQL_PARAM_UNUSED,
                              SQL_PARAM_UNUSED};
  SQLULEN processed = 0;
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAM_BIND_TYPE,
                       reinterpret_cast<SQLPOINTER>(SQL_PARAM_BIND_BY_COLUMN),
                       0),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQL_ATTR_PARAM_BIND_TYPE parameter-array SELECT");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAMSET_SIZE,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{3}), 0),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQL_ATTR_PARAMSET_SIZE parameter-array SELECT");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAM_STATUS_PTR, statuses, 0),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQL_ATTR_PARAM_STATUS_PTR parameter-array SELECT");
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_PARAMS_PROCESSED_PTR,
                       &processed, 0),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQL_ATTR_PARAMS_PROCESSED_PTR parameter-array SELECT");
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, values, 0, indicators),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQLBindParameter parameter-array SELECT");
  const std::string sql = "SELECT ?";
  check(SQLExecDirect(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect parameter-array SELECT");
  expect(processed == 3,
         "parameter-array SELECT processed=" + std::to_string(processed));
  std::vector<SQLINTEGER> actual_values;
  for (size_t i = 0; i < 3; ++i) {
    expect(statuses[i] == SQL_PARAM_SUCCESS ||
               statuses[i] == SQL_PARAM_SUCCESS_WITH_INFO,
           "parameter-array SELECT status " + std::to_string(i) + "=" +
               std::to_string(statuses[i]));
    check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
          "SQLFetch parameter-array SELECT item " + std::to_string(i));
    SQLINTEGER actual = 0;
    SQLLEN actual_len = 0;
    check(SQLGetData(stmt.handle(), 1, SQL_C_SLONG, &actual, sizeof(actual),
                     &actual_len),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLGetData parameter-array SELECT item " + std::to_string(i));
    actual_values.push_back(actual);
  }
  expect(SQLFetch(stmt.handle()) == SQL_NO_DATA,
         "parameter-array SELECT returned more than three rows");
  const std::vector<SQLINTEGER> expected_values = {3, 1, 2};
  if (actual_values == expected_values) return;
  std::vector<SQLINTEGER> sorted_actual = actual_values;
  std::sort(sorted_actual.begin(), sorted_actual.end());
  if (sorted_actual == std::vector<SQLINTEGER>({1, 2, 3})) {
    throw KnownIssue(
        "matrixorigin/matrixone#27034: UNION ALL branch reordering breaks "
        "ODBC parameter-array SELECT result mapping");
  }
  std::ostringstream message;
  message << "parameter-array SELECT returned unexpected values:";
  for (const SQLINTEGER value : actual_values) message << ' ' << value;
  throw Failure(message.str());
}

void test_max_rows_attribute(SQLHDBC dbc) {
  Statement stmt(dbc);
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_MAX_ROWS,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{3}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "SQL_ATTR_MAX_ROWS=3");
  const std::string sql =
      "SELECT id FROM mo_odbc_deep.pbi_sales ORDER BY id";
  check(SQLExecDirect(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect MAX_ROWS query");
  int rows = 0;
  while (SQLFetch(stmt.handle()) == SQL_SUCCESS) ++rows;
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_MAX_ROWS,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{0}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "restore SQL_ATTR_MAX_ROWS");
  if (rows == 3) return;
  if (rows == 5) {
    throw KnownIssue(
        "matrixorigin/matrixone#27035: sql_select_limit is accepted but "
        "ignored, breaking ODBC SQL_ATTR_MAX_ROWS");
  }
  throw Failure("SQL_ATTR_MAX_ROWS=3 returned " + std::to_string(rows) +
                " rows");
}

void test_data_at_execution_cancel_recovery(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string insert_sql =
      "INSERT INTO mo_odbc_deep.long_values VALUES (?,?,NULL)";
  check(SQLPrepare(
            stmt.handle(), reinterpret_cast<SQLCHAR *>(
                               const_cast<char *>(insert_sql.data())),
            static_cast<SQLINTEGER>(insert_sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare cancellable stream");
  SQLINTEGER id = 99;
  SQLLEN id_len = 0;
  char token = 0;
  SQLLEN stream_len = SQL_DATA_AT_EXEC;
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &id, 0, &id_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter cancellable id");
  check(SQLBindParameter(stmt.handle(), 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_LONGVARCHAR, 1024, 0, &token, 0, &stream_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter cancellable stream");
  expect(SQLExecute(stmt.handle()) == SQL_NEED_DATA,
         "cancellable stream did not enter SQL_NEED_DATA");
  SQLPOINTER returned_token = nullptr;
  expect(SQLParamData(stmt.handle(), &returned_token) == SQL_NEED_DATA &&
             returned_token == &token,
         "SQLParamData did not return the cancellable stream token");
  check(SQLPutData(stmt.handle(), const_cast<char *>("partial"), 7),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPutData partial stream");

  const SQLRETURN sequence_rc = SQLExecute(stmt.handle());
  expect(sequence_rc == SQL_ERROR,
         "SQLExecute during SQL_NEED_DATA should fail with HY010");
  const auto sequence_records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  expect(!sequence_records.empty() && sequence_records.front().state == "HY010",
         "SQLExecute during SQL_NEED_DATA should report HY010" +
             diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
  check(SQLCancel(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLCancel during SQL_NEED_DATA");
  check(SQLFreeStmt(stmt.handle(), SQL_CLOSE), SQL_HANDLE_STMT, stmt.handle(),
        "SQL_CLOSE after SQL_NEED_DATA cancellation");
  check(SQLFreeStmt(stmt.handle(), SQL_RESET_PARAMS), SQL_HANDLE_STMT,
        stmt.handle(), "SQL_RESET_PARAMS after SQL_NEED_DATA cancellation");

  const std::string recovery_sql = "SELECT 1";
  check(SQLExecDirect(
            stmt.handle(), reinterpret_cast<SQLCHAR *>(
                               const_cast<char *>(recovery_sql.data())),
            static_cast<SQLINTEGER>(recovery_sql.size())),
        SQL_HANDLE_STMT, stmt.handle(),
        "reuse statement after SQL_NEED_DATA cancellation");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "fetch after SQL_NEED_DATA cancellation");
  expect(scalar_int(dbc,
                    "SELECT COUNT(*) FROM mo_odbc_deep.long_values "
                    "WHERE id=99") == 0,
         "cancelled data-at-execution insert became visible");
}

void test_diagnostic_lifecycle(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string bad_sql =
      "SELECT * FROM mo_odbc_deep.diagnostic_table_that_does_not_exist";
  expect(SQLExecDirect(
             stmt.handle(), reinterpret_cast<SQLCHAR *>(
                                const_cast<char *>(bad_sql.data())),
             static_cast<SQLINTEGER>(bad_sql.size())) == SQL_ERROR,
         "diagnostic lifecycle setup statement unexpectedly succeeded");
  expect(!diagnostics(SQL_HANDLE_STMT, stmt.handle()).empty(),
         "failed statement did not create a diagnostic");

  const std::string good_sql = "SELECT 1";
  check(SQLExecDirect(
            stmt.handle(), reinterpret_cast<SQLCHAR *>(
                               const_cast<char *>(good_sql.data())),
            static_cast<SQLINTEGER>(good_sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "success after statement error");
  expect(diagnostics(SQL_HANDLE_STMT, stmt.handle()).empty(),
         "successful SQLExecDirect left stale diagnostics");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch diagnostic lifecycle");

  SQLINTEGER value = 0;
  SQLLEN value_len = 0;
  expect(SQLGetData(stmt.handle(), 2, SQL_C_SLONG, &value, sizeof(value),
                    &value_len) == SQL_ERROR,
         "SQLGetData with an invalid column number should fail");
  const auto column_records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  expect(!column_records.empty() && column_records.front().state == "07009",
         "invalid column number should report SQLSTATE 07009" +
             diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
  check(SQLGetData(stmt.handle(), 1, SQL_C_SLONG, &value, sizeof(value),
                   &value_len),
        SQL_HANDLE_STMT, stmt.handle(),
        "valid SQLGetData after invalid column number");
  expect(value == 1, "statement was not reusable after invalid column access");
  expect(diagnostics(SQL_HANDLE_STMT, stmt.handle()).empty(),
         "successful SQLGetData left stale diagnostics");
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

void test_truncation_diagnostics(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql =
      "SELECT c_varchar FROM mo_odbc_deep.type_matrix WHERE id=1";
  check(SQLExecDirect(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect truncation query");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch truncation row");

  SQLCHAR value[8] = {};
  SQLLEN value_len = 0;
  SQLRETURN rc = SQLGetData(stmt.handle(), 1, SQL_C_CHAR, value,
                            sizeof(value), &value_len);
  expect(rc == SQL_SUCCESS_WITH_INFO,
         "truncated SQLGetData should return SQL_SUCCESS_WITH_INFO");
  const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  expect(!records.empty() && records.front().state == "01004",
         "truncated SQLGetData should report SQLSTATE 01004" +
             diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
  expect(value_len > static_cast<SQLLEN>(sizeof(value) - 1),
         "truncated SQLGetData did not report the full value length");
}

void test_numeric_overflow_diagnostics(SQLHDBC dbc) {
  auto expect_overflow = [&](const std::string &sql, SQLSMALLINT c_type,
                             SQLPOINTER value, SQLLEN value_size,
                             const std::string &label) {
    Statement stmt(dbc);
    check(SQLExecDirect(
              stmt.handle(),
              reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
              static_cast<SQLINTEGER>(sql.size())),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLExecDirect numeric overflow " + label);
    check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
          "SQLFetch numeric overflow " + label);
    SQLLEN value_len = 0;
    SQLRETURN rc = SQLGetData(stmt.handle(), 1, c_type, value, value_size,
                              &value_len);
    expect(rc == SQL_ERROR,
           "out-of-range " + label + " conversion should fail");
    const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
    expect(!records.empty() && records.front().state == "22003",
           label + " overflow should report SQLSTATE 22003" +
               diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
  };

  SQLSCHAR signed_tiny = 0;
  SQLCHAR unsigned_tiny = 0;
  SQLSMALLINT signed_short = 0;
  SQLUSMALLINT unsigned_short = 0;
  SQLINTEGER signed_long = 0;
  SQLUINTEGER unsigned_long = 0;
  expect_overflow("SELECT 1000", SQL_C_STINYINT, &signed_tiny,
                  sizeof(signed_tiny), "SQL_C_STINYINT");
  expect_overflow("SELECT -1", SQL_C_UTINYINT, &unsigned_tiny,
                  sizeof(unsigned_tiny), "SQL_C_UTINYINT");
  expect_overflow("SELECT 40000", SQL_C_SSHORT, &signed_short,
                  sizeof(signed_short), "SQL_C_SSHORT");
  expect_overflow("SELECT -1", SQL_C_USHORT, &unsigned_short,
                  sizeof(unsigned_short), "SQL_C_USHORT");
  expect_overflow("SELECT 3000000000", SQL_C_SLONG, &signed_long,
                  sizeof(signed_long), "SQL_C_SLONG");
  expect_overflow("SELECT -1", SQL_C_ULONG, &unsigned_long,
                  sizeof(unsigned_long), "SQL_C_ULONG");
}

void test_missing_parameter_diagnostics(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql = "SELECT ?";
  check(SQLPrepare(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare missing parameter");
  SQLRETURN rc = SQLExecute(stmt.handle());
  expect(rc == SQL_ERROR,
         "SQLExecute with a missing parameter should fail");
  const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  expect(!records.empty() && records.front().state.compare(0, 2, "07") == 0,
         "missing parameter should report SQLSTATE class 07" +
             diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
}

void test_function_sequence_diagnostics(SQLHDBC dbc) {
  Statement stmt(dbc);
  SQLRETURN rc = SQLFetch(stmt.handle());
  expect(rc == SQL_ERROR, "SQLFetch before execution should fail");
  const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  expect(!records.empty() &&
             (records.front().state == "HY010" ||
              records.front().state == "24000"),
         "SQLFetch before execution should report a sequence/cursor state "
         "diagnostic" + diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
}

void test_schema_drift_recovery(SQLHDBC dbc) {
  execute(dbc, "DROP TABLE IF EXISTS mo_odbc_deep.schema_drift");
  execute(dbc, "CREATE TABLE mo_odbc_deep.schema_drift "
               "(id INT PRIMARY KEY, metric INT)");
  execute(dbc, "INSERT INTO mo_odbc_deep.schema_drift VALUES (1,10)");

  Statement stmt(dbc);
  const std::string sql =
      "SELECT metric FROM mo_odbc_deep.schema_drift WHERE id=?";
  check(SQLPrepare(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare schema drift query");
  SQLINTEGER id = 1;
  SQLLEN id_len = 0;
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &id, 0, &id_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter schema drift id");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute schema drift baseline");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch schema drift baseline");
  check(SQLFreeStmt(stmt.handle(), SQL_CLOSE), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFreeStmt schema drift baseline");

  execute(dbc, "ALTER TABLE mo_odbc_deep.schema_drift DROP COLUMN metric");
  SQLRETURN rc = SQLExecute(stmt.handle());
  expect(rc == SQL_ERROR,
         "prepared query should fail after its selected column is dropped");
  const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  expect(!records.empty(), "schema drift error has no ODBC diagnostic");
  const bool generic_schema_drift_diagnostic =
      records.front().state == "HY000" &&
      records.front().native_error == 20301;
  expect(generic_schema_drift_diagnostic ||
             records.front().state.compare(0, 2, "42") == 0,
         "schema drift returned an unexpected diagnostic" +
             diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));

  check(SQLFreeStmt(stmt.handle(), SQL_CLOSE), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFreeStmt after schema drift error");
  check(SQLFreeStmt(stmt.handle(), SQL_RESET_PARAMS), SQL_HANDLE_STMT,
        stmt.handle(), "SQL_RESET_PARAMS after schema drift error");
  execute(dbc, "ALTER TABLE mo_odbc_deep.schema_drift ADD COLUMN metric INT");
  execute(dbc, "UPDATE mo_odbc_deep.schema_drift SET metric=20 WHERE id=1");
  check(SQLPrepare(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare schema drift recovery");
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &id, 0, &id_len),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQLBindParameter schema drift recovery");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute schema drift recovery");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch schema drift recovery");
  SQLINTEGER metric = 0;
  SQLLEN metric_len = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_SLONG, &metric, sizeof(metric),
                   &metric_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData schema drift recovery");
  expect(metric == 20, "schema drift recovery returned stale data");
  if (generic_schema_drift_diagnostic) {
    throw KnownIssue(
        "matrixorigin/matrixone#27024: missing column returns generic "
        "HY000/20301 instead of SQLSTATE 42S22");
  }
}

void test_transaction_error_recovery(SQLHDBC dbc) {
  execute(dbc, "DELETE FROM mo_odbc_deep.tx_values");
  check(SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
                          reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0),
        SQL_HANDLE_DBC, dbc, "disable autocommit for error recovery");
  try {
    execute(dbc, "INSERT INTO mo_odbc_deep.tx_values VALUES (10,'first')");
    const Diagnostic duplicate = expect_error(
        dbc, "INSERT INTO mo_odbc_deep.tx_values VALUES (10,'duplicate')");
    expect(duplicate.state.compare(0, 2, "23") == 0,
           "duplicate key in transaction should report class 23");
    check(SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_ROLLBACK), SQL_HANDLE_DBC, dbc,
          "rollback transaction after statement error");
    expect(scalar_int(dbc,
                      "SELECT COUNT(*) FROM mo_odbc_deep.tx_values") == 0,
           "rollback after statement error left a visible row");
    expect(scalar_int(dbc, "SELECT 1") == 1,
           "connection is not reusable after transaction error rollback");
  } catch (...) {
    SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_ROLLBACK);
    SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
                      reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0);
    throw;
  }
  check(SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
                          reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0),
        SQL_HANDLE_DBC, dbc, "restore autocommit after error recovery");
}

void test_unknown_charset_metadata(SQLHDBC dbc) {
  execute(dbc, "DROP TABLE IF EXISTS mo_odbc_deep.explain_delete");
  execute(dbc, "CREATE TABLE mo_odbc_deep.explain_delete "
               "(id INT UNSIGNED, c CHAR(10))");
  execute(dbc, "CREATE INDEX explain_delete_i ON "
               "mo_odbc_deep.explain_delete(id,c)");

  Statement stmt(dbc);
  const std::string sql =
      "EXPLAIN DELETE a1,a2 FROM mo_odbc_deep.explain_delete AS a1 "
      "INNER JOIN mo_odbc_deep.explain_delete AS a2 "
      "WHERE a1.id=a2.id AND a2.id>=?";
  SQLRETURN prepare_rc = SQLPrepare(
      stmt.handle(),
      reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
      static_cast<SQLINTEGER>(sql.size()));
  if (!succeeded(prepare_rc)) {
    const auto records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
    if (!records.empty() && records.front().state.compare(0, 2, "42") == 0) {
      throw KnownIssue(
          "older MatrixOne releases do not support the multi-table EXPLAIN "
          "DELETE syntax that exposes matrixorigin/matrixone#27022");
    }
    check(prepare_rc, SQL_HANDLE_STMT, stmt.handle(),
          "SQLPrepare EXPLAIN DELETE metadata regression");
  }
  SQLUINTEGER minimum_id = 0;
  SQLLEN minimum_id_len = 0;
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_ULONG,
                         SQL_INTEGER, 0, 0, &minimum_id, 0,
                         &minimum_id_len),
        SQL_HANDLE_STMT, stmt.handle(),
        "SQLBindParameter EXPLAIN DELETE metadata regression");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute EXPLAIN DELETE metadata regression");

  SQLSMALLINT column_count = 0;
  check(SQLNumResultCols(stmt.handle(), &column_count), SQL_HANDLE_STMT,
        stmt.handle(), "SQLNumResultCols EXPLAIN DELETE metadata regression");
  expect(column_count > 0,
         "EXPLAIN DELETE metadata regression returned no columns");
  for (SQLUSMALLINT column = 1; column <= column_count; ++column) {
    SQLULEN column_size = 0;
    check(SQLDescribeCol(stmt.handle(), column, nullptr, 0, nullptr, nullptr,
                         &column_size, nullptr, nullptr),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLDescribeCol EXPLAIN DELETE metadata regression");
  }
  SQLRETURN fetch_rc = SQLFetch(stmt.handle());
  expect(succeeded(fetch_rc) || fetch_rc == SQL_NO_DATA,
         "EXPLAIN DELETE result fetch failed" +
             diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
}

void test_unreachable_connection_sqlstate(
    const std::string &connection_string) {
  SQLHENV env = SQL_NULL_HENV;
  SQLHDBC dbc = SQL_NULL_HDBC;
  check(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env),
        SQL_HANDLE_ENV, env, "SQLAllocHandle unreachable ENV");
  try {
    check(SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
                        reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0),
          SQL_HANDLE_ENV, env, "SQLSetEnvAttr unreachable ODBC3");
    check(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc), SQL_HANDLE_ENV, env,
          "SQLAllocHandle unreachable DBC");
    check(SQLSetConnectAttr(dbc, SQL_ATTR_LOGIN_TIMEOUT,
                            reinterpret_cast<SQLPOINTER>(uintptr_t{2}), 0),
          SQL_HANDLE_DBC, dbc, "SQL_ATTR_LOGIN_TIMEOUT unreachable");

    const std::string unreachable =
        connection_string + ";SERVER=127.0.0.1;PORT=1;LOGIN_TIMEOUT=2";
    std::vector<SQLWCHAR> connection = widen_ascii(unreachable);
    SQLWCHAR completed[4096] = {};
    SQLSMALLINT completed_len = 0;
    SQLRETURN rc = SQLDriverConnectW(
        dbc, nullptr, connection.data(), SQL_NTS, completed,
        sizeof(completed) / sizeof(completed[0]), &completed_len,
        SQL_DRIVER_NOPROMPT);
    expect(rc == SQL_ERROR,
           "connection to an unused local port should fail");
    const auto records = diagnostics(SQL_HANDLE_DBC, dbc);
    expect(!records.empty() && records.front().state == "08001",
           "initial connection failure should report SQLSTATE 08001" +
               diagnostic_text(SQL_HANDLE_DBC, dbc));
  } catch (...) {
    if (dbc != SQL_NULL_HDBC) SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    throw;
  }
  SQLFreeHandle(SQL_HANDLE_DBC, dbc);
  SQLFreeHandle(SQL_HANDLE_ENV, env);
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

void test_prefetch_query_rewrite(const std::string &connection_string) {
  Database db(connection_string + ";PREFETCH=2");
  Statement stmt(db.handle());
  std::string literal;
  literal.reserve(5000);
  for (int i = 0; i < 500; ++i) literal += "0123456789";
  const std::string sql =
      "SELECT id,'" + literal +
      "' FROM mo_odbc_deep.type_matrix ORDER BY id";
  check(SQLExecDirect(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "PREFETCH query rewrite");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch PREFETCH query");
  SQLCHAR value[5001] = {};
  SQLLEN value_len = 0;
  check(SQLGetData(stmt.handle(), 2, SQL_C_CHAR, value, sizeof(value),
                   &value_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData PREFETCH query");
  expect(value_len == static_cast<SQLLEN>(literal.size()) &&
             std::memcmp(value, literal.data(), literal.size()) == 0,
         "PREFETCH query rewrite changed the long literal");
}

void test_pad_space_connection(const std::string &connection_string) {
  Database db(connection_string + ";PAD_SPACE=1");
  Statement stmt(db.handle());
  const std::string sql =
      "SELECT c_char FROM mo_odbc_deep.type_matrix WHERE id=1";
  check(SQLExecDirect(
            stmt.handle(),
            reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
            static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLExecDirect PAD_SPACE query");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch PAD_SPACE query");
  SQLCHAR value[16] = {};
  SQLLEN value_len = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_CHAR, value, sizeof(value),
                   &value_len),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData PAD_SPACE query");
  if (value_len == 8 &&
      std::string(reinterpret_cast<char *>(value), 8) == "MO      ") {
    return;
  }
  if (value_len == 2 &&
      std::strcmp(reinterpret_cast<char *>(value), "MO") == 0) {
    throw KnownIssue(
        "matrixorigin/matrixone#27036: PAD_CHAR_TO_FULL_LENGTH is accepted "
        "but CHAR values remain unpadded over ODBC");
  }
  throw Failure("PAD_SPACE=1 returned length=" + std::to_string(value_len) +
                " value='" + reinterpret_cast<char *>(value) + "'");
}

void test_empty_bookmark_fetch(const std::string &connection_string) {
  {
    Database setup_db(connection_string);
    execute(setup_db.handle(),
            "DROP TABLE IF EXISTS mo_odbc_deep.empty_bookmark_case");
    execute(setup_db.handle(),
            "CREATE TABLE mo_odbc_deep.empty_bookmark_case "
            "(id INT, name VARCHAR(128) NOT NULL)");
    execute(setup_db.handle(),
            "INSERT INTO mo_odbc_deep.empty_bookmark_case VALUES "
            "(1,'one'),(2,'two'),(3,'three'),(4,'four'),(5,'five')");
  }
  for (int selected_id : {0, 3}) {
   for (const char *mode : {"NO_SSPS=0", "NO_SSPS=1"}) {
    const std::string label = std::string(mode) + " id=" +
                              std::to_string(selected_id);
    Database db(connection_string + ";" + mode);
    Statement stmt(db.handle());
    constexpr SQLULEN rowset_size = 5;
    SQLULEN rows_fetched = 999;
    SQLUSMALLINT row_status[rowset_size] = {};
    SQLINTEGER ids[rowset_size] = {};
    SQLCHAR names[rowset_size][16] = {};
    SQLLEN name_lengths[rowset_size] = {};
    SQLCHAR bookmarks[rowset_size][10] = {};

    check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_USE_BOOKMARKS,
                         reinterpret_cast<SQLPOINTER>(SQL_UB_VARIABLE), 0),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQL_ATTR_USE_BOOKMARKS " + label);
    check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_ROW_STATUS_PTR, row_status, 0),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQL_ATTR_ROW_STATUS_PTR " + label);
    check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_ROWS_FETCHED_PTR,
                         &rows_fetched, 0),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQL_ATTR_ROWS_FETCHED_PTR " + label);
    check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_CURSOR_TYPE,
                         reinterpret_cast<SQLPOINTER>(SQL_CURSOR_STATIC), 0),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQL_ATTR_CURSOR_TYPE " + label);
    check(SQLSetStmtOption(stmt.handle(), SQL_ROWSET_SIZE, rowset_size),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQL_ROWSET_SIZE " + label);

    const std::string sql =
        "SELECT id,name FROM mo_odbc_deep.empty_bookmark_case "
        "WHERE id=" + std::to_string(selected_id) + " ORDER BY name DESC";
    check(SQLPrepare(
              stmt.handle(), reinterpret_cast<SQLCHAR *>(
                                 const_cast<char *>(sql.data())),
              static_cast<SQLINTEGER>(sql.size())),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLPrepare bookmark query " + label);
    check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
          "SQLExecute bookmark query " + label);
    check(SQLBindCol(stmt.handle(), 0, SQL_C_VARBOOKMARK, bookmarks,
                     sizeof(bookmarks[0]), nullptr),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLBindCol bookmark " + label);
    check(SQLBindCol(stmt.handle(), 1, SQL_C_SLONG, ids, sizeof(ids[0]),
                     nullptr),
          SQL_HANDLE_STMT, stmt.handle(), "SQLBindCol id " + label);
    check(SQLBindCol(stmt.handle(), 2, SQL_C_CHAR, names, sizeof(names[0]),
                     name_lengths),
          SQL_HANDLE_STMT, stmt.handle(),
          "SQLBindCol name " + label);
    const SQLRETURN rc =
        SQLFetchScroll(stmt.handle(), SQL_FETCH_BOOKMARK, 0);
    const SQLRETURN expected_rc =
        selected_id == 0 ? SQL_NO_DATA : SQL_SUCCESS;
    const SQLULEN expected_rows = selected_id == 0 ? 0 : 1;
    expect(rc == expected_rc && rows_fetched == expected_rows,
           "bookmark fetch returned an unexpected result for " + label +
               ", rc=" + std::to_string(rc) +
               " rows_fetched=" + std::to_string(rows_fetched) +
               diagnostic_text(SQL_HANDLE_STMT, stmt.handle()));
   }
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
  const auto timeout_records =
      rc == SQL_ERROR ? diagnostics(SQL_HANDLE_STMT, stmt.handle())
                      : std::vector<Diagnostic>{};
  check(SQLSetStmtAttr(stmt.handle(), SQL_ATTR_QUERY_TIMEOUT,
                       reinterpret_cast<SQLPOINTER>(uintptr_t{0}), 0),
        SQL_HANDLE_STMT, stmt.handle(), "restore SQL_ATTR_QUERY_TIMEOUT");
  if (rc == SQL_ERROR && seconds >= 0.5 && seconds < 1.8) {
    expect(!timeout_records.empty() && timeout_records.front().state == "HYT00",
           "query timeout should report HYT00" + execute_diagnostics);
    return;
  }
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
  const auto cancel_records = diagnostics(SQL_HANDLE_STMT, stmt.handle());
  if (!succeeded(cancel_rc) || execute_rc.load() != SQL_ERROR || seconds > 4.0 ||
      cancel_records.empty() || cancel_records.front().state != "HY008") {
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
        {"ODBC scalar function escapes",
         [&] { test_odbc_function_escapes(db.handle()); }},
        {"catalog metadata", [&] { test_catalog_metadata(db.handle()); }},
        {"result descriptors", [&] { test_result_descriptors(db.handle()); }},
        {"JSON descriptor", [&] { test_json_descriptor(db.handle()); }},
        {"type roundtrip", [&] { test_type_roundtrip(db.handle()); }},
        {"prepared parameters",
         [&] { test_prepared_parameters(db.handle()); }},
        {"prepared floating-point precision",
         [&] { test_prepared_floating_point(db.handle()); }},
        {"prepared boolean parameters",
         [&] { test_prepared_boolean(db.handle()); }},
        {"Power BI DirectQuery SQL shape",
         [&] { test_directquery_shape(db.handle()); }},
        {"offset without limit",
         [&] { test_offset_without_limit_known_issue(db.handle()); }},
        {"VARBINARY wide conversion",
         [&] { test_varbinary_wide_conversion(db.handle()); }},
        {"unquoted Unicode identifier",
         [&] { test_unquoted_unicode_identifier(db.handle()); }},
        {"transactions", [&] { test_transactions(db.handle()); }},
        {"transaction isolation",
         [&] { test_transaction_isolation(db.handle()); }},
        {"streaming and chunked SQLGetData",
         [&] { test_streaming_and_chunked_getdata(db.handle()); }},
        {"parameter arrays and partial failure",
         [&] { test_parameter_arrays_and_partial_failure(db.handle()); }},
        {"row array fetch", [&] { test_row_array_fetch(db.handle()); }},
        {"parameter array SELECT",
         [&] { test_parameter_array_select(db.handle()); }},
        {"SQL_ATTR_MAX_ROWS",
         [&] { test_max_rows_attribute(db.handle()); }},
        {"data-at-execution cancel recovery",
         [&] { test_data_at_execution_cancel_recovery(db.handle()); }},
        {"diagnostic lifecycle",
         [&] { test_diagnostic_lifecycle(db.handle()); }},
        {"SQLSTATE classes", [&] { test_sqlstates(db.handle()); }},
        {"truncation diagnostics",
         [&] { test_truncation_diagnostics(db.handle()); }},
        {"numeric overflow diagnostics",
         [&] { test_numeric_overflow_diagnostics(db.handle()); }},
        {"missing parameter diagnostics",
         [&] { test_missing_parameter_diagnostics(db.handle()); }},
        {"function sequence diagnostics",
         [&] { test_function_sequence_diagnostics(db.handle()); }},
        {"schema drift recovery",
         [&] { test_schema_drift_recovery(db.handle()); }},
        {"transaction error recovery",
         [&] { test_transaction_error_recovery(db.handle()); }},
        {"unknown charset result metadata",
         [&] { test_unknown_charset_metadata(db.handle()); }},
        {"unreachable connection SQLSTATE",
         [&] { test_unreachable_connection_sqlstate(connection_string); }},
        {"concurrent connections",
         [&] { test_concurrent_connections(connection_string); }},
        {"PREFETCH query rewrite",
         [&] { test_prefetch_query_rewrite(connection_string); }},
        {"PAD_SPACE connection option",
         [&] { test_pad_space_connection(connection_string); }},
        {"empty bookmark fetch",
         [&] { test_empty_bookmark_fetch(connection_string); }},
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
