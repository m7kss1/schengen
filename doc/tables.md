# TPC-H Table generation notes

Conventions used below:
- `row_index` is the 0-based row position inside the logical table order.
- `order_index` is the 1-based order sequence number (`row_index + 1`).
- `scale_factor` is the TPCH scale factor; it only affects tables with
  scale-dependent row counts

## region

**Keys**
- `r_regionkey`: `row_index` (0..4). The table is a fixed 5-row list.

**Notes**
- Independent of scale factor.

## nation

**Keys**
- `n_nationkey`: `row_index` (0..24), aligned with `kNations` order.
- `n_regionkey`: cumulative sum of `kNations[i].weight` up to `row_index`.

**Notes**
- `kNations` is an ordered list (not weighted sampling). We walk it in order and
  compute `n_regionkey` from cumulative weights as in `distribution.h`.
- Independent of scale factor.

## supplier

**Keys**
- `s_suppkey`: `row_index + 1` (1-based sequential).
- `s_nationkey`: `RandomBoundedInt(seed=110356601, 0..24)`.

**Notes**
- `s_suppkey` is deterministic by position only.
- `s_nationkey` uses a bounded RNG; the mapping is stable under partitioning
  because RNG is advanced per row.

## customer

**Keys**
- `c_custkey`: `row_index + 1` (1-based sequential).
- `c_nationkey`: `RandomBoundedInt(seed=1489529863, 0..24)`.

**Notes**
- `c_custkey` is deterministic by position only.
- `c_nationkey` is a bounded RNG draw; stable across partitions.

## part

**Keys**
- `p_partkey`: `row_index + 1` (1-based sequential).

**Notes**
- Independent of scale factor in formula; scale only controls how many rows are emitted.

## partsupp

**Keys**
- `ps_partkey`: `part_key = part_row_index + 1` (1-based part key).
- `ps_suppkey`: deterministic mapping from `(part_key, supplier_number)` where
  `supplier_number ∈ {0,1,2,3}`.

**Generation shape**
- For each part, exactly 4 partsupp rows are produced (one per `supplier_number`).
- Part-space partitioning is used: partitions are computed in part-space,
  then expanded to 4 rows per part.

**Supplier mapping**
Let `supplier_key_max = 10_000 * scale_factor`.
```
ps_suppkey = ((part_key
              + supplier_number * (supplier_key_max / 4 + (part_key - 1) / supplier_key_max))
             % supplier_key_max) + 1
```
This guarantees 4 distinct supplier keys per part and keeps the key in
`[1, supplier_key_max]`.

## orders

**Keys**
- `o_orderkey`: “sparse” key from the logical order index.
- `o_custkey`: bounded RNG with mortality adjustment.

**o_orderkey (sparse)**
```
low = order_index & ((1 << 3) - 1)   // keep 3 low bits
ok  = order_index
ok >>= 3
ok <<= 2
ok <<= 3
ok += low
```
This matches TPC-H sparse-key behavior (gaps between keys) while remaining
one-to-one with `order_index`.

**o_custkey**
- `max_customer_key = kCustomer.rows_at_sf1 * scale_factor` (min 1)
- `customer_key = RandomBoundedLong(seed=851767375, 1..max_customer_key)`
- If `customer_key % 3 == 0`, adjust by `+1, -1, +2, -2, ...` until non-multiple
  of 3 (bounded to `[1, max_customer_key]`).

## lineitem

**Keys**
- `l_orderkey`: same sparse key as orders for the current `order_index`.
- `l_linenumber`: `1..line_count` within the current order.
- `l_partkey`: `RandomBoundedLong(seed=1808217256, 1..max_part_key)`.
- `l_suppkey`: same deterministic `SelectPartSupplier(part_key, supplier_number)` as partsupp.

**Generation shape**
- Lineitems are generated per order: for each order we draw `line_count`, then emit
  `line_count` rows with `l_linenumber` increasing from 1.
- `max_part_key = kPart.rows_at_sf1 * scale_factor` (min 1).
- `supplier_number = RandomBoundedInt(seed=2095021727, 0..3)` per line; it feeds
  the same supplier mapping as partsupp.

## Scale-factor dependencies (keys only)

- `customer`: affects the upper bound of `o_custkey` in `orders`.
- `part`: affects the upper bound of `l_partkey` in `lineitem` and any part-based
  RNGs in `orders`.
- `supplier`: affects the maximum supplier key used in partsupp/lineitem mapping
  (`10_000 * scale_factor`).
- `region`/`nation`: unaffected by scale factor (fixed-size lists).

## Determinism and partitions

All RNG-based keys use row-aligned generators with `AdvanceRows`/`RowFinished`.
This ensures that any partition `[start_row, end_row)` produces the same keys as
the full, single-threaded run, and different partitions never overlap.
