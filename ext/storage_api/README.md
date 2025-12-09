# Storage API Prototype

This directory hosts an experimental, SQL-free storage API built directly on SQLite's pager/b-tree/VFS stack. The code is intentionally small and header-driven so the API surface can be iterated without touching the SQL front-end.

## Layout
- `include/storage.h` – public API surface for opening databases, transactions, cursors, and record helpers.
- `record_codec.c/h` – thin record encoder/decoder (currently raw payload copies).
- `schema_service.c/h` – loads `sqlite_master` via the real b-tree layer to resolve root pages and object types.
- `storage_api.c` – wraps SQLite's internal b-tree/pager handles for SQL-free transactions and cursors.
- `examples/kv_demo.c` – a tiny demo showing how the API could be consumed.
- `Makefile` – builds a static library (`libstorage_api.a`) and the demo (`kv_demo`).

## Building
Before building the storage API, generate the SQLite amalgamation and headers from the repo root so `sqliteInt.h`, `btreeInt.h`,
and friends are available to the wrapper:

```sh
./configure
make sqlite3.c sqlite3.h
```

This produces the generated headers under `src/` and the amalgamated sources in the root directory. After that, build
`libsqlite3.a` (for example, `make` or `./configure && make` will emit `.libs/libsqlite3.a`). Then run:

```sh
make SQLITE_LIB=../..//.libs/libsqlite3.a
```

You may point `SQLITE_LIB` at any compatible static SQLite build. The targets here depend on SQLite internals (btree/pager/vdbe), so headers are pulled from the main source tree.

By default the makefile trims the SQLite amalgamation to reduce the object size for this storage-only use case. The `SQLITE_OMIT_FLAGS` variable injects a small-footprint set of compile-time options (omitting extension loading, shared-cache support, trace/progress hooks, compile-option diagnostics, automatic index creation, deprecated APIs, and runtime memory-status tracking). Override `SQLITE_OMIT_FLAGS` if you need to re-enable any of these features or further customize the footprint.

## Status
The API now exercises SQLite's real storage engine: schema discovery walks `sqlite_master` using `sqlite3BtreeCursor`, transactions are driven by `sqlite3BtreeBeginTrans/Commit/Rollback`, and cursor operations call into `sqlite3BtreeInsert/Delete/...`. Record handling remains raw byte copies and needs to be upgraded to SQLite's record format for mixed-type columns.
