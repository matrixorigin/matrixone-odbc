// Use this file to write queries to test your data connector
let
    //result = MatrixOne.Contents("localhost")
    connstr = "localhost:6001;tpc",
    tokens = Text.Split(connstr, ";"),
    result = List.First(tokens),
    db = null,
    result2 = Record.Combine({[Driver = "a", Server = "b"],
    if db <> null then [db = db] else []})
in
    result2

