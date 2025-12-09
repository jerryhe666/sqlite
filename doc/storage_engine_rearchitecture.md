# SQLite Storage Engine Rearchitecture Plan

This document proposes a "complete refactor" path to reuse SQLite's storage engine (pager + b-tree + VFS) without the SQL front-end. The focus is on defining a thin, stable API over the existing internal modules so they can be embedded as a standalone storage library.

## Goals
- Preserve SQLite's on-disk format, transactions, WAL/rollback safety, and locking semantics.
- Remove dependency on SQL parsing/optimization/VDBE opcode execution while retaining durability and concurrency controls.
- Provide a small C API for table/index access, schema metadata, and transactional operations.
- Keep the refactor incremental: isolate boundaries first, then peel away SQL-only code.

## Architectural Boundaries
1. **Core Storage (keep largely intact)**
   - Pager: `src/pager.c`, `src/pager.h` (page cache, journaling, WAL, locks).
   - B-Tree: `src/btree.c`, `src/btree.h` (table/index pages, free-list, cell layouts).
   - VFS: `src/os_*.c`, `src/vfs.c` (file I/O abstraction).
2. **New Access Layer (to be added)**
   - Purpose: provide CRUD primitives over B-Tree tables/indexes without VDBE.
   - Components:
     - **Handle types**: database connection, transaction, cursor abstractions.
     - **Schema service**: parses/writes `sqlite_master` entries, owns root-page allocation, ensures compatibility with page formats.
     - **Record codec**: reuse `src/vdbeInt.h` record packing/unpacking logic, lifted into a standalone module to avoid VDBE dependency.
   - Suggested location: new directory `src/storage_api/` or `ext/storage_api/` with headers under `include/`.
3. **Compatibility Shims (temporary)**
   - Small wrappers that allow existing internal calls expecting `sqlite3`/`BtShared`/`Pager` structures to compile while new API evolves.
   - Gradual replacement of VDBE-only utilities (`sqlite3VdbeMem*`, opcode helpers) with storage-focused equivalents.

## Refactor Phases
1. **Boundary Identification**
   - Extract headers for pager/btree/vfs into stable includes; remove unrelated SQL macros (`SQLITE_OMIT_*` guard where possible).
   - Introduce a `storage_context` struct that wraps `sqlite3` fields used by pager/btree (e.g., mutexes, lookaside settings, error logging).
2. **Record & Schema Modules**
   - Move record encoding/decoding helpers (currently in `vdbeInt.h`/`vdbe.c`) into a new module (`record_codec.c/h`).
   - Implement schema loader that reads `sqlite_master` b-tree (root page 1) and exposes table/index metadata structs.
3. **API Surface Draft**
   - Define public headers for:
     - `storage_open/close` (database lifecycle, VFS selection).
     - `storage_txn_begin/commit/rollback` (maps to pager transactions/WAL frames).
     - `storage_table_open` & cursor navigation (`first`, `next`, `seek`, `insert`, `delete`, `update`).
     - `storage_index_open` with key encoding utilities.
   - Ensure API functions are plain C, return SQLite-style `int` codes, and accept allocator callbacks to ease embedding.
4. **Decouple from VDBE**
   - Replace VDBE opcode helpers used in b-tree record handling with calls into the new record module.
   - Remove VDBE dependencies in `btree.c` (e.g., `KeyInfo`, `UnpackedRecord` allocation paths) by providing equivalents in the new API.
5. **Build & Config Simplification**
   - Add minimal build target (e.g., `make storage_lib`) that compiles only pager/btree/vfs + new API.
   - Use feature flags to omit SQL parser/optimizer (`SQLITE_OMIT_*`), FTS, RTREE, etc., ensuring amalgamation still works.
6. **Testing Strategy**
   - Create C test harness that exercises the new API: open DB, create schema entries, insert/scan rows, verify transactions and WAL replay.
   - Reuse existing `test/` infrastructure where possible, but target the new API directly instead of SQL.

## Open Questions & Risks
- **Schema evolution**: without SQL DDL, how to express table/index creation? Proposed approach is to provide helper routines that build `sqlite_master` entries and root pages programmatically.
- **Backwards compatibility**: aim to keep file format intact so databases remain readable by full SQLite; verify via cross-open tests.
- **Locking semantics**: ensure new API correctly drives pager state transitions (`SHARED`, `RESERVED`, `PENDING`, `EXCLUSIVE`).
- **Memory ownership**: define clear allocation/free rules for records and schema structs to avoid relying on `sqlite3_value`/`Mem` types.

## Next Steps
- Prototype `record_codec.c/h` and `schema_service.c/h` that compile alongside current source without VDBE includes.
- Draft `include/storage.h` with public API prototypes and minimal docstrings.
- Add a small sample program under `ext/storage_api/examples/kv_demo.c` that opens a DB, writes/reads a key/value table using the new API.
