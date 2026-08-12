DROP DATABASE IF EXISTS pqsdk_test;
CREATE DATABASE pqsdk_test;

CREATE TABLE pqsdk_test.NycTaxiData (
  `RecordID` INT,
  `VendorID` INT,
  `lpep_pickup_datetime` DATETIME,
  `lpep_dropoff_datetime` DATETIME,
  `store_and_fwd_flag` BOOL,
  `RatecodeID` INT,
  `PULocationID` INT,
  `DOLocationID` INT,
  `passenger_count` INT,
  `trip_distance` DOUBLE,
  `fare_amount` DOUBLE,
  `extra` DOUBLE,
  `mta_tax` DOUBLE,
  `tip_amount` DOUBLE,
  `tolls_amount` DOUBLE,
  `ehail_fee` DOUBLE,
  `improvement_surcharge` DOUBLE,
  `total_amount` DOUBLE,
  `payment_type` INT,
  `trip_type` INT,
  `congestion_surcharge` DOUBLE
);

CREATE TABLE pqsdk_test.NycTaxiDateData (
  `RecordID` INT NOT NULL,
  `lpep_pickup_date` DATE NOT NULL,
  `lpep_dropoff_date` DATE NOT NULL
);

CREATE TABLE pqsdk_test.TaxiZoneLookup (
  `LocationID` INT,
  `Borough` VARCHAR(100),
  `Zone` VARCHAR(200),
  `service_zone` VARCHAR(100)
);

CREATE TABLE pqsdk_test.misc_table (
  `DATETIMEFIELD` DATETIME,
  `BOOLEANFIELD` BOOL,
  `BIGNUMERICFIELD` DECIMAL(38,18),
  `NUMERICFIELD` DECIMAL(12,6),
  `INTEGERFIELD` INT,
  `STRINGFIELD` VARCHAR(100)
);
