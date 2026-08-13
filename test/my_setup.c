// Copyright (c) 2003, 2024, Oracle and/or its affiliates.
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

#include "odbctap.h"

/**
  Bug #66548: Driver use the char ';' as separator in attributes string
              instead of the '\0'
*/
DECLARE_TEST(t_bug66548)
{
  /* TODO: remove #ifdef _WIN32 when Linux and MacOS setup is released */
#ifdef _WIN32
  char attrs[8192];
  char drv[128];
  char conn_out[512];
  SQLSMALLINT conn_out_len;
  int i, len;
  HDBC hdbc1;

  /*
    Use ';' as separator because sprintf doesn't work after '\0'
    The last attribute in the list must end with ';'
  */

  len = sprintf(attrs, "DSN=bug66548dsn;SERVER=%s;USER=%s;PASSWORD=%s;"
    "PORT=%d;DATABASE=%s;SSL-MODE=DISABLED;", myserver, myuid, mypwd,
    myport, mydb);
  attrs[len] = '\0';

  len = (int)strlen(attrs);

  /* replacing ';' by '\0' */
  for (i= 0; i < len; ++i)
  {
    if (attrs[i] == ';')
      attrs[i]= '\0';
  }

  /* Adding the extra string termination to get \0\0 */
  attrs[i]= '\0';

  len = (int)strlen(mydriver);

  if (mydriver[0] == '{')
  {
    /* We need to remove {} in the driver name or it will not register */
    memcpy(drv, mydriver+1, sizeof(char)*(len-2));
    drv[len-2]= '\0';
  }
  else
  {
    memcpy(drv, mydriver, sizeof(char)*len);
	drv[len]= '\0';
  }

  /*
    Trying to remove the DSN if it is left from the previous run,
    no need to check the result
  */
  SQLConfigDataSource(NULL, ODBC_REMOVE_DSN, drv, "DSN=bug66548dsn\0\0");

  /* Create the DSN */
  ok_install(SQLConfigDataSource(NULL, ODBC_ADD_DSN, drv, attrs));

  ok_env(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc1));

  /* Try connecting using the newly created DSN */
  ok_con(hdbc1, SQLDriverConnect(
    hdbc1, NULL, SC_NTS("DSN=bug66548dsn"),
    SC_SIZE(conn_out), &conn_out_len, SQL_DRIVER_NOPROMPT
  ));
  ok_con(hdbc1, SQLDisconnect(hdbc1));
  ok_con(hdbc1, SQLFreeHandle(SQL_HANDLE_DBC, hdbc1));
  hdbc1= SQL_NULL_HDBC;

  ok_install(SQLConfigDataSource(NULL, ODBC_REMOVE_DSN, drv, "DSN=bug66548dsn\0\0"));
#endif

  return OK;
}


/**
  A non-interactive Windows DSN update must preserve the DSN and resolve the
  stored driver DLL path back to the registered driver name.
*/
DECLARE_TEST(t_matrixone_config_dsn_update)
{
#ifdef _WIN32
  char drv[128];
  char add_attrs[] = "DSN=matrixone_config_update\0SERVER=localhost\0"
                     "PORT=6001\0DATABASE=before\0\0";
  char update_attrs[] = "DSN=matrixone_config_update\0SERVER=localhost\0"
                        "PORT=6002\0DATABASE=after\0\0";
  char value[64] = {0};
  size_t len = strlen(mydriver);

  if (mydriver[0] == '{')
  {
    memcpy(drv, mydriver + 1, len - 2);
    drv[len - 2] = '\0';
  }
  else
  {
    memcpy(drv, mydriver, len);
    drv[len] = '\0';
  }

  SQLConfigDataSource(NULL, ODBC_REMOVE_DSN, drv,
                      "DSN=matrixone_config_update\0\0");
  ok_install(SQLConfigDataSource(NULL, ODBC_ADD_DSN, drv, add_attrs));
  ok_install(SQLConfigDataSource(NULL, ODBC_CONFIG_DSN, drv, update_attrs));
  is_num(SQLGetPrivateProfileString("matrixone_config_update", "PORT", "",
                                    value, sizeof(value), "ODBC.INI"), 4);
  is_str(value, "6002", 4);
  ok_install(SQLConfigDataSource(NULL, ODBC_REMOVE_DSN, drv,
                                 "DSN=matrixone_config_update\0\0"));
#endif

  return OK;
}


