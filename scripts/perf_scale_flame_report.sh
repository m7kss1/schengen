#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

SCALE_FACTORS=("1" "10" "20")
PERF_EVENT="cycles"
PERF_FREQ="199"
OUTPUT_FORMAT="parquet"
REPORT_DIR="${ROOT_DIR}/perf-report-$(date -u +%Y%m%d-%H%M%S)"
BINARY_PATH="${ROOT_DIR}/build/schengen_main"
KEEP_WORKLOAD_OUTPUT=0

TABLES=()
EXTRA_ARGS=()

usage() {
    cat <<USAGE
Usage: $(basename "$0") [options] [-- extra-generator-args]

Collect Linux perf profiles for scale factors 1/10/20, build flamegraph CSV data,
and produce HTML report.

Options:
  --binary PATH               Path to generator binary (default: ${ROOT_DIR}/build/schengen_main)
  --report-dir PATH           Output report directory (default: ${REPORT_DIR})
  --scale-factors LIST        Comma-separated scale factors (default: 1,10,20)
  --event EVENT               perf event (default: cycles)
  --freq N                    perf sample frequency (default: 199)
  --output-format FORMAT      Generator output format (default: parquet)
  --table NAME                Table to generate (can be repeated)
  --keep-workload-output      Keep temporary generated data (by default removed)
  -h, --help                  Show this help

Examples:
  $(basename "$0")
  $(basename "$0") --table lineitem --table orders --freq 499
  $(basename "$0") --binary ./build/schengen_main -- --batch-rows 262144
USAGE
}

require_cmd() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Missing required command: $cmd" >&2
        exit 1
    fi
}

