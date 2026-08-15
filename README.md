A calculator library with an emphasis on natural language expressions.

Can do maths, convert between currencies, timezones, and much more...

Although there is some set up for it we don't yet use boost multiprecision to handle big integers. For now everything is a `double`.

## Data

- Currency tables under `src/gen/` are generated from Unicode CLDR supplemental data (`extra/supplementalData.xml`) by `scripts/gen-currency-tables.py`.
- Place name to timezone resolution (`now in paris tx`) uses data from [GeoNames](https://www.geonames.org), licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The extract under `extra/geonames/` is turned into `src/gen/geo-tz-tables.inc` by `scripts/gen-timezone-tables.py`; run it with `extract <dir>` pointing at fresh GeoNames dumps to refresh the extract.
