/*
  Copyright (c) 2026 Matrix Origin.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2.0.

  Minimal MatrixOne compatibility smoke test. It deliberately uses only the
  public ODBC API so failures can be compared across driver builds and driver
  managers.
*/

#include <sql.h>
#include <sqlext.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static int succeeded(SQLRETURN rc) {
  return rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO;
}

static void diagnostics(SQLSMALLINT handle_type, SQLHANDLE handle) {
  SQLCHAR state[6];
  SQLCHAR message[1024];
  SQLINTEGER native_error;
  SQLSMALLINT message_len;
  SQLSMALLINT record = 1;

  while (SQLGetDiagRec(handle_type, handle, record, state, &native_error,
                       message, sizeof(message), &message_len) == SQL_SUCCESS) {
    fprintf(stderr, "  SQLSTATE=%s native=%d message=%s\n", state,
            (int)native_error, message);
    ++record;
  }
}

static int require_success(const char *operation, SQLRETURN rc,
                           SQLSMALLINT handle_type, SQLHANDLE handle) {
  if (succeeded(rc)) {
    printf("PASS %s\n", operation);
    if (rc == SQL_SUCCESS_WITH_INFO) diagnostics(handle_type, handle);
    return 1;
  }
  fprintf(stderr, "FAIL %s rc=%d\n", operation, (int)rc);
  diagnostics(handle_type, handle);
  ++failures;
  return 0;
}

static void print_info(SQLHDBC dbc, SQLUSMALLINT key, const char *label) {
  SQLCHAR value[256];
  SQLSMALLINT len = 0;
  SQLRETURN rc = SQLGetInfo(dbc, key, value, sizeof(value), &len);
  if (require_success(label, rc, SQL_HANDLE_DBC, dbc))
    printf("INFO %s=%s\n", label, value);
}

static void reset_statement(SQLHSTMT stmt) {
  SQLFreeStmt(stmt, SQL_CLOSE);
  SQLFreeStmt(stmt, SQL_UNBIND);
  SQLFreeStmt(stmt, SQL_RESET_PARAMS);
}

static int get_text(SQLHSTMT stmt, SQLUSMALLINT column, SQLCHAR *buffer,
                    SQLLEN buffer_size) {
  SQLLEN indicator = 0;
  SQLRETURN rc;
  buffer[0] = 0;
  rc = SQLGetData(stmt, column, SQL_C_CHAR, buffer, buffer_size, &indicator);
  if (!succeeded(rc)) {
    fprintf(stderr, "FAIL SQLGetData column=%u rc=%d\n", (unsigned)column,
            (int)rc);
    diagnostics(SQL_HANDLE_STMT, stmt);
    ++failures;
    return 0;
  }
  if (indicator == SQL_NULL_DATA)
    snprintf((char *)buffer, (size_t)buffer_size, "NULL");
  return 1;
}

