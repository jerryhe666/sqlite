#ifndef STORAGE_API_H
#define STORAGE_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Provide minimal SQLite-style status codes when sqlite3.h is not included. */
#ifndef SQLITE_OK
#define SQLITE_OK 0
#endif
#ifndef SQLITE_ERROR
#define SQLITE_ERROR 1
#endif
#ifndef SQLITE_NOMEM
#define SQLITE_NOMEM 7
#endif
#ifndef SQLITE_READONLY
#define SQLITE_READONLY 8
#endif
#ifndef SQLITE_INTERRUPT
#define SQLITE_INTERRUPT 9
#endif
#ifndef SQLITE_IOERR
#define SQLITE_IOERR 10
#endif
#ifndef SQLITE_CORRUPT
#define SQLITE_CORRUPT 11
#endif
#ifndef SQLITE_NOTFOUND
#define SQLITE_NOTFOUND 12
#endif
#ifndef SQLITE_FULL
#define SQLITE_FULL 13
#endif
#ifndef SQLITE_CANTOPEN
#define SQLITE_CANTOPEN 14
#endif
#ifndef SQLITE_PROTOCOL
#define SQLITE_PROTOCOL 15
#endif
#ifndef SQLITE_EMPTY
#define SQLITE_EMPTY 16
#endif
#ifndef SQLITE_SCHEMA
#define SQLITE_SCHEMA 17
#endif
#ifndef SQLITE_TOOBIG
#define SQLITE_TOOBIG 18
#endif
#ifndef SQLITE_CONSTRAINT
#define SQLITE_CONSTRAINT 19
#endif
#ifndef SQLITE_MISMATCH
#define SQLITE_MISMATCH 20
#endif
#ifndef SQLITE_MISUSE
#define SQLITE_MISUSE 21
#endif

/* Opaque handles. */
typedef struct StorageDb StorageDb;
typedef struct StorageTxn StorageTxn;
typedef struct StorageSchema StorageSchema;
typedef struct StorageTable StorageTable;
typedef struct StorageIndex StorageIndex;
typedef struct StorageCursor StorageCursor;

/* Simple record representation; will map to SQLite's record format later. */
typedef struct StorageRecord {
  /* Optional integer key (rowid) for BTREE_INTKEY tables. */
  int has_key;
  int64_t key;
  /* Raw payload bytes stored alongside the key. */
  uint8_t *data;
  int data_len;
} StorageRecord;

/* Schema entry metadata mirroring sqlite_master. */
typedef struct StorageSchemaEntry {
  const char *name;
  const char *type; /* "table" or "index" */
  int root_page;
  int is_index;
} StorageSchemaEntry;

/* Database open configuration. */
typedef struct StorageOpenConfig {
  const char *vfs_name; /* NULL for default */
  int flags;            /* SQLITE_OPEN_* compatible */
  int page_size;        /* Optional override; 0 keeps default */
  int cache_size;       /* Optional override; 0 keeps default */
} StorageOpenConfig;

/* Transaction flags. */
#define STORAGE_TXN_DEFAULT 0
#define STORAGE_TXN_WRITE 0x01

/* Database lifecycle. */
int storage_open(const char *filename, const StorageOpenConfig *config, StorageDb **out_db);
int storage_close(StorageDb *db);

/* Transactions. */
int storage_txn_begin(StorageDb *db, int flags, StorageTxn **out_txn);
int storage_txn_commit(StorageTxn *txn);
int storage_txn_rollback(StorageTxn *txn);

/* Schema management. */
int storage_schema_load(StorageDb *db, StorageSchema **out_schema);
int storage_schema_free(StorageSchema *schema);

/* Table/index access. */
int storage_table_open(StorageDb *db, const char *name, StorageTable **out_table);
int storage_index_open(StorageDb *db, const char *name, StorageIndex **out_index);
int storage_table_close(StorageTable *table);
int storage_index_close(StorageIndex *index);

/* Cursor lifecycle. */
int storage_cursor_open_table(StorageTable *table, StorageCursor **out_cursor);
int storage_cursor_open_index(StorageIndex *index, StorageCursor **out_cursor);
int storage_cursor_close(StorageCursor *cursor);

/* Cursor navigation. */
int storage_cursor_first(StorageCursor *cursor);
int storage_cursor_next(StorageCursor *cursor);
int storage_cursor_seek(StorageCursor *cursor, const StorageRecord *key, int *exact_out);

/* Cursor payload operations. */
int storage_cursor_read(StorageCursor *cursor, StorageRecord *out_record);
int storage_cursor_insert(StorageCursor *cursor, const StorageRecord *record);
int storage_cursor_update(StorageCursor *cursor, const StorageRecord *record);
int storage_cursor_delete(StorageCursor *cursor);

/* Record encoding helpers. */
int record_codec_encode(const StorageRecord *record, uint8_t **out_bytes, int *out_len);
int record_codec_decode(const uint8_t *bytes, int len, StorageRecord *out_record);

/* Schema helpers. */
int schema_service_load(StorageDb *db, StorageSchema **out_schema);
int schema_service_find(const StorageSchema *schema, const char *name, StorageSchemaEntry *out_entry);
int schema_service_free(StorageSchema *schema);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STORAGE_API_H */
