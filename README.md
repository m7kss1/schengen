## Easy start

### Pull the container

```bash
docker pull ghcr.io/m7kss1/schengen:latest
```

### Generate data on the host

The simplest way is to mount a host directory into the container and pass it via `--output-path`

```bash
mkdir -p ./data

docker run --rm \
  -v "$(pwd)/data:/data" \
  ghcr.io/m7kss1/schengen:latest \
  --scale-factor 1 \
  --table region \
  --output-format parquet \
  --output-path /data
```

## Build from source

Initialize submodules before configuring the build. The command is idempotent and can be re-run after checkout:

```bash
git submodule update --init --recursive
```

Then configure and build:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=... \
  -DENABLE_PARQUET=ON \
  -DENABLE_ORC=ON \
  -DENABLE_LANCE=ON \
  -DENABLE_VORTEX=ON

cmake --build build --target schengen_main
```

All bundled dependencies are built from `contrib` submodules. Use `--output-format all` or a comma-separated list such as `--output-format parquet,orc,lance,vortex` to generate several formats in one run; multi-format output is written under per-format subdirectories. For S3-compatible storage, pass an `s3://bucket/prefix` `--output-path` and configure AWS credentials/endpoint through the usual `AWS_*` environment variables

## CLI parameters

### General parameters

| Parameter | Default | Description |
| --- | --- | --- |
| `--help` | - | Show CLI help |
| `--list-tables` | - | Print the list of available `TPC-H` tables |
| `--list-formats` | - | Print the list of supported output formats |
| `--scale-factor <value>` | `1` | Set the dataset scale factor |
| `--output-format <name>` | `parquet` | Select the output format: `parquet`, `orc`, `lance`, `vortex`; comma-separated lists and `all` are supported |
| `--output-path <path-or-uri>` | `.` | Set the output directory or target URI |
| `--table <name...>` | all tables | Select one or more tables, `all` is also supported |
| `--batch-rows <count>` | `131072` | Number of rows per generated batch |

### `Parquet` parameters

| Parameter | Default | Description |
| --- | --- | --- |
| `--parquet-use-threads` | `off` | Enable internal multithreaded `Parquet` column writing |
| `--parquet-row-group-bytes <bytes>` | `7340032` | Target `row group` size in bytes |
| `--parquet-max-row-group-rows <rows>` | `0` | Hard limit for rows per `row group` |
| `--parquet-output-buffer-bytes <bytes>` | `22020096` | Buffered output stream size |
| `--parquet-compression <codec>` | `snappy` | Compression codec: `snappy` or `uncompressed` |

### `ORC` parameters

| Parameter | Default | Description |
| --- | --- | --- |
| `--orc-max-partition-rows <rows>` | `0` | Maximum number of rows in one output partition file |
| `--orc-stripe-bytes <bytes>` | `7340032` | Target `stripe` size in bytes |
| `--orc-max-stripe-rows <rows>` | `0` | Hard limit for rows per `stripe` |
| `--orc-output-buffer-bytes <bytes>` | `22020096` | Buffered output stream size |
| `--orc-compression <codec>` | `snappy` | Compression codec: `snappy`, `zstd`, `lz4`, `zlib`, `uncompressed` |

### `Lance` parameters

| Parameter | Default | Description |
| --- | --- | --- |
| `--lance-target-partition-rows <rows>` | `0` | Logical partition size for generation when writing a single dataset |
| `--lance-max-rows-per-file <rows>` | `0` | Maximum rows per data file |
| `--lance-max-rows-per-group <rows>` | `0` | Maximum rows per row group |
| `--lance-max-bytes-per-file <bytes>` | `0` | Maximum size of one data file in bytes |

### `Vortex` parameters

| Parameter | Default | Description |
| --- | --- | --- |
| `--vortex-target-partition-rows <rows>` | `0` | Logical partition size for generation during output |
| `--vortex-max-partition-rows <rows>` | `0` | Deprecated alias for `--vortex-target-partition-rows` |
| `--vortex-row-block-size <rows>` | `8192` | Row block size used for zoned layout statistics |
| `--vortex-output-buffer-bytes <bytes>` | `16777216` | Output buffer size for local `Vortex` files |

### Write strategy parameters

| Parameter | Default | Description |
| --- | --- | --- |
| `--write-strategy <mode>` | `auto` | Write strategy: `auto`, `parallel-files`, `single-file-ordered` |
| `--write-workers <count>` | `0` | Number of worker threads for partition processing, `0` means auto |
| `--write-queue-capacity <count>` | `8` | Capacity of the bounded queue between generation and writing |

### Progress parameters

| Parameter | Default | Description |
| --- | --- | --- |
| `--verbosity <level>` | `quiet` | CLI progress mode: `quiet` or `verbose` |

## Supported tables

```text
region
nation
supplier
part
partsupp
customer
orders
lineitem
```

## Notes

- To write results to the host, always mount a directory with `-v <host-dir>:<container-dir>` and pass the in-container path via `--output-path`
- If the write strategy is not specified explicitly, use `--write-strategy auto`
- For `S3`-compatible storage, `--output-path` may be a URI such as `s3://bucket/prefix`
