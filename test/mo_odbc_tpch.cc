/*
  Copyright (c) 2026 Matrix Origin.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2.0.

  MatrixOne TPC-H SF1 validation through public ODBC APIs. Load the public
  data set with test/tpch/load_tpch.ps1 before running this executable.
*/

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
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

std::string diagnostics(SQLSMALLINT handle_type, SQLHANDLE handle) {
  std::ostringstream out;
  for (SQLSMALLINT record = 1;; ++record) {
    SQLCHAR state[6] = {};
    SQLCHAR message[2048] = {};
    SQLINTEGER native_error = 0;
    SQLSMALLINT message_length = 0;
    SQLRETURN rc = SQLGetDiagRec(handle_type, handle, record, state,
                                 &native_error, message, sizeof(message),
                                 &message_length);
    if (rc == SQL_NO_DATA || !succeeded(rc)) break;
    out << " SQLSTATE=" << state << " native=" << native_error
        << " message=" << message;
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
                  diagnostics(handle_type, handle));
  }
}

std::vector<SQLWCHAR> widen_ascii(const std::string &text) {
  std::vector<SQLWCHAR> result(text.size() + 1, 0);
  for (size_t i = 0; i < text.size(); ++i) {
    const unsigned char value = static_cast<unsigned char>(text[i]);
    if (value > 0x7f) throw Failure("connection string must be ASCII");
    result[i] = static_cast<SQLWCHAR>(value);
  }
  return result;
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

      auto connection = widen_ascii(connection_string);
      SQLWCHAR completed[4096] = {};
      SQLSMALLINT completed_length = 0;
      rc = SQLDriverConnectW(
          dbc_, nullptr, connection.data(), SQL_NTS, completed,
          static_cast<SQLSMALLINT>(sizeof(completed) / sizeof(completed[0])),
          &completed_length, SQL_DRIVER_NOPROMPT);
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

void exec(SQLHSTMT stmt, const std::string &sql) {
  SQLRETURN rc = SQLExecDirect(
      stmt, reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
      static_cast<SQLINTEGER>(sql.size()));
  check(rc, SQL_HANDLE_STMT, stmt, "SQLExecDirect");
}

int64_t scalar_int(SQLHDBC dbc, const std::string &sql) {
  Statement stmt(dbc);
  exec(stmt.handle(), sql);
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch scalar integer");
  SQLBIGINT value = 0;
  SQLLEN indicator = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_SBIGINT, &value, sizeof(value),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData scalar integer");
  expect(indicator != SQL_NULL_DATA, "scalar integer was NULL");
  return static_cast<int64_t>(value);
}

std::string scalar_text(SQLHDBC dbc, const std::string &sql) {
  Statement stmt(dbc);
  exec(stmt.handle(), sql);
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch scalar text");
  SQLCHAR value[512] = {};
  SQLLEN indicator = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_CHAR, value, sizeof(value),
                   &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData scalar text");
  expect(indicator != SQL_NULL_DATA, "scalar text was NULL");
  return reinterpret_cast<const char *>(value);
}

SQLLEN fetch_count(SQLHSTMT stmt) {
  SQLLEN count = 0;
  for (;;) {
    SQLRETURN rc = SQLFetch(stmt);
    if (rc == SQL_NO_DATA) break;
    check(rc, SQL_HANDLE_STMT, stmt, "SQLFetch row set");
    ++count;
  }
  return count;
}

void test_dataset_counts(SQLHDBC dbc) {
  struct Expected {
    const char *table;
    int64_t rows;
  } expected[] = {{"nation", 25},      {"region", 5},
                  {"supplier", 10000}, {"customer", 150000},
                  {"part", 200000},    {"partsupp", 800000},
                  {"orders", 1500000}, {"lineitem", 6001215}};
  for (const auto &item : expected) {
    const int64_t actual =
        scalar_int(dbc, "SELECT COUNT(*) FROM tpch." + std::string(item.table));
    expect(actual == item.rows,
           std::string(item.table) + " expected " +
               std::to_string(item.rows) + " rows, got " +
               std::to_string(actual));
  }
}

void test_catalog_metadata(SQLHDBC dbc) {
  Statement tables(dbc);
  SQLCHAR catalog[] = "tpch";
  SQLCHAR pattern[] = "%";
  SQLCHAR table_type[] = "TABLE";
  check(SQLTables(tables.handle(), catalog, SQL_NTS, nullptr, 0, pattern,
                  SQL_NTS, table_type, SQL_NTS),
        SQL_HANDLE_STMT, tables.handle(), "SQLTables tpch");
  expect(fetch_count(tables.handle()) == 8,
         "SQLTables did not return the eight TPC-H tables");

  Statement columns(dbc);
  SQLCHAR lineitem[] = "lineitem";
  check(SQLColumns(columns.handle(), catalog, SQL_NTS, nullptr, 0, lineitem,
                   SQL_NTS, pattern, SQL_NTS),
        SQL_HANDLE_STMT, columns.handle(), "SQLColumns tpch.lineitem");
  const SQLLEN column_count = fetch_count(columns.handle());
  expect(column_count == 16,
         "SQLColumns expected 16 lineitem columns, got " +
             std::to_string(column_count));
}

void test_varchar_descriptor(SQLHDBC dbc) {
  Statement stmt(dbc);
  exec(stmt.handle(), "SELECT p_comment FROM tpch.part LIMIT 1");
  SQLCHAR name[128] = {};
  SQLSMALLINT name_length = 0;
  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  check(SQLDescribeCol(stmt.handle(), 1, name, sizeof(name), &name_length,
                       &data_type, &column_size, &decimal_digits, &nullable),
        SQL_HANDLE_STMT, stmt.handle(), "SQLDescribeCol p_comment");
  if (column_size == 23) return;
  if (column_size == 17) {
    throw KnownIssue(
        "matrixorigin/matrixone#26967: VARCHAR(23) result descriptor reports "
        "ColumnSize=17");
  }
  throw Failure("VARCHAR(23) reported unexpected ColumnSize=" +
                std::to_string(column_size));
}

void test_q1_aggregate(SQLHDBC dbc) {
  Statement stmt(dbc);
  exec(stmt.handle(),
       "SELECT l_returnflag, l_linestatus, SUM(l_quantity), "
       "SUM(l_extendedprice), SUM(l_extendedprice * (1-l_discount)), "
       "AVG(l_quantity), COUNT(*) FROM tpch.lineitem "
       "WHERE l_shipdate <= DATE '1998-12-01' - INTERVAL '112' DAY "
       "GROUP BY l_returnflag,l_linestatus "
       "ORDER BY l_returnflag,l_linestatus");
  SQLLEN rows = 0;
  std::string first_flag;
  int64_t first_count = 0;
  for (;;) {
    SQLRETURN rc = SQLFetch(stmt.handle());
    if (rc == SQL_NO_DATA) break;
    check(rc, SQL_HANDLE_STMT, stmt.handle(), "SQLFetch Q1");
    SQLCHAR flag[8] = {};
    SQLBIGINT count = 0;
    SQLLEN indicator = 0;
    check(SQLGetData(stmt.handle(), 1, SQL_C_CHAR, flag, sizeof(flag),
                     &indicator),
          SQL_HANDLE_STMT, stmt.handle(), "SQLGetData Q1 flag");
    check(SQLGetData(stmt.handle(), 7, SQL_C_SBIGINT, &count, sizeof(count),
                     &indicator),
          SQL_HANDLE_STMT, stmt.handle(), "SQLGetData Q1 count");
    if (rows == 0) {
      first_flag = reinterpret_cast<const char *>(flag);
      first_count = static_cast<int64_t>(count);
    }
    ++rows;
  }
  expect(rows == 4, "TPC-H Q1 expected 4 rows, got " + std::to_string(rows));
  expect(first_flag == "A" && first_count == 1478493,
         "TPC-H Q1 first group did not match the SF1 reference result");
}

void test_q3_join(SQLHDBC dbc) {
  Statement stmt(dbc);
  exec(stmt.handle(),
       "SELECT l_orderkey, SUM(l_extendedprice*(1-l_discount)) AS revenue, "
       "o_orderdate, o_shippriority FROM tpch.customer, tpch.orders, "
       "tpch.lineitem WHERE c_mktsegment='HOUSEHOLD' "
       "AND c_custkey=o_custkey AND l_orderkey=o_orderkey "
       "AND o_orderdate < DATE '1995-03-29' "
       "AND l_shipdate > DATE '1995-03-29' "
       "GROUP BY l_orderkey,o_orderdate,o_shippriority "
       "ORDER BY revenue DESC,o_orderdate LIMIT 10");
  check(SQLFetch(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLFetch Q3 first row");
  SQLBIGINT order_key = 0;
  SQLLEN indicator = 0;
  check(SQLGetData(stmt.handle(), 1, SQL_C_SBIGINT, &order_key,
                   sizeof(order_key), &indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLGetData Q3 order key");
  expect(order_key == 2152675, "TPC-H Q3 first order key was " +
                                   std::to_string(order_key));
  SQLLEN rows = 1 + fetch_count(stmt.handle());
  expect(rows == 10, "TPC-H Q3 expected 10 rows");
}

void test_outer_join_nested_query(SQLHDBC dbc) {
  Statement stmt(dbc);
  exec(stmt.handle(),
       "SELECT c_count, COUNT(*) AS custdist FROM (SELECT c_custkey, "
       "COUNT(o_orderkey) AS c_count FROM tpch.customer LEFT OUTER JOIN "
       "tpch.orders ON c_custkey=o_custkey AND "
       "o_comment NOT LIKE '%pending%accounts%' GROUP BY c_custkey) c_orders "
       "GROUP BY c_count ORDER BY custdist DESC,c_count DESC");
  expect(fetch_count(stmt.handle()) == 42,
         "TPC-H Q13 expected 42 distribution rows");
}

void test_prepared_folding_shape(SQLHDBC dbc) {
  Statement stmt(dbc);
  const std::string sql =
      "SELECT o_orderpriority,COUNT(*) FROM tpch.orders "
      "WHERE o_orderdate >= ? AND o_orderdate < ? "
      "GROUP BY o_orderpriority ORDER BY o_orderpriority LIMIT ?";
  check(SQLPrepare(stmt.handle(),
                   reinterpret_cast<SQLCHAR *>(const_cast<char *>(sql.data())),
                   static_cast<SQLINTEGER>(sql.size())),
        SQL_HANDLE_STMT, stmt.handle(), "SQLPrepare folding query");

  SQL_DATE_STRUCT start{1995, 1, 1};
  SQL_DATE_STRUCT end{1996, 1, 1};
  SQLINTEGER limit = 5;
  SQLLEN start_indicator = sizeof(start);
  SQLLEN end_indicator = sizeof(end);
  SQLLEN limit_indicator = 0;
  check(SQLBindParameter(stmt.handle(), 1, SQL_PARAM_INPUT, SQL_C_TYPE_DATE,
                         SQL_TYPE_DATE, 10, 0, &start, sizeof(start),
                         &start_indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter start date");
  check(SQLBindParameter(stmt.handle(), 2, SQL_PARAM_INPUT, SQL_C_TYPE_DATE,
                         SQL_TYPE_DATE, 10, 0, &end, sizeof(end),
                         &end_indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter end date");
  check(SQLBindParameter(stmt.handle(), 3, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 10, 0, &limit, 0, &limit_indicator),
        SQL_HANDLE_STMT, stmt.handle(), "SQLBindParameter limit");
  check(SQLExecute(stmt.handle()), SQL_HANDLE_STMT, stmt.handle(),
        "SQLExecute folding query");
  expect(fetch_count(stmt.handle()) == 5,
         "prepared folding query expected five priority rows");
}

void test_large_result_fetch(SQLHDBC dbc) {
  Statement stmt(dbc);
  exec(stmt.handle(),
       "SELECT l_orderkey,l_extendedprice,l_shipdate,l_comment "
       "FROM tpch.lineitem ORDER BY l_orderkey,l_linenumber LIMIT 100000");
  SQLLEN rows = 0;
  for (;;) {
    SQLRETURN rc = SQLFetch(stmt.handle());
    if (rc == SQL_NO_DATA) break;
    check(rc, SQL_HANDLE_STMT, stmt.handle(), "SQLFetch large result");
    SQLBIGINT order_key = 0;
    SQLCHAR decimal_value[64] = {};
    SQL_DATE_STRUCT ship_date{};
    SQLCHAR comment[256] = {};
    SQLLEN indicator = 0;
    check(SQLGetData(stmt.handle(), 1, SQL_C_SBIGINT, &order_key,
                     sizeof(order_key), &indicator),
          SQL_HANDLE_STMT, stmt.handle(), "SQLGetData large order key");
    check(SQLGetData(stmt.handle(), 2, SQL_C_CHAR, decimal_value,
                     sizeof(decimal_value), &indicator),
          SQL_HANDLE_STMT, stmt.handle(), "SQLGetData large decimal");
    check(SQLGetData(stmt.handle(), 3, SQL_C_TYPE_DATE, &ship_date,
                     sizeof(ship_date), &indicator),
          SQL_HANDLE_STMT, stmt.handle(), "SQLGetData large date");
    check(SQLGetData(stmt.handle(), 4, SQL_C_CHAR, comment, sizeof(comment),
                     &indicator),
          SQL_HANDLE_STMT, stmt.handle(), "SQLGetData large comment");
    expect(order_key > 0 && decimal_value[0] != 0 && ship_date.year >= 1992,
           "large result returned an invalid converted value");
    ++rows;
  }
  expect(rows == 100000, "large result expected 100000 rows, got " +
                             std::to_string(rows));
}

void test_parallel_analytics(const std::string &connection_string) {
  constexpr int threads_count = 8;
  constexpr int iterations = 5;
  std::atomic<int> failures{0};
  std::mutex mutex;
  std::vector<std::string> messages;
  std::vector<std::thread> threads;
  for (int thread_id = 0; thread_id < threads_count; ++thread_id) {
    threads.emplace_back([&, thread_id] {
      try {
        Database db(connection_string);
        for (int iteration = 0; iteration < iterations; ++iteration) {
          const std::string value = scalar_text(
              db.handle(),
              "SELECT SUM(l_extendedprice*l_discount) FROM tpch.lineitem "
              "WHERE l_shipdate >= DATE '1994-01-01' "
              "AND l_shipdate < DATE '1995-01-01' "
              "AND l_discount BETWEEN 0.02 AND 0.04 AND l_quantity < 24");
          if (value != "61660051.7967") {
            throw Failure("Q6 revenue was " + value);
          }
        }
      } catch (const std::exception &error) {
        ++failures;
        std::lock_guard<std::mutex> guard(mutex);
        messages.push_back("thread " + std::to_string(thread_id) + ": " +
                           error.what());
      }
    });
  }
  for (auto &thread : threads) thread.join();
  if (failures != 0) {
    std::ostringstream message;
    message << failures << " parallel workers failed";
    for (const auto &item : messages) message << "; " << item;
    throw Failure(message.str());
  }
}

struct TestCase {
  std::string name;
  std::function<void()> run;
};

}  // namespace

int main() {
  const char *raw_connection_string = std::getenv("MO_ODBC_CONNECTION_STRING");
  if (!raw_connection_string || !*raw_connection_string) {
    std::cerr << "MO_ODBC_CONNECTION_STRING is required" << std::endl;
    return 2;
  }
  const std::string connection_string(raw_connection_string);

  int passed = 0;
  int xfailed = 0;
  int failed = 0;
  try {
    Database db(connection_string);
    std::vector<TestCase> tests = {
        {"TPC-H SF1 row counts", [&] { test_dataset_counts(db.handle()); }},
        {"TPC-H catalog metadata", [&] { test_catalog_metadata(db.handle()); }},
        {"TPC-H VARCHAR descriptor",
         [&] { test_varchar_descriptor(db.handle()); }},
        {"TPC-H Q1 aggregate", [&] { test_q1_aggregate(db.handle()); }},
        {"TPC-H Q3 three-table join", [&] { test_q3_join(db.handle()); }},
        {"TPC-H Q13 outer join and nested query",
         [&] { test_outer_join_nested_query(db.handle()); }},
        {"prepared Power Query folding shape",
         [&] { test_prepared_folding_shape(db.handle()); }},
        {"100000-row typed result fetch",
         [&] { test_large_result_fetch(db.handle()); }},
        {"parallel TPC-H analytics",
         [&] { test_parallel_analytics(connection_string); }},
    };

    std::cout << "1.." << tests.size() << std::endl;
    for (size_t index = 0; index < tests.size(); ++index) {
      const auto started = std::chrono::steady_clock::now();
      try {
        tests[index].run();
        ++passed;
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started);
        std::cout << "ok " << (index + 1) << " - " << tests[index].name
                  << " # elapsed=" << elapsed.count() << "s" << std::endl;
      } catch (const KnownIssue &issue) {
        ++xfailed;
        std::cout << "ok " << (index + 1) << " - " << tests[index].name
                  << " # XFAIL " << issue.what() << std::endl;
      } catch (const std::exception &error) {
        ++failed;
        std::cout << "not ok " << (index + 1) << " - " << tests[index].name
                  << std::endl;
        std::cerr << "# " << error.what() << std::endl;
      }
    }
  } catch (const std::exception &error) {
    std::cerr << "Bail out! connection failure: " << error.what() << std::endl;
    return 1;
  }

  std::cout << "# summary passed=" << passed << " xfailed=" << xfailed
            << " failed=" << failed << std::endl;
  return failed == 0 ? 0 : 1;
}
