DROP DATABASE IF EXISTS mo_odbc_smoke;
CREATE DATABASE mo_odbc_smoke;

CREATE TABLE mo_odbc_smoke.sales (
  id BIGINT PRIMARY KEY,
  name VARCHAR(64),
  amount DECIMAL(18, 2),
  event_date DATE,
  event_ts DATETIME,
  active BOOLEAN
);

INSERT INTO mo_odbc_smoke.sales VALUES
  (1, '上海', 1234.50, '2026-08-04', '2026-08-04 11:20:30', TRUE),
  (2, 'Power BI', -7.25, NULL, '2026-08-04 12:00:00', FALSE);