static void test_type_info(SQLHSTMT stmt) {
  SQLRETURN rc = SQLGetTypeInfo(stmt, SQL_ALL_TYPES);
  SQLLEN rows = 0;
  if (!require_success("SQLGetTypeInfo", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }

  while (succeeded(rc = SQLFetch(stmt))) ++rows;
  if (rc != SQL_NO_DATA) {
    require_success("SQLGetTypeInfo fetch", rc, SQL_HANDLE_STMT, stmt);
  } else if (rows == 0) {
    fprintf(stderr, "FAIL SQLGetTypeInfo returned zero rows\n");
    ++failures;
  } else {
    printf("PASS SQLGetTypeInfo rows=%lld\n", (long long)rows);
  }
  reset_statement(stmt);
}

static void test_tables(SQLHSTMT stmt) {
  SQLCHAR catalog[] = "mo_odbc_smoke";
  SQLCHAR table[] = "sales";
  SQLCHAR kind[] = "TABLE";
  SQLRETURN rc = SQLTables(stmt, catalog, SQL_NTS, NULL, 0, table, SQL_NTS,
                           kind, SQL_NTS);
  SQLLEN rows = 0;
  if (!require_success("SQLTables", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }
  while (succeeded(rc = SQLFetch(stmt))) ++rows;
  if (rc != SQL_NO_DATA) {
    require_success("SQLTables fetch", rc, SQL_HANDLE_STMT, stmt);
  } else if (rows != 1) {
    fprintf(stderr, "FAIL SQLTables expected 1 row, got %lld\n",
            (long long)rows);
    ++failures;
  } else {
    printf("PASS SQLTables rows=1\n");
  }
  reset_statement(stmt);
}

static void test_columns(SQLHSTMT stmt) {
  SQLCHAR catalog[] = "mo_odbc_smoke";
  SQLCHAR table[] = "sales";
  SQLRETURN rc = SQLColumns(stmt, catalog, SQL_NTS, NULL, 0, table, SQL_NTS,
                            NULL, 0);
  SQLCHAR column[256];
  SQLCHAR type_name[256];
  SQLLEN column_len = 0;
  SQLLEN type_len = 0;
  SQLLEN rows = 0;

  if (!require_success("SQLColumns", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }
  rc = SQLBindCol(stmt, 4, SQL_C_CHAR, column, sizeof(column), &column_len);
  if (!require_success("SQLBindCol column name", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }
  rc = SQLBindCol(stmt, 6, SQL_C_CHAR, type_name, sizeof(type_name), &type_len);
  if (!require_success("SQLBindCol type name", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }
  while (succeeded(rc = SQLFetch(stmt))) {
    ++rows;
    printf("COLUMN name=%s type=%s\n", column, type_name);
  }
  if (rc != SQL_NO_DATA) {
    require_success("SQLColumns fetch", rc, SQL_HANDLE_STMT, stmt);
  } else if (rows != 6) {
    fprintf(stderr, "FAIL SQLColumns expected 6 rows, got %lld\n",
            (long long)rows);
    ++failures;
  } else {
    printf("PASS SQLColumns rows=6\n");
  }
  reset_statement(stmt);
}

static void test_query_and_unicode(SQLHSTMT stmt) {
  SQLCHAR query[] =
      "select id,name,amount,event_date,event_ts,active from sales order by id";
  SQLRETURN rc = SQLExecDirect(stmt, query, SQL_NTS);
  SQLLEN rows = 0;
  if (!require_success("SQLExecDirect", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }

  while (succeeded(rc = SQLFetch(stmt))) {
    SQLBIGINT id = 0;
    SQLCHAR name[256];
    SQLCHAR amount[64];
    SQLCHAR event_date[64];
    SQLCHAR event_ts[64];
    SQLCHAR active[16];
    SQLLEN id_indicator = 0;

    rc = SQLGetData(stmt, 1, SQL_C_SBIGINT, &id, sizeof(id), &id_indicator);
    if (!succeeded(rc)) {
      fprintf(stderr, "FAIL SQLGetData id rc=%d\n", (int)rc);
      diagnostics(SQL_HANDLE_STMT, stmt);
      ++failures;
    }
    get_text(stmt, 2, name, sizeof(name));
    get_text(stmt, 3, amount, sizeof(amount));
    get_text(stmt, 4, event_date, sizeof(event_date));
    get_text(stmt, 5, event_ts, sizeof(event_ts));
    get_text(stmt, 6, active, sizeof(active));
    printf("ROW id=%lld name=%s amount=%s date=%s timestamp=%s active=%s\n",
           (long long)id, name, amount, event_date, event_ts, active);
    if (id == 1 && strcmp((const char *)name, "上海") != 0) {
      fprintf(stderr, "FAIL SQL_C_CHAR Unicode value was not UTF-8\n");
      ++failures;
    }
    ++rows;
  }
  if (rc != SQL_NO_DATA) {
    require_success("SQLFetch query", rc, SQL_HANDLE_STMT, stmt);
  } else if (rows != 2) {
    fprintf(stderr, "FAIL query expected 2 rows, got %lld\n", (long long)rows);
    ++failures;
  } else {
    printf("PASS query rows=2\n");
  }
  reset_statement(stmt);

  rc = SQLExecDirect(stmt, (SQLCHAR *)"select name from sales where id=1",
                     SQL_NTS);
  if (!require_success("SQLExecDirect Unicode", rc, SQL_HANDLE_STMT, stmt))
  {
    reset_statement(stmt);
    return;
  }
  if (succeeded(SQLFetch(stmt))) {
    SQLWCHAR name[32];
    SQLLEN len = 0;
    memset(name, 0, sizeof(name));
    rc = SQLGetData(stmt, 1, SQL_C_WCHAR, name, sizeof(name), &len);
    if (require_success("SQL_C_WCHAR fetch", rc, SQL_HANDLE_STMT, stmt)) {
      printf("UNICODE code_units=%04x,%04x bytes=%lld\n", (unsigned)name[0],
             (unsigned)name[1], (long long)len);
      if (name[0] != 0x4e0a || name[1] != 0x6d77) {
        fprintf(stderr, "FAIL SQL_C_WCHAR expected 4e0a,6d77\n");
        ++failures;
      } else {
        printf("PASS SQL_C_WCHAR Unicode value\n");
      }
    }
  } else {
    fprintf(stderr, "FAIL Unicode query returned no row\n");
    ++failures;
  }
  reset_statement(stmt);
}

static void test_parameter_binding(SQLHSTMT stmt) {
  SQLBIGINT id = 2;
  SQLLEN id_len = 0;
  SQLCHAR name[256];
  SQLLEN name_len = 0;
  SQLRETURN rc = SQLPrepare(stmt,
                            (SQLCHAR *)"select name from sales where id = ?",
                            SQL_NTS);
  name[0] = 0;
  if (!require_success("SQLPrepare parameter", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }
  rc = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0,
                        0, &id, 0, &id_len);
  if (!require_success("SQLBindParameter", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }
  rc = SQLExecute(stmt);
  if (!require_success("SQLExecute parameter", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }
  if (!succeeded(SQLFetch(stmt))) {
    fprintf(stderr, "FAIL parameter query returned no row\n");
    ++failures;
  } else {
    rc = SQLGetData(stmt, 1, SQL_C_CHAR, name, sizeof(name), &name_len);
    if (require_success("SQLGetData parameter", rc, SQL_HANDLE_STMT, stmt) &&
        strcmp((const char *)name, "Power BI") == 0) {
      printf("PASS parameter value=%s\n", name);
    } else {
      fprintf(stderr, "FAIL parameter query expected Power BI, got %s\n", name);
      ++failures;
    }
  }
  reset_statement(stmt);
}

static void test_wide_sql(SQLHSTMT stmt) {
  SQLWCHAR query[] = {'s', 'e', 'l', 'e', 'c', 't', ' ', '\'', 0x4e0a,
                      0x6d77, '\'', 0};
  SQLWCHAR value[32];
  SQLLEN len = 0;
  SQLRETURN rc = SQLExecDirectW(stmt, query, SQL_NTS);
  if (!require_success("SQLExecDirectW Unicode", rc, SQL_HANDLE_STMT, stmt)) {
    reset_statement(stmt);
    return;
  }
  if (!succeeded(SQLFetch(stmt))) {
    fprintf(stderr, "FAIL wide SQL returned no row\n");
    ++failures;
    reset_statement(stmt);
    return;
  }
  memset(value, 0, sizeof(value));
  rc = SQLGetData(stmt, 1, SQL_C_WCHAR, value, sizeof(value), &len);
  if (require_success("SQLExecDirectW result", rc, SQL_HANDLE_STMT, stmt) &&
      value[0] == 0x4e0a && value[1] == 0x6d77) {
    printf("PASS SQLExecDirectW Unicode value\n");
  } else {
    fprintf(stderr, "FAIL SQLExecDirectW expected 4e0a,6d77 got %04x,%04x\n",
            (unsigned)value[0], (unsigned)value[1]);
    ++failures;
  }
  reset_statement(stmt);
}

int main(void) {
  const char *connection_string = getenv("MO_ODBC_CONNECTION_STRING");
  const char *expected_sqlstate = getenv("MO_ODBC_EXPECT_SQLSTATE");
  SQLHENV env = SQL_NULL_HENV;
  SQLHDBC dbc = SQL_NULL_HDBC;
  SQLHSTMT stmt = SQL_NULL_HSTMT;
  SQLWCHAR wide_connection_string[4096];
  SQLWCHAR completed[2048];
  SQLSMALLINT completed_len = 0;
  SQLRETURN rc;
  size_t connection_len;

  if (!connection_string || !*connection_string) {
    fprintf(stderr, "MO_ODBC_CONNECTION_STRING is required\n");
    return 2;
  }
  connection_len = strlen(connection_string);
  if (connection_len >= sizeof(wide_connection_string) /
                            sizeof(wide_connection_string[0])) {
    fprintf(stderr, "MO_ODBC_CONNECTION_STRING is too long\n");
    return 2;
  }
  for (size_t i = 0; i <= connection_len; ++i)
    wide_connection_string[i] = (unsigned char)connection_string[i];

  rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
  if (!succeeded(rc)) return 2;
  rc = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
  if (!require_success("SQLSetEnvAttr ODBC3", rc, SQL_HANDLE_ENV, env)) goto done;
  rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
  if (!require_success("SQLAllocHandle DBC", rc, SQL_HANDLE_ENV, env)) goto done;
  rc = SQLSetConnectAttr(dbc, SQL_ATTR_LOGIN_TIMEOUT,
                         (SQLPOINTER)(uintptr_t)10, 0);
  if (!require_success("SQL_ATTR_LOGIN_TIMEOUT", rc, SQL_HANDLE_DBC, dbc))
    goto done;
  rc = SQLDriverConnectW(dbc, NULL, wide_connection_string, SQL_NTS,
                         completed,
                         sizeof(completed) / sizeof(completed[0]),
                         &completed_len, SQL_DRIVER_NOPROMPT);
  if (!succeeded(rc) && expected_sqlstate && *expected_sqlstate) {
    SQLCHAR state[6];
    SQLCHAR message[1024];
    SQLINTEGER native_error = 0;
    SQLSMALLINT message_len = 0;
    SQLRETURN diag_rc = SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 1, state,
                                     &native_error, message, sizeof(message),
                                     &message_len);
    if (succeeded(diag_rc) &&
        strcmp((const char *)state, expected_sqlstate) == 0) {
      printf("PASS SQLDriverConnect expected failure SQLSTATE=%s native=%d "
             "message=%s\n", state, (int)native_error, message);
      goto done;
    }
    fprintf(stderr, "FAIL SQLDriverConnect expected SQLSTATE=%s\n",
            expected_sqlstate);
    diagnostics(SQL_HANDLE_DBC, dbc);
    ++failures;
    goto done;
  }
  if (succeeded(rc) && expected_sqlstate && *expected_sqlstate) {
    fprintf(stderr, "FAIL SQLDriverConnect unexpectedly succeeded; expected "
                    "SQLSTATE=%s\n", expected_sqlstate);
    ++failures;
    goto done;
  }
  if (!require_success("SQLDriverConnect", rc, SQL_HANDLE_DBC, dbc)) goto done;

  print_info(dbc, SQL_DBMS_NAME, "SQL_DBMS_NAME");
  print_info(dbc, SQL_DBMS_VER, "SQL_DBMS_VER");
  print_info(dbc, SQL_DRIVER_NAME, "SQL_DRIVER_NAME");
  print_info(dbc, SQL_DRIVER_VER, "SQL_DRIVER_VER");

  rc = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
  if (!require_success("SQLAllocHandle STMT", rc, SQL_HANDLE_DBC, dbc)) goto done;
  rc = SQLSetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT,
                      (SQLPOINTER)(uintptr_t)30, 0);
  if (!require_success("SQL_ATTR_QUERY_TIMEOUT", rc, SQL_HANDLE_STMT, stmt))
    goto done;
  test_type_info(stmt);
  test_tables(stmt);
  test_columns(stmt);
  test_query_and_unicode(stmt);
  test_parameter_binding(stmt);
  test_wide_sql(stmt);

done:
  if (stmt != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, stmt);
  if (dbc != SQL_NULL_HDBC) {
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
  }
  if (env != SQL_NULL_HENV) SQLFreeHandle(SQL_HANDLE_ENV, env);

  if (failures) {
    fprintf(stderr, "RESULT FAIL failures=%d\n", failures);
    return 1;
  }
  printf("RESULT PASS\n");
  return 0;
}
