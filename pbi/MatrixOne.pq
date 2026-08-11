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
  MatrixOne modifications copyright (c) 2026 Matrix Origin.

  The main module of the Power BI Data extension for MatrixOne with
  DirectQuery support.

  This thin fork keeps the upstream MySQL Connector/ODBC protocol and
  metadata implementation, while providing MatrixOne defaults and a
  separately named Power BI data source.
*/

[Version = "1.0.0"] // https://docs.microsoft.com/en-us/power-query/handlingversioning
section MatrixOne;

/*
  When set to true, additional trace information will be written out
  to the User log. This should be set to false before release.
  When EnableTraceOutput is set to false, the `LogMsg` function used
  for tracing becomes a no-op.
*/

EnableTraceOutput = false;

/*
  Additional switch for enabling/disabling verbose traces from
  the SQLGetTypeInfo() hook. Has effect only when `EnableTraceOutput`
  is true.
*/

TraceTypeInfo = true;

/*
  When false enables the 'View Native Query' context menu item on a query
  step in Power BI Desktop Query Editor. By default viewing native queries
  is enabled when tracing is enabled. MS samples say: 'Extensions should
  set this to true.'
  https://github.com/microsoft/DataConnectors/blob/master/samples/ODBC/SqlODBC/SqlODBC.pq
*/

Config_HideNativeQuery = not EnableTraceOutput;

// MatrixOne ODBC inherits parameter binding support from the upstream driver.

Config_UseParameterBindings = true;


// Note: The .version.pqm file is generated during cmake configuration step.

MatrixOneInfo = Extension.LoadFunction("MatrixOne.version.pqm");

/*
  Keep this name synchronized with the Windows and Unix driver registration.
*/

ODBCDriver = "MatrixOne ODBC " & MatrixOneInfo[Version] & " Unicode Driver";
DefaultMatrixOnePort = 6001;
DefaultMatrixOneHost = "localhost";


// Data Source Kind description

