# Docker

## Image names

- `base`: prebuilt Arrow/Parquet toolchain for `USE_SYSTEM_ARROW=ON`
- `verify-parquet-sf1`: runs row-by-row gtest validation for all SF1 tables

Use the same pattern for future checks:

- `verify-<format>-<scale-factor>`

`base` also embeds reference fixtures from `test/expected` into:

- `/opt/schengen-fixtures` (`SCHENGEN_EXPECTED_DIR`)

## Build base

```bash
docker build -f .docker/Dockerfile.base -t base .
```

## Build verifier

```bash
docker build -f .docker/Dockerfile.verify-parquet-sf1 \
  --build-arg BASE_IMAGE=base \
  -t verify-parquet-sf1 .
```

## Run verifier

```bash
docker run --rm -v "$PWD:/workspace" verify-parquet-sf1
```

If your source is mounted elsewhere:

```bash
docker run --rm -v "$PWD:/src" verify-parquet-sf1 /src
```

The verifier executes these existing gtests by default:

- `RegionGeneratorTest.MatchesReferenceTable`
- `NationGeneratorTest.MatchesReferenceTable`
- `SupplierGeneratorTest.MatchesReferenceTable`
- `PartSuppGeneratorTest.MatchesReferenceTable`
- `PartGeneratorTest.MatchesReferenceTable`
- `CustomerGeneratorTest.MatchesReferenceTable`
- `OrderGeneratorTest.MatchesReferenceTable`
- `LineitemGeneratorTest.MatchesReferenceTable`

To run only one table test, override regex:

```bash
docker run --rm -e TEST_REGEX='^RegionGeneratorTest\.MatchesReferenceTable$' -v "$PWD:/workspace" verify-parquet-sf1
```
