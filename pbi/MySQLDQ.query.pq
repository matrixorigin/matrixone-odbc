// Use this file to write queries to test your data connector
let
    //result = MySQLDQ.Contents("localhost")
    connstr = "localhost:3307;tpc",
    tokens = Text.Split(connstr, ";"),
    result = List.First(tokens),
    db = null,
    result2 = Record.Combine({[Driver = "a", Server = "b"],
    if db <> null then [db = db] else []})
in
    result2