parse_scale_factors() {
    local raw="$1"
    IFS=',' read -r -a SCALE_FACTORS <<< "$raw"
    if [[ "${#SCALE_FACTORS[@]}" -eq 0 ]]; then
        echo "--scale-factors cannot be empty" >&2
        exit 2
    fi
    local sf
    for sf in "${SCALE_FACTORS[@]}"; do
        if [[ ! "$sf" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
            echo "Invalid scale factor: $sf" >&2
            exit 2
        fi
    done
}

html_escape() {
    local value="$1"
    printf '%s' "$value" \
        | sed -e 's/&/\&amp;/g' \
              -e 's/</\&lt;/g' \
              -e 's/>/\&gt;/g' \
              -e 's/\"/\&quot;/g' \
              -e "s/'/\&apos;/g"
}

build_csv_from_folded() {
    local folded_path="$1"
    local csv_path="$2"

    awk '
        BEGIN {
            print "stack,count,percent"
        }
        {
            count = $NF + 0
            $NF = ""
            sub(/[[:space:]]+$/, "", $0)
            stacks[++n] = $0
            counts[n] = count
            total += count
        }
        END {
            for (i = 1; i <= n; i++) {
                stack = stacks[i]
                gsub(/"/, "\"\"", stack)
                pct = (total > 0 ? (counts[i] * 100.0 / total) : 0)
                printf "\"%s\",%d,%.6f\n", stack, counts[i], pct
            }
        }
    ' "$folded_path" > "$csv_path"
}

append_section_top_rows() {
    local report_html="$1"
    local folded_path="$2"

    local total_samples
    total_samples="$(awk '{ sum += $NF } END { printf "%.0f", sum }' "$folded_path")"
    if [[ -z "$total_samples" || "$total_samples" == "0" ]]; then
        printf '<p>No samples captured.</p>\n' >> "$report_html"
        return
    fi

    printf '<table>\n' >> "$report_html"
    printf '<thead><tr><th>#</th><th>Stack</th><th>Samples</th><th>Percent</th></tr></thead>\n' >> "$report_html"
    printf '<tbody>\n' >> "$report_html"

    local rank=0
    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        rank=$((rank + 1))
        if (( rank > 25 )); then
            break
        fi

        local count stack escaped_stack percent
        count="${line##* }"
        stack="${line% ${count}}"
        escaped_stack="$(html_escape "$stack")"
        percent="$(awk -v c="$count" -v t="$total_samples" 'BEGIN { printf "%.4f", (c * 100.0 / t) }')"

        printf '<tr><td>%d</td><td><code>%s</code></td><td>%s</td><td>%s%%</td></tr>\n' \
            "$rank" "$escaped_stack" "$count" "$percent" >> "$report_html"
    done < "$folded_path"

    printf '</tbody>\n' >> "$report_html"
    printf '</table>\n' >> "$report_html"
}

append_csv_preview() {
    local report_html="$1"
    local csv_path="$2"

    printf '<details><summary>flamegraph.csv preview (first 30 rows)</summary>\n' >> "$report_html"
    printf '<pre>\n' >> "$report_html"

    local line=0
    while IFS= read -r raw; do
        line=$((line + 1))
        if (( line > 31 )); then
            break
        fi
        printf '%s\n' "$(html_escape "$raw")" >> "$report_html"
    done < "$csv_path"

    printf '</pre>\n' >> "$report_html"
    printf '</details>\n' >> "$report_html"
}

svg_to_data_uri() {
    local svg_path="$1"
    local payload
    payload="$(base64 < "$svg_path" | tr -d '\n')"
    printf 'data:image/svg+xml;base64,%s' "$payload"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --binary)
            [[ $# -ge 2 ]] || { echo "--binary requires a value" >&2; exit 2; }
            BINARY_PATH="$2"
            shift 2
            ;;
        --report-dir)
            [[ $# -ge 2 ]] || { echo "--report-dir requires a value" >&2; exit 2; }
            REPORT_DIR="$2"
            shift 2
            ;;
        --scale-factors)
            [[ $# -ge 2 ]] || { echo "--scale-factors requires a value" >&2; exit 2; }
            parse_scale_factors "$2"
            shift 2
            ;;
        --event)
            [[ $# -ge 2 ]] || { echo "--event requires a value" >&2; exit 2; }
            PERF_EVENT="$2"
            shift 2
            ;;
        --freq)
            [[ $# -ge 2 ]] || { echo "--freq requires a value" >&2; exit 2; }
            PERF_FREQ="$2"
            shift 2
            ;;
        --output-format)
            [[ $# -ge 2 ]] || { echo "--output-format requires a value" >&2; exit 2; }
            OUTPUT_FORMAT="$2"
            shift 2
            ;;
        --table)
            [[ $# -ge 2 ]] || { echo "--table requires a value" >&2; exit 2; }
            TABLES+=("$2")
            shift 2
            ;;
        --keep-workload-output)
            KEEP_WORKLOAD_OUTPUT=1
            shift
            ;;
        --)
            shift
            EXTRA_ARGS=("$@")
            break
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

require_cmd perf
require_cmd perl
require_cmd awk
require_cmd base64

if [[ ! -x "$BINARY_PATH" ]]; then
    echo "Binary not found or not executable: $BINARY_PATH" >&2
    exit 1
fi
if [[ ! -f "${SCRIPT_DIR}/stackcollapse-perf.pl" ]]; then
    echo "Missing ${SCRIPT_DIR}/stackcollapse-perf.pl" >&2
    exit 1
fi
if [[ ! -f "${SCRIPT_DIR}/flamegraph.pl" ]]; then
    echo "Missing ${SCRIPT_DIR}/flamegraph.pl" >&2
    exit 1
fi

mkdir -p "$REPORT_DIR"

declare -a RUN_COMMANDS=()
declare -a RUN_DIRS=()

echo "Report directory: $REPORT_DIR"
echo "Perf event=${PERF_EVENT}, freq=${PERF_FREQ}, output-format=${OUTPUT_FORMAT}"

for sf in "${SCALE_FACTORS[@]}"; do
    run_dir="${REPORT_DIR}/sf${sf}"
    mkdir -p "$run_dir"

    workload_dir="$(mktemp -d "${TMPDIR:-/tmp}/yatpchgen-perf-sf${sf}-XXXXXX")"
    if (( KEEP_WORKLOAD_OUTPUT == 1 )); then
        workload_dir="${run_dir}/workload-output"
        mkdir -p "$workload_dir"
    fi

    perf_data="${run_dir}/perf.data"
    perf_script="${run_dir}/perf.script"
    folded="${run_dir}/stacks.folded"
    csv="${run_dir}/flamegraph.csv"

    cmd=("$BINARY_PATH" "--scale-factor" "$sf" "--output-format" "$OUTPUT_FORMAT" "--output-path" "$workload_dir")
    case "$OUTPUT_FORMAT" in
        parquet)
            cmd+=("--parquet-compression" "uncompressed")
            ;;
        orc)
            cmd+=("--orc-compression" "uncompressed")
            ;;
    esac
    if [[ "${#TABLES[@]}" -gt 0 ]]; then
        cmd+=("--table" "${TABLES[@]}")
    fi
    if [[ "${#EXTRA_ARGS[@]}" -gt 0 ]]; then
        cmd+=("${EXTRA_ARGS[@]}")
    fi

    cmd_pretty="$(printf '%q ' "${cmd[@]}")"
    RUN_COMMANDS+=("$cmd_pretty")
    RUN_DIRS+=("$run_dir")

    echo "[SF=${sf}] perf record ..."
    perf record -F "$PERF_FREQ" -g -e "$PERF_EVENT" -o "$perf_data" -- "${cmd[@]}" >/dev/null 2>&1

    echo "[SF=${sf}] perf script ..."
    perf script -i "$perf_data" > "$perf_script"

    echo "[SF=${sf}] stackcollapse ..."
    perl "${SCRIPT_DIR}/stackcollapse-perf.pl" "$perf_script" | sort -k2,2nr > "$folded"

    echo "[SF=${sf}] csv ..."
    build_csv_from_folded "$folded" "$csv"

    echo "[SF=${sf}] flamegraph svg ..."
    perl "${SCRIPT_DIR}/flamegraph.pl" --title "Scale Factor ${sf}" "$folded" > "${run_dir}/flamegraph.svg"

    if (( KEEP_WORKLOAD_OUTPUT == 0 )); then
        rm -rf "$workload_dir"
    fi
done

report_html="${REPORT_DIR}/report.html"
run_ts="$(date -u +"%Y-%m-%d %H:%M:%S UTC")"

{
    cat <<HTML
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Perf Flamegraph Report</title>
  <style>
    :root {
      --bg: #f5f7fb;
      --panel: #ffffff;
      --ink: #1b2333;
      --subtle: #63708a;
      --line: #d9e0ef;
      --accent: #1749d8;
    }
    body {
      margin: 0;
      background: linear-gradient(165deg, #eef2ff, var(--bg));
      color: var(--ink);
      font-family: "IBM Plex Sans", "Segoe UI", sans-serif;
      line-height: 1.4;
    }
    main {
      max-width: 1200px;
      margin: 0 auto;
      padding: 24px;
    }
    h1, h2 {
      margin: 0 0 12px;
    }
    .meta {
      color: var(--subtle);
      margin-bottom: 20px;
    }
    .panel {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 12px;
      padding: 16px;
      margin-bottom: 18px;
      box-shadow: 0 8px 22px rgba(17, 24, 39, 0.06);
    }
    table {
      width: 100%;
      border-collapse: collapse;
      font-size: 13px;
    }
    th, td {
      border-bottom: 1px solid var(--line);
      text-align: left;
      padding: 8px;
      vertical-align: top;
    }
    th {
      color: var(--subtle);
      font-weight: 600;
      background: #f8faff;
    }
    code {
      font-family: "JetBrains Mono", "SFMono-Regular", monospace;
      font-size: 12px;
      white-space: normal;
      word-break: break-all;
    }
    .links a {
      margin-right: 12px;
      color: var(--accent);
      text-decoration: none;
      font-weight: 600;
    }
    .links a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
<main>
  <h1>Perf Flamegraph Report</h1>
  <p class="meta">Generated: ${run_ts}</p>

  <section class="panel">
    <h2>Summary</h2>
    <table>
      <thead>
        <tr>
          <th>Scale factor</th>
          <th>Total samples</th>
          <th>Top stack</th>
          <th>Artifacts</th>
        </tr>
      </thead>
      <tbody>
HTML
} > "$report_html"

for idx in "${!SCALE_FACTORS[@]}"; do
    sf="${SCALE_FACTORS[$idx]}"
    run_dir="${RUN_DIRS[$idx]}"
    folded="${run_dir}/stacks.folded"

    total_samples="$(awk '{ sum += $NF } END { printf "%.0f", sum }' "$folded")"
    if [[ -z "$total_samples" ]]; then
        total_samples="0"
    fi

    top_stack="$(awk 'NR==1 { count=$NF; $NF=""; sub(/[[:space:]]+$/, "", $0); print $0 }' "$folded")"
    if [[ -z "$top_stack" ]]; then
        top_stack="(no samples)"
    fi

    escaped_top_stack="$(html_escape "$top_stack")"

    {
        printf '<tr>\n'
        printf '<td><a href="#sf%s">SF%s</a></td>\n' "$sf" "$sf"
        printf '<td>%s</td>\n' "$total_samples"
        printf '<td><code>%s</code></td>\n' "$escaped_top_stack"
        printf '<td class="links"><a href="sf%s/flamegraph.csv">flamegraph.csv</a><a href="sf%s/flamegraph.svg">flamegraph.svg</a>' "$sf" "$sf"
        printf '<a href="sf%s/stacks.folded">folded</a><a href="sf%s/perf.script">perf.script</a><a href="sf%s/perf.data">perf.data</a></td>\n' "$sf" "$sf" "$sf"
        printf '</tr>\n'
    } >> "$report_html"
done

{
    cat <<HTML
      </tbody>
    </table>
  </section>
HTML
} >> "$report_html"

for idx in "${!SCALE_FACTORS[@]}"; do
    sf="${SCALE_FACTORS[$idx]}"
    run_dir="${RUN_DIRS[$idx]}"
    folded="${run_dir}/stacks.folded"
    csv="${run_dir}/flamegraph.csv"
    svg_path="${run_dir}/flamegraph.svg"
    cmd="${RUN_COMMANDS[$idx]}"
    svg_data_uri="$(svg_to_data_uri "$svg_path")"

    escaped_cmd="$(html_escape "$cmd")"

    {
        printf '<section class="panel" id="sf%s">\n' "$sf"
        printf '<h2>Scale Factor %s</h2>\n' "$sf"
        printf '<p><strong>Command:</strong> <code>%s</code></p>\n' "$escaped_cmd"
        printf '<p class="links"><a href="sf%s/flamegraph.csv">flamegraph.csv</a><a href="sf%s/flamegraph.svg">flamegraph.svg</a><a href="sf%s/stacks.folded">stacks.folded</a><a href="sf%s/perf.script">perf.script</a><a href="sf%s/perf.data">perf.data</a></p>\n' "$sf" "$sf" "$sf" "$sf" "$sf"
        printf '<p><object data="%s" type="image/svg+xml" style="width: 100%%; min-height: 560px; border: 1px solid #d9e0ef; border-radius: 8px; background: #fff;"></object></p>\n' "$svg_data_uri"
        printf '<h3>Top 25 Stacks</h3>\n'
    } >> "$report_html"

    append_section_top_rows "$report_html" "$folded"
    append_csv_preview "$report_html" "$csv"

    printf '</section>\n' >> "$report_html"
done

cat <<'HTML' >> "$report_html"
</main>
</body>
</html>
HTML

echo "Report generated: ${report_html}"
