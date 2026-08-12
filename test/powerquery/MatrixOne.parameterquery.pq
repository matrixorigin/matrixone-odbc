let
    Source = MatrixOne.Contents("localhost:6001", "pqsdk_test"),
    NycTaxiData = Source{[Name = "nyctaxidata", Kind = "Table"]}[Data],
    NycTaxiDateData = Source{[Name = "nyctaxidatedata", Kind = "Table"]}[Data],
    TaxiZoneLookup = Source{[Name = "taxizonelookup", Kind = "Table"]}[Data],
    MiscTable = Source{[Name = "misc_table", Kind = "Table"]}[Data]
in
    [
        taxi_table = NycTaxiData,
        taxi_date_table = NycTaxiDateData,
        zone_table = TaxiZoneLookup,
        misc_table = MiscTable
    ]
