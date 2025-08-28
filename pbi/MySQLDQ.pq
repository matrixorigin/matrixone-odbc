/*
 Copyright (c) 2025, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0, as
 published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms, as
 designated in a particular file or component or in included license
 documentation. The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 Without limiting anything contained in the foregoing, this file,
 which is part of Power BI Data extension for MySQL with DirectQuery support,
 is also subject to the
 Universal FOSS Exception, version 1.0, a copy of which can be found at
 https://oss.oracle.com/licenses/universal-foss-exception.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 See the GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

/*
  The main module of Power BI Data extension for MySQL with
  DirectQuery support.

  This Data Dxtension allows Power BI Desktop users to process data
  using DirectQuery technology with MySQL Server. The Data Extension
  utilizes MySQL Connector/ODBC for connecting to MySQL Server.
*/

[Version = "1.0.0"] // https://docs.microsoft.com/en-us/power-query/handlingversioning
section MySQLDQ;

/*
  When set to true, additional trace information will be written out
  to the User log. This should be set to false before release.
  Tracing is done through a call to Diagnostics.LogValue().
  When EnableTraceOutput is set to false, the call becomes a
  no-op and simply returns the original value.
*/

EnableTraceOutput = true;

/*
  When false enables the 'View Native Query' context menu item on a query
  step in Power BI Desktop Query Editor. By default viewing native queries
  is enabled when tracing is enabled. MS samples say: 'Extensions should
  set this to true.'
  https://github.com/microsoft/DataConnectors/blob/master/samples/ODBC/SqlODBC/SqlODBC.pq
*/

Config_HideNativeQuery = not EnableTraceOutput;

// MySQL ODBC Driver supports parameter bindings

Config_UseParameterBindings = true;

MySQLDQInfo = Extension.LoadFunction("MySQLDQ.version.pqm");

ODBCDriver = "MySQL ODBC " & MySQLDQInfo[Version] & " Unicode Driver";  // FIXME: Hardcoded driver name?
DefaultMySQLPort = 3306;
DefaultMySQLHost = "localhost";


// Data Source Kind description