/**
  Bug #24581: Support File DSN
*/
DECLARE_TEST(t_bug24581)
{
  char grant_query[128];
  char conn_in[2048], conn_out[2048], socket_path[512] = {0};
  char conn_fdsn[512];
  char fdsn_path[255];
  SQLSMALLINT conn_out_len;
  HDBC hdbc1;
  HSTMT hstmt1;

  /* We need a user with minimal privileges and no password */
  sprintf(grant_query, "grant usage on %s.* to 'user24851'@'%s'"\
          " identified by 'pass24851'", mydb, myserver);

  ok_stmt(hstmt, SQLExecDirect(hstmt, SC_NTS(grant_query)));

  ok_env(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc1));

#ifdef _WIN32
  sprintf(fdsn_path, "%s\\filedsn24851.dsn", getenv("TEMP"));
#else
  sprintf(fdsn_path, "%s/filedsn24851.dsn", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  if (mysock)
  {
    sprintf(socket_path, "SOCKET=%s", mysock);
  }
  printf("\nSOCKET_PATH=%s", socket_path);
#endif

  sprintf(conn_in, "DRIVER=%s;SERVER=%s;UID=user24851;DATABASE=%s;"\
          "PORT=%d;SAVEFILE=%s;PASSWORD=pass24851;%s",
          mydriver, myserver, mydb, myport, fdsn_path, socket_path);

  /* Create a .dsn file in the TEMP directory, we will remove it later */
  ok_con(hdbc1, SQLDriverConnect(
    hdbc1, NULL, SC_NTS(conn_in),
    SC(conn_out), 512, &conn_out_len, SQL_DRIVER_NOPROMPT
  ));
  /* Not necessary, but keep the driver manager happy */
  ok_con(hdbc1, SQLDisconnect(hdbc1));

  sprintf(conn_fdsn, "FileDSN=%s;PASSWORD=pass24851", fdsn_path);

  /* Connect using the new file DSN */
  ok_con(hdbc1, SQLDriverConnect(
    hdbc1, NULL, SC_NTS(conn_fdsn),
    SC(conn_out), 512, &conn_out_len, SQL_DRIVER_NOPROMPT
  ));

  ok_con(hdbc1, SQLAllocHandle(SQL_HANDLE_STMT, hdbc1, &hstmt1));

  /* just a simple select to verify the server result */
  ok_sql(hstmt1, "select 24851");

  ok_stmt(hstmt1, SQLFetch(hstmt1));
  is_num(my_fetch_int(hstmt1, 1), 24851);

  ok_sql(hstmt, "drop user 'user24851'@'localhost'");
  free_basic_handles(NULL, &hdbc1, &hstmt1);

  /* Remove the file DSN */
  is(remove(fdsn_path)==0);

  return OK;
}


/**
  Bug #17508006:
  FileDSN is created evein if the connection credentials are wrong
*/
DECLARE_TEST(t_bug17508006)
{
  /* TODO: remove #ifdef _WIN32 when Linux and MacOS setup is released */
#ifdef _WIN32
  char conn_in[512], conn_out[512];
  char fdsn_path[255];
  SQLSMALLINT conn_out_len;
  HDBC hdbc1;

  ok_env(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc1));

  sprintf(fdsn_path, "%s\\filedsn17508006.dsn", getenv("TEMP"));
  sprintf(conn_in, "DRIVER=%s;SERVER=%s;UID=user17508006;DATABASE=%s;"\
                   "SAVEFILE=%s;PASSWORD='wrongpassword'",
                   mydriver, myserver, mydb, fdsn_path);

  /* This should result in an error */
  expect_dbc(hdbc1, SQLDriverConnect(
    hdbc1, NULL, SC_NTS(conn_in),
    SC(conn_out), 512, &conn_out_len, SQL_DRIVER_NOPROMPT
  ), SQL_ERROR);

  ok_con(hdbc1, SQLFreeHandle(SQL_HANDLE_DBC, hdbc1));

  /* Removing the file DSN should not be successful */
  is(remove(fdsn_path) == -1);
#endif

  return OK;
}


BEGIN_TESTS
  ADD_TEST(t_bug66548)
  ADD_TEST(t_matrixone_config_dsn_update)
  // ADD_TEST(t_bug24581) TODO: fix
  ADD_TEST(t_bug17508006)
END_TESTS


RUN_TESTS
