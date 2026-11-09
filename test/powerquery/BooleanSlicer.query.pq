// Uses the public Power Query test fixture loaded by load_powerquery.ps1.
// Run with PQTest run-test -p -e MatrixOne.mez -q this-file --failOnFoldingFailure.
// Expected distinct values: 0 and 1 (plus null if present in the fixture).
let
    Source = MatrixOne.Contents("localhost:6001", "pqsdk_test"),
    Taxi = Source{[Name = "nyctaxidata", Kind = "Table"]}[Data],
    Projected = Table.SelectColumns(Taxi, {"store_and_fwd_flag"}),
    Converted = Table.TransformColumnTypes(Projected, {{"store_and_fwd_flag", Int64.Type}}),
    Result = Table.FirstN(Table.Sort(Table.Distinct(Converted), {{"store_and_fwd_flag", Order.Ascending}}), 501)
in
    Result
