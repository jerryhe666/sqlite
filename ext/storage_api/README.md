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
First build `libsqlite3.a` from the SQLite root (for example, `./configure && make` will emit `.libs/libsqlite3.a`). Then run:

```sh
make SQLITE_LIB=../..//.libs/libsqlite3.a
```

You may point `SQLITE_LIB` at any compatible static SQLite build. The targets here depend on SQLite internals (btree/pager/vdbe), so headers are pulled from the main source tree.

## Status
The API now exercises SQLite's real storage engine: schema discovery walks `sqlite_master` using `sqlite3BtreeCursor`, transactions are driven by `sqlite3BtreeBeginTrans/Commit/Rollback`, and cursor operations call into `sqlite3BtreeInsert/Delete/...`. Record handling remains raw byte copies and needs to be upgraded to SQLite's record format for mixed-type columns.