MatrixOne =
[
  SupportsEncryption = true,
  Authentication = [ UsernamePassword = [] ],

  // TestConnection is required for the Gateway Support:
  // https://docs.microsoft.com/en-us/power-query/handlinggatewaysupport

  TestConnection = (dataSourcePath) =>
    let
      json = Json.Document(dataSourcePath),
      server = json[#"MatrixOne Server"]
    in
      { "MatrixOne.Contents", server}
];


// Data Source UI publishing description

MatrixOne.Publish =
[
  Beta = false,
  SupportsDirectQuery = true,     // enables direct query
  Category = "Database",
  LearnMoreUrl = "https://powerbi.microsoft.com/",
  SourceImage = MatrixOne.Icons,
  SourceTypeImage = MatrixOne.Icons,

  ButtonText =
  {
    Extension.LoadString("ButtonTitle"),
    Extension.LoadString("ButtonHelp")
  }
];


Server_FieldCaption = Extension.LoadString("Server_FieldCaption");
Database_FieldCaption = Extension.LoadString("Database_FieldCaption");


/*
  Main function for connecting to a MatrixOne server.

  See usage examples below.

  It parses connection parameters, configures authentication and SSL options,
  constructs the ODBC connection string, and uses Odbc.DataSource to connect
  to to the server and return data source catalog (see `Catalog_description`
  below).

  Also normalizes type and column information for compatibility with Power BI
  using hooks defined below. Optionally filters results to a specified database
  and handles error reporting and diagnostics.
*/

Usage1 =
[
  Description = Text.Format(
    "Returns MatrixOne tables and views from the database <code>company</code> on server <code>#{0}</code>.",
    {DefaultMatrixOneHost}
  ),

  Code = Text.Format(
    "MatrixOne.Contents(""#{0}"", ""company"")",
    {DefaultMatrixOneHost}
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
    "Returns databases on the specified MatrixOne server (#{0}) using the default
     port #{1} to connect. Equivalent to <code>MatrixOne.Contents(""#{0}:#{1}"")</code>.",
    {DefaultMatrixOneHost, DefaultMatrixOnePort}
  ),

  Code = Text.Format("MatrixOne.Contents(""#{0}"")", {DefaultMatrixOneHost}),

  Result =
    "#table({""Name"",               ""Description"", ""Data"",   ""Kind""}, {
       {""company"",            null,          #table(), ""Table""},
       {""information_schema"", null,          #table(), ""Table""},
       {""system"",             null,          #table(), ""View""}
       })"
];

Catalog_description = Text.Format("<p>Navigation Table:</p>
  <ul>
  <li>If <code>databasename</code> parameter is omitted it returns
  a table of databases from the MatrixOne server <code>servername</code>.
  <li>When <code>databasename</code> parameter is specified it returns
  a table containing tables and views from the MatrixOne database
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
      <b>MatrixOne Server</b>.
  </ul>
  <p>The first parameter <b>#{0}</b> is identifying a data source in
      Power BI uniquely.</br>
  Different values of the parameter <b>#{0}</b> correspond to independent
  data sources that can have different credentials.</br>

  MatrixOne Database with DirectQuery support works with Basic authentication, which makes
  possible to connect with different credentials to databases residing
  on the same MatrixOne server.</p>",
  {Server_FieldCaption, Database_FieldCaption}
);


MatrixOneOdbc = (connstr as text, optional db) as table =>
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

  SqlCapabilities = LogMsg("SqlCapabilities_Options: #{0}",
  [
    // LimitOffset also requires OFFSET without LIMIT, which MatrixOne does not
    // support yet (matrixorigin/matrixone#26769).
    LimitClauseKind = LimitClauseKind.Limit,
    SupportsTop = false,
    GroupByCapabilities = ODBC[SQL_GB][SQL_GB_NO_RELATION],
    FractionalSecondsScale = 3
  ]
  & ParamBindings),

  SQLGetInfo = LogMsg("SQLGetInfo_Options: #{0}",
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

      SqlCapabilities = SqlCapabilities,
      SQLGetInfo = SQLGetInfo,

      // Hooks

      OnError = OnError,
      SQLColumns = MatrixOneColumns,
      SQLGetTypeInfo = MatrixOneGetTypeInfo
  ])

in

  if db <> null
  then

    // Return one table describing the selected database
    OdbcDataSource{[Name = db]}[Data]

  else

    // Return tables for all databases
    OdbcDataSource;


/*
  Parse connection string and return record with fields:

  Credentials  -- user credentials extracted from the connection string
    (`UID`, `PWD`)

  Options -- record with extracted connection options including target
    `Server`, `Port` and `Driver` (ODBC driver name).
*/

ParseConnectionString = (connstr as text, db) =>
let

  tokens = Text.Split(connstr, ";"),
  server = tokens{0},

  server_db = if List.Count(tokens) > 1 then tokens{1} else null,

  /*
    To process the connection string properly we use a trick to convert
    it to URI and then use the standard URI parts for validation.
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
    If port is not specified Uri.Parts returns 80; replace it with the MatrixOne default port.
  */

  port =
    if parts[Port] = 80 and not Text.EndsWith(server, ":80")
    then
      DefaultMatrixOnePort
    else
      parts[Port],

  host = parts[Host],

  db_name = if db <> null and db <> "" then db else server_db,
  db_record =
    if null = db_name then [] else [ Database = db_name],

  cred = Extension.CurrentCredential(),

  cred_record =
    if (cred[AuthenticationKind]?) = "UsernamePassword"
    then
      [ UID = cred[Username], PWD = cred[Password] ]
    else
      // Currently only UsernamePassword authentication is supported
      error Extension.LoadString("Error_UidPwdRequired"),

  ssl_opts =
    if (cred[EncryptConnection]? = true)
    then
      [SSLMODE = "REQUIRED"]
    else
      [SSLMODE = "PREFERRED"]

in
  [
    Options = [
      Driver = ODBCDriver,
      Server = host,
      Port = port,
      ApplicationIntent = "readonly"
    ]
    & ssl_opts & db_record,

    Credentials = cred_record
  ]
;


/*
  Given a metadata record, such as from MatrixOneColumns(), return updates that
  should be applied to the meta-data or null if no updates are needed.

  We update reported type names because PBI mashup engine often constructs
  SQL queries with CAST() clauses using reported type names. This could lead
  to incorrect queries because the MySQL-compatible protocol exposes type names
  that are not always legal in CAST(). For that reason we normalize reported
  names, for example CHAR instead of VARCHAR.
*/

md_fixes = (md as record) as nullable record =>
let

  ty_id = md[DATA_TYPE],
  ty_name = md[TYPE_NAME],
  // MatrixOne versions differ in the casing used by INFORMATION_SCHEMA.
  // Match type names case-insensitively while preserving the original record.
  ty_name_lc = Text.Lower(ty_name),

  fixes0 =
  if
    Text.Contains(ty_name_lc, "int")
  then
    /*
      Note: The only integer types allowed in a CAST() expression
      are SIGNED INTEGER and UNSIGNED INTEGER where INTEGER part is optional.
    */
    [
      TYPE_NAME =
      if Text.Contains(ty_name_lc, "unsigned") then "unsigned" else "signed"
    ]

  else if
    /*
      Note: Doing similar renaming for TEXT type and similar seems to
      not be needed and also leads to other issues.
    */
    List.Contains({"long varchar", "varchar", "enum", "set"}, ty_name_lc)
  then
    /*
      Note: The only string type allowed in a CAST() expression is CHAR.
    */
    [ TYPE_NAME = "char", DATA_TYPE = ODBC[SQL_TYPE][WVARCHAR] ]

  else if
    "float" = ty_name_lc
  then
    [ TYPE_NAME = "double", DATA_TYPE = ODBC[SQL_TYPE][DOUBLE] ]

  else
    null
  ,

  /*
    Note: Do not apply the changes if the changed name is identical
    to the original one.
  */

  fixes =
  if null = fixes0 or md[TYPE_NAME] = fixes0[TYPE_NAME]?
  then null
  else
    /*
      Note: Ensure that LOCAL_TYPE_NAME is the same as TYPE_NAME after
      the changes. This assumes that fixes always contain TYPE_NAME field.
    */
    fixes0 & [ LOCAL_TYPE_NAME = fixes0[TYPE_NAME] ]

in
  fixes
;


/*
  The SQLGetTypeInfo() hook. It mangles reported type names as implemented
  by `md_fixes` function.
*/

MatrixOneGetTypeInfo = (type_info) =>
let

  fix_names = (ti as record) as record =>
  let

    fixes0 = md_fixes(ti),

    fixes =
      if null = fixes0 then null
      else
        // Note: For GetTypeInfo do not change the reported type id
        if Record.HasFields(fixes0, "DATA_TYPE")
        then Record.RemoveFields(fixes0, "DATA_TYPE")
        else fixes0
    ,

    fixed = ti & (fixes ?? []),

    traced = if not TraceTypeInfo then fixed
      else
        LogMsg(
          "SQLGetTypeInfo for [ NAME = #{1}, TYPE = #{2}, ID = #{3}] -> #{4}",
          fixed,
          {
            ti[TYPE_NAME], ti[DATA_TYPE], sql_type_name(ti[DATA_TYPE]),
            fixes
          }
        )

  in
    traced
  ,

  fixed_rows = Table.TransformRows(type_info, fix_names),
  fixed = Table.FromRecords(fixed_rows)

in
  if not TraceTypeInfo then fixed
  else
    // Note: Add header to distinguish different invocations in the trace.
    LogMsg("SQLGetTypeInfo()", () => fixed, {}, true)
;


/*
  The SQLColumns() hook. Applies changes implemented by `md_fixes` to the
  original column meta-data before returning it to PBI.
*/

MatrixOneColumns = (catalogName, schemaName, tableName, columnName, md) =>
let

  fix_md = (md) as record =>
  let
    fixes = md_fixes(md),
    fixed = md & (fixes ?? [])
  in
    LogMsg(
      "SQLColumns for #{1}.#{2} (#{3}):#(lf)#{0}",
      fixed, {md[TABLE_NAME], md[COLUMN_NAME], md[TYPE_NAME]}
    )
  ,

  fixed_rows = Table.TransformRows(md, fix_md),

  // get an empty table with the original column data types
  empty = Table.FirstN(md, 0),
  fixed = Table.InsertRows(empty, 0, fixed_rows)

in
  fixed
;


// ----------------------------------------------------------------------------


// Main entry point declaration.

[DataSource.Kind="MatrixOne", Publish="MatrixOne.Publish"]
shared MatrixOne.Contents = Value.ReplaceType(MatrixOneOdbc, MatrixOneDatabaseType);


// Meta-data information for the main entry point `MatrixOneOdbc`

MatrixOneDatabaseType = type function
(
  #"MatrixOne Server" as (type text meta
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


MatrixOne.Icons =
[
  Icon16 = {
    Extension.Contents("MatrixOne16.png"),
    Extension.Contents("MatrixOne20.png"),
    Extension.Contents("MatrixOne24.png"),
    Extension.Contents("MatrixOne32.png")
  },

  Icon32 = {
    Extension.Contents("MatrixOne32.png"),
    Extension.Contents("MatrixOne40.png"),
    Extension.Contents("MatrixOne48.png"),
    Extension.Contents("MatrixOne64.png")
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
    OdbcErrors = try errorRecord[Detail][OdbcErrors] otherwise null,
    OdbcError =
      if OdbcErrors <> null and not List.IsEmpty(OdbcErrors)
      then OdbcErrors{0}
      else null,
    OdbcErrorCode =
      if OdbcError <> null then OdbcError[NativeError]? else null,

    // Only authentication/authorization failures should invalidate saved
    // credentials. Treating every non-zero native error as a credential error
    // makes syntax, metadata, and query failures prompt for a password again.
    HasCredentialError = List.Contains({1044, 1045}, OdbcErrorCode)
  in
    if HasCredentialError
    then
      error Extension.CredentialError(
        Credential.AccessDenied,
        OdbcError[Message]
      )
    else
      error errorRecord;


// OdbcConstants contains numeric constants from the ODBC header files, and a
// helper function to create bitfield values.

ODBC = Extension.LoadFunction("OdbcConstants.pqm");
ODBCTypes = Record.ToTable(ODBC[SQL_TYPE]);

/*
  Return name of SQL_TYPE constant corresponding to the given numeric
  type id value. This is used for diagnostic messages.
*/

sql_type_name = (type_id) as text =>
let
  names =  Table.SelectRows(ODBCTypes, each [Value] = type_id),
  name = names{0}[Name]
in
  name
;

/*
  Usage:

    LogMsg(fmt, val, args)              // <1>
    LogMsg(fmt, () => val, args, true)  // <2>
    LogMsg(fmt, val)                    // <3>

  This evaluates to `val` but also puts a message in the trace log formatted
  using format string `fmt` with the value passed as the first format argument
  (accessed as `#{0}` in the format string) and `args` as additional format
  arguments if any (accessed as `#{1}`, #{2}` etc. in the format string).
  Invocation <3> is without any additional format arguments.

  Note: The value is evaluated before constructing the trace message
  -- if value evaluation produces other trace messages they will appear
  in the log before the main one. To change that use invocation <2> which
  first outputs the main message and then evaluates the value.
*/

LogMsg = (fmt, val, optional extra, optional wait) =>
  if not EnableTraceOutput then
    if wait ?? false then val() else val
  else
    let
      to_txt = (v) as text => try Diagnostics[ValueToText](v) otherwise "<!?>",
      args0 = (if wait ?? false then {} else {val}) & (extra ?? {}),
      args = List.Transform(args0, to_txt),
      msg = Text.Format(
        fmt, (if wait ?? false then {"<delayed>"} else {}) & args
      )
    in
      Diagnostics.Trace(TraceLevel.Information, msg, val, wait)
;

Diagnostics = Extension.LoadFunction("Diagnostics.pqm");


/*
  Load and evaluate M code from the named resource.
*/

Extension.LoadFunction = (name as text) =>
  let
    binary = Extension.Contents(name),
    asText = Text.FromBinary(binary)
  in
    Expression.Evaluate(asText, #shared);

