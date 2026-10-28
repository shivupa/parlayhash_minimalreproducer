# parlayhash: concurrent `Upsert` during table growth loses entries / segfaults

Tested against cmuparlay/parlayhash `081408ba6111ce78b5add9129f8d902fa2fea58f` (current `main`),
GCC 15.2, x86_64 Linux, 4 cores / 8 threads.

## Reproduce

```sh
cmake -S . -B build && cmake --build build
./build/upsert_resize_repro            # args: [initial_size=1000] [threads=8]
```

Expected:

```
init_size=1000 threads=8 size=200000 (expected 200000) sum=4000000 (expected 4000000) -> OK
```

Observed: segfault on every run (20/20). It crashes reading `table_version::next` (null) in
`check_bucket_and_state`, called from `Upsert`:

```
SEGV on unknown address 0x000000000010
  #0 parlay_hash<...>::table_version::get_index            parlay_hash.h:344
  #1 parlay_hash<...>::check_bucket_and_state              parlay_hash.h:605
  #2 parlay_hash<...>::check_bucket_and_state              (recursive)
  #3 parlay_hash<...>::Upsert ... lambda                    parlay_hash.h:736
```

It does not reproduce when the map is constructed large enough to never grow (e.g.
`./build/upsert_resize_repro 1000000`), or with a single thread. Threads must work on
contiguous chunks of the operation range. With a strided assignment (thread `t` does
`i = t, t + nt, ...`) it does not show up in practice.

## Cause

In `parlay_hash::Upsert` (`include/parlay_hash/parlay_hash.h`):

```cpp
auto [s, tag] = b->ll();
state out_s = s;                                   // (1) copy of the bucket state
copy_if_needed(ht, idx);
check_bucket_and_state(ht, key, b, s, tag, idx);   // (2) may redirect b/s/tag to the next table version
...
if (s.buffer[i].equal(key)) {
  Entry new_e = constr(std::optional(s.buffer[i]));
  out_s.buffer[i] = new_e;
  if (b->sc(tag, out_s)) ...                       // (3) stores the stale out_s into the *new* bucket
```

If the bucket was forwarded during a resize, (2) moves `b`, `s`, and `tag` to the bucket in
the next table version, but `out_s` is still a copy of the old forwarded state
(`list_head == forwarded_val`). If the key is found in the new bucket, (3) succeeds and
stores the forwarded state into a bucket of the table that is still being filled. That bucket
loses all its other entries and now reads as forwarded. That table's `next` is still null, so
the next operation that reaches the bucket dereferences null in `check_bucket_and_state`.
Instrumentation confirms this: the offending new-table bucket has version tag 2 (exactly one
`sc` since `initialize`), and its parent bucket's copy block is `Done`. `copy_bucket` is never
called on the new table.

## Fix

Take `out_s` after `check_bucket_and_state`. See `0001-fix-upsert-forwarded-state.patch`.
It contains the one-line move plus a unit test (`TestMapUpsertWhileGrowing`) that Upserts
existing keys while the table grows. That test segfaults 10/10 without the fix and passes
10/10 with it.

`Upsert` is the only operation that keeps a pre-redirect copy of the state. `Insert`, `Remove`,
and `Find` only use `s` after `check_bucket_and_state`.

To test the fixed tree with this harness:

```sh
cmake -S . -B build-fixed -DFETCHCONTENT_SOURCE_DIR_PARLAYHASH=/path/to/patched/parlayhash
cmake --build build-fixed && ./build-fixed/upsert_resize_repro
```