MySQLDQ =
[
  SupportsEncryption = true,
  Authentication = [ UsernamePassword = [] ],

  // TestConnection is required for the Gateway Support:
  // https://docs.microsoft.com/en-us/power-query/handlinggatewaysupport

  TestConnection = (dataSourcePath) =>
    let
      json = Json.Document(dataSourcePath),
      server = json[#"MySQL Server"]
    in
      { "MySQLDQ.Contents", server}
];


// Data Source UI publishing description

MySQLDQ.Publish =
[
  Beta = false,
  SupportsDirectQuery = true,     // enables direct query
  Category = "Database",
  LearnMoreUrl = "https://powerbi.microsoft.com/",
  SourceImage = MySQLDQ.Icons,
  SourceTypeImage = MySQLDQ.Icons,

  ButtonText =
  {
    Extension.LoadString("ButtonTitle"),
    Extension.LoadString("ButtonHelp")
  }
];


Server_FieldCaption = Extension.LoadString("Server_FieldCaption");
Database_FieldCaption = Extension.LoadString("Database_FieldCaption");


/*
  Main function for connecting to a MySL server.

  See usage examples below.

  It parses connection parameters, configures authentication and SSL options,
  constructs the ODBC connection string, and uses Odbc.DataSource to connect
  to to the server and return data source catalog (see `Catalog_description`
  below).

  Also normalizes type and column information for compatibility with Power BI.
  Optionally filters results to a specified database and handles error
  reporting and diagnostics.
*/

Usage1 =
[
  Description = Text.Format(
    "Returns MySQL tables and views from the MySQL database <code>company</code> on server <code>#{0}</code>.",
    {DefaultMySQLHost}
  ),

  Code = Text.Format(
    "MySQLDQ.Contents(""#{0}"", ""company"")",
    {DefaultMySQLHost}
  ),

  Result =
  "#table({""Name"",        ""Description"",         ""Data"",   ""Kind""}, {
       {""departments"", ""Company departments"", #table(), ""Table""},
       {""employees"",   ""Compamy employees"",   #table(), ""Table""},
       {""managers"",    ""Managers"",            #table(), ""View""}
       })"
];

Usage2 =
[
  Description = Text.Format(
    "Returns databases on the specified MySQL server (#{0}) using the default
     port #{1} to connect. Equivalent to <code>MySQLDQ.Contents(""#{0}:#{1}"")</code>.",
    {DefaultMySQLHost, DefaultMySQLPort}
  ),

  Code = Text.Format("MySQLDQ.Contents(""#{0}"")", {DefaultMySQLHost}),

  Result =
    "#table({""Name"",               ""Description"", ""Data"",   ""Kind""}, {
       {""company"",            null,          #table(), ""Table""},
       {""information_schema"", null,          #table(), ""Table""},
       {""mysql"",              null,          #table(), ""View""}
       })"
];

Catalog_description = Text.Format("<p>Navigation Table:</p>
  <ul>
  <li>If <code>databasename</code> parameter is omitted it returns
  a table of databases from the MySQL server <code>servername</code>.
  <li>When <code>databasename</code> parameter is specified it returns
  a table containing tables and views from the MySQL database
      <code>databasename</code> on the server <code>servername</code>.
  </ul>

  <p>The <code>databasename</code> can be given in either of the input
      parameters:</p>
  <ul>
  <li>As <code>'servername:port;database'</code> in <b>#{0}</b>.
      This allows using credentials specific to a particular datbase.
      See details below.
  <li>As <code>'database'</code> in parameter <b>#{1}</b>.
      The same credentials for all databases can be used on the specified
      <b>MySQL Server</b>.
  </ul>
  <p>The first parameter <b>#{0}</b> is identifying a data source in
      Power BI uniquely.</br>
  Different values of the parameter <b>#{0}</b> correspond to independent
  data sources that can have different credentials.</br>

  MySQL Database with Direct Query support extension works with Basic authentication, which makes
  possible to connect with different credentials to databases residing
  on the same MySQL server.</p>",
  {Server_FieldCaption, Database_FieldCaption}
);


MySQLOdbc = (connstr as text, optional db) as table =>
let

  ConnectionOpts = ParseConnectionString(connstr, db),

  ParamBindings =
  if Config_UseParameterBindings = false
  then
  [
    SupportsNumericLiterals = true,
    SupportsStringLiterals = true,
    SupportsOdbcDateLiterals = true,
    SupportsOdbcTimeLiterals = true,
    SupportsOdbcTimestampLiterals = true
  ]
  else [],

  SqlCapabilities = Diagnostics.LogValue("SqlCapabilities_Options",
  [
    LimitClauseKind = LimitClauseKind.LimitOffset,
    SupportsTop = false,
    GroupByCapabilities = ODBC[SQL_GB][SQL_GB_NO_RELATION],
    FractionalSecondsScale = 3
  ]
  & ParamBindings),

  SQLGetInfo = Diagnostics.LogValue("SQLGetInfo_Options",
  [
    SQL_SQL_CONFORMANCE = ODBC[SQL_SC][SQL_SC_SQL92_FULL],
    SQL_CONVERT_FUNCTIONS = ODBC[SQL_FN_CVT][SQL_FN_CVT_CAST]
  ]),

  OdbcDataSource = Odbc.DataSource(ConnectionOpts[Options],
  [
      // A logical (true/false) that sets whether to view the tables grouped by their schema names
      HierarchicalNavigation = true,

      // Prevents execution of native SQL statements. Extensions should set this to true.
      HideNativeQuery = Config_HideNativeQuery,

      // Allows upconversion of numeric types
      SoftNumbers = true,

      // Allow upconversion / resizing of numeric and string types
      TolerateConcatOverflow = true,

      // Enables connection pooling via the system ODBC manager
      ClientConnectionPooling = true,

      CredentialConnectionString = ConnectionOpts[Credentials],

      OnError = OnError,

      SqlCapabilities = SqlCapabilities,
      SQLGetInfo = SQLGetInfo,

      // Note: These are used for type mangling -- see below

      SQLColumns = MySQLColumns,
      SQLGetTypeInfo = MySQLGetTypeInfo
  ])

in

  if db <> null
  then

    // Return one table describing the selected database
    OdbcDataSource{[Name = db]}[Data]

  else

    // Return tables for all databases
    OdbcDataSource;


ParseConnectionString = (connstr as text, db) =>
let
  tokens = Text.Split(connstr, ";"),
  server = tokens{0},

  server_db = if List.Count(tokens) > 1 then tokens{1} else null,

  /*
    To process the connection string properly we use a trick to convert
    it to URI and then use the standard parts for validation.
  */

  parts = Uri.Parts("http://" & server),

  check =
    if parts[Host] <>  ""
      and parts[[Scheme],[Path],[Query],[Fragment],[UserName],[Password]] =
        [Scheme="http", Path="/", Query=[], Fragment="", UserName="", Password=""]
    then
      true
    else
      error Extension.LoadString("Error_InvalidConnstr"),

  /*
    If port is not specified the default value is 80, this needs to be replaced by MySQL default port.
  */

  Port =
    if parts[Port] = 80 and not Text.EndsWith(server, ":80")
    then
      DefaultMySQLPort
    else
      parts[Port],

  Host = parts[Host],

  Database = if db <> null and db <> "" then db else server_db,

  Credential = Extension.CurrentCredential(),

  CredentialRecord =
    if (Credential[AuthenticationKind]?) = "UsernamePassword"
    then
      [ UID = Credential[Username], PWD = Credential[Password] ]
    else
      // Currently only UsernamePassword authentication is supported
      error Extension.LoadString("Error_UidPwdRequired"),

  SSLRecord =
    if (Credential[EncryptConnection]? = true)
    then
      [SSLMODE = "REQUIRED"]
    else
      [SSLMODE = "PREFERRED"]

in
  [
    Options = [
      Driver = ODBCDriver,
      Server = Host,
      Port = Port,
      ApplicationIntent = "readonly"
    ]
    & SSLRecord
    & (if Database <> null then [Database = Database] else []),

    Credentials = CredentialRecord
  ]
 ;


/*
  MapMySQLType() does mapping of MySQL types.
  Fixing of the type names is required because they will be used in
  CAST(... AS TYPENAME) expressions inside SQL queries to the server.
  MySQL Server does not support TYPENAME being 'integer', 'int', 'varchar',
  'enum' and 'set'. Therefore, such types have to be replaced by nearest
  matching type supported for CAST().
*/

MapMySQLType = (MySQLType) =>
if
  List.Contains({"integer", "int"}, MySQLType)
then
  [ TYPE_NAME = "signed" ]
else if
  List.Contains({"varchar", "enum", "set"}, MySQLType)
then
  [ TYPE_NAME = "char", DATA_TYPE = ODBC[SQL_TYPE][WVARCHAR] ]
else if
  "float" = MySQLType
then
  [ TYPE_NAME = "double", DATA_TYPE = ODBC[SQL_TYPE][DOUBLE] ]
else
  null;


/*
  Power BI Desktop will attempt to get type information using SQLGetTypeInfo(),
  which returns MySQL types not supported by Power BI Desktop.
  The function MySQLGetTypeInfo() should act as an intermediate
  layer mapping unsupported MySQL types to closest supported types.
*/

MySQLGetTypeInfo = (types) =>
let

  FixTypeNames = (row) as record =>
  let
    ty = MapMySQLType(row[TYPE_NAME])
  in
    if null <> ty
    then
      row & [ TYPE_NAME = ty[TYPE_NAME], LOCAL_TYPE_NAME = ty[TYPE_NAME] ]
    else
      row
  ,
  FixedRows = Table.TransformRows(types, each FixTypeNames(_)),
  TypeInfo = Table.FromRecords(FixedRows)

  // Note: Printing diagnostics using side effects
  ,
  _ignore = if true <> EnableTraceOutput then null
  else
    Table.TransformRows(TypeInfo,
      each Diagnostics.LogValue("SQLGetTypeInfo " & _[TYPE_NAME], _)
    )

in
  TypeInfo;

/*
  Power BI Desktop will attempt to get column information using SQLColumns(),
  which returns MySQL types not supported by Power BI Desktop.
  The function MySQLColumns() should act as an intermediate
  layer mapping unsupported MySQL types to closest supported types.
*/

MySQLColumns = (catalogName, schemaName, tableName, columnName, source) =>
let

  /*
    Power BI will match the results from the call to MySQLGetTypeInfo() against
    the column type names returned by SQLColumns().
    For this reason the types must be fixed in a similar fashion.
  */

  FixColumns = (row) as record =>
  let
    ty = MapMySQLType(row[TYPE_NAME])
  in
    if ty <> null
    then
      row
      & [TYPE_NAME = ty[TYPE_NAME]]
      & ( if null <> ty[DATA_TYPE]?  then [DATA_TYPE = ty[DATA_TYPE]]  else [] )
    else
      row
  ,
  TransformedRows = Table.TransformRows(source, each FixColumns(_)),

  // get an empty table with the original column data types
  EmptyTableWithColumnTypes = Table.FirstN(source, 0),

  Columns = Table.InsertRows(EmptyTableWithColumnTypes, 0, TransformedRows),

  // Note: Printing diagnostics using side effects

  _ignore = if true <> EnableTraceOutput then null
  else
    let
      _i1 = Diagnostics.LogValue("SQLColumns.TableName", tableName),
      _i2 = Diagnostics.LogValue("SQLColumns.ColumnName", columnName)
    in
      Table.TransformRows(Columns,
        each Diagnostics.LogValue("SQLColumns", _)
      )

in
  Columns;


// ----------------------------------------------------------------------------


// Main entry point declaration.

[DataSource.Kind="MySQLDQ", Publish="MySQLDQ.Publish"]
shared MySQLDQ.Contents = Value.ReplaceType(MySQLOdbc, MySQLDatabaseType);


// Meta-data information for the main entry point `MySQLOdbc`

MySQLDatabaseType = type function
(
  #"MySQL Server" as (type text meta
  [
    Documentation.FieldCaption = Server_FieldCaption,
    Documentation.FieldDescription = Extension.LoadString("Server_FieldDescription"),
    Documentation.SampleValues = {"servername:portnumber;databasename"}
  ]),

  optional database as (type text meta
  [
      Documentation.FieldCaption = Database_FieldCaption,
      Documentation.FieldDescription = Extension.LoadString("Database_FieldDescription"),
      Documentation.SampleValues = {"databasename"}
  ])
)
as table meta
[
  Documentation.Name = Extension.LoadString("GetData_Title"),
  Documentation.LongDescription = Catalog_description,
  Documentation.Examples = { Usage1, Usage2 }
];


MySQLDQ.Icons =
[
  Icon16 = {
    Extension.Contents("MySQLDQ16.png"),
    Extension.Contents("MySQLDQ20.png"),
    Extension.Contents("MySQLDQ24.png"),
    Extension.Contents("MySQLDQ32.png")
  },

  Icon32 = {
    Extension.Contents("MySQLDQ32.png"),
    Extension.Contents("MySQLDQ40.png"),
    Extension.Contents("MySQLDQ48.png"),
    Extension.Contents("MySQLDQ64.png")
  }
];


// ----------------------------------------------------------------------------
// ODBC helpers

/*
  OnError Helper Function is called when error occurs.
  It makes distinction between Authentication (CredentialError)
  and all other errors.
*/

OnError = (errorRecord as record) =>
  let
    OdbcError = errorRecord[Detail][OdbcErrors]{0},
    OdbcErrorMessage = OdbcError[Message],
    OdbcErrorCode = OdbcError[NativeError],
    HasCredentialError = errorRecord[Detail] <> null
      and errorRecord[Detail][OdbcErrors]? <> null
      and OdbcErrorCode <> 0 and OdbcErrorCode <> 7
  in
    if HasCredentialError
    then
      error Extension.CredentialError(Credential.AccessDenied, OdbcErrorMessage)
    else
      error errorRecord;


// OdbcConstants contains numeric constants from the ODBC header files, and a
// helper function to create bitfield values.

ODBC = Extension.LoadFunction("OdbcConstants.pqm");

/*
  Note: Use the `LogValue` function from Diagnostics module if logging is enabled.
*/

Diagnostics = Extension.LoadFunction("Diagnostics.pqm");

Diagnostics.LogValue = if (EnableTraceOutput)
  then
    Diagnostics[LogValue]
  else
    (prefix, value) => value;


/*
  Load and evaluate M code from the named resource.
*/

Extension.LoadFunction = (name as text) =>
  let
    binary = Extension.Contents(name),
    asText = Text.FromBinary(binary)
  in
    Expression.Evaluate(asText, #shared);

