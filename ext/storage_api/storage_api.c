#include "include/storage.h"
#include "storage_internal.h"

#include "vdbeInt.h"

#include <stdlib.h>
#include <string.h>

static char *storage_strdup(sqlite3 *db, const char *src) {
  if (!src) return NULL;
  (void)db;
  return sqlite3_mprintf("%s", src);
}

static int validate_db(StorageDb *db) {
  return db && db->db && db->pBt ? SQLITE_OK : SQLITE_MISUSE;
}

int storage_open(const char *filename, const StorageOpenConfig *config, StorageDb **out_db) {
  if (!filename || !out_db) {
    return SQLITE_MISUSE;
  }
  StorageDb *wrapper = (StorageDb *)sqlite3_malloc(sizeof(StorageDb));
  if (!wrapper) {
    return SQLITE_NOMEM;
  }
  memset(wrapper, 0, sizeof(*wrapper));

  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI;
  const char *vfs = NULL;
  if (config) {
    if (config->flags) flags = config->flags;
    vfs = config->vfs_name;
  }
  int rc = sqlite3_open_v2(filename, &wrapper->db, flags, vfs);
  if (rc != SQLITE_OK) {
    sqlite3_free(wrapper);
    return rc;
  }

  wrapper->pBt = wrapper->db->aDb[0].pBt;
  if (!wrapper->pBt) {
    sqlite3_close(wrapper->db);
    sqlite3_free(wrapper);
    return SQLITE_CANTOPEN;
  }

  if (config) {
    if (config->page_size > 0) {
      sqlite3BtreeSetPageSize(wrapper->pBt, config->page_size, 0, 0);
    }
    if (config->cache_size > 0) {
      sqlite3BtreeSetCacheSize(wrapper->pBt, config->cache_size);
    }
  }

  *out_db = wrapper;
  return SQLITE_OK;
}

int storage_close(StorageDb *db) {
  if (!db) return SQLITE_OK;
  sqlite3 *sdb = db->db;
  sqlite3_free(db);
  return sqlite3_close(sdb);
}

int storage_txn_begin(StorageDb *db, int flags, StorageTxn **out_txn) {
  if (validate_db(db) != SQLITE_OK || !out_txn) {
    return SQLITE_MISUSE;
  }
  int wr = (flags & STORAGE_TXN_WRITE) ? 1 : 0;
  int rc;
  sqlite3_mutex_enter(sqlite3_db_mutex(db->db));
  sqlite3BtreeEnter(db->pBt);
  rc = sqlite3BtreeBeginTrans(db->pBt, wr, 0);
  sqlite3BtreeLeave(db->pBt);
  sqlite3_mutex_leave(sqlite3_db_mutex(db->db));
  if (rc != SQLITE_OK) return rc;

  StorageTxn *txn = (StorageTxn *)sqlite3_malloc(sizeof(StorageTxn));
  if (!txn) return SQLITE_NOMEM;
  memset(txn, 0, sizeof(*txn));
  txn->db = db;
  txn->flags = flags;
  txn->active = 1;
  *out_txn = txn;
  return SQLITE_OK;
}

int storage_txn_commit(StorageTxn *txn) {
  if (!txn || !txn->active) {
    return SQLITE_MISUSE;
  }
  int rc;
  sqlite3_mutex_enter(sqlite3_db_mutex(txn->db->db));
  sqlite3BtreeEnter(txn->db->pBt);
  rc = sqlite3BtreeCommit(txn->db->pBt);
  sqlite3BtreeLeave(txn->db->pBt);
  sqlite3_mutex_leave(sqlite3_db_mutex(txn->db->db));
  sqlite3_free(txn);
  return rc;
}

int storage_txn_rollback(StorageTxn *txn) {
  if (!txn || !txn->active) {
    return SQLITE_MISUSE;
  }
  int rc;
  sqlite3_mutex_enter(sqlite3_db_mutex(txn->db->db));
  sqlite3BtreeEnter(txn->db->pBt);
  rc = sqlite3BtreeRollback(txn->db->pBt, SQLITE_OK, 0);
  sqlite3BtreeLeave(txn->db->pBt);
  sqlite3_mutex_leave(sqlite3_db_mutex(txn->db->db));
  sqlite3_free(txn);
  return rc;
}

int storage_table_open(StorageDb *db, const char *name, StorageTable **out_table) {
  if (validate_db(db) != SQLITE_OK || !name || !out_table) return SQLITE_MISUSE;
  StorageSchema *schema = NULL;
  int rc = schema_service_load(db, &schema);
  if (rc != SQLITE_OK) return rc;
  StorageSchemaEntry entry;
  rc = schema_service_find(schema, name, &entry);
  if (rc != SQLITE_OK) {
    schema_service_free(schema);
    return rc;
  }
  StorageTable *t = sqlite3_malloc(sizeof(StorageTable));
  if (!t) {
    schema_service_free(schema);
    return SQLITE_NOMEM;
  }
  memset(t, 0, sizeof(*t));
  t->db = db;
  t->root_page = entry.root_page;
  t->name = storage_strdup(db->db, name);
  schema_service_free(schema);
  *out_table = t;
  return SQLITE_OK;
}

int storage_index_open(StorageDb *db, const char *name, StorageIndex **out_index) {
  if (validate_db(db) != SQLITE_OK || !name || !out_index) return SQLITE_MISUSE;
  StorageSchema *schema = NULL;
  int rc = schema_service_load(db, &schema);
  if (rc != SQLITE_OK) return rc;
  StorageSchemaEntry entry;
  rc = schema_service_find(schema, name, &entry);
  if (rc != SQLITE_OK) {
    schema_service_free(schema);
    return rc;
  }
  StorageIndex *idx = sqlite3_malloc(sizeof(StorageIndex));
  if (!idx) {
    schema_service_free(schema);
    return SQLITE_NOMEM;
  }
  memset(idx, 0, sizeof(*idx));
  idx->db = db;
  idx->root_page = entry.root_page;
  idx->name = storage_strdup(db->db, name);
  schema_service_free(schema);
  *out_index = idx;
  return SQLITE_OK;
}

int storage_table_close(StorageTable *table) {
  if (!table) return SQLITE_OK;
  sqlite3_free(table->name);
  sqlite3_free(table);
  return SQLITE_OK;
}

int storage_index_close(StorageIndex *index) {
  if (!index) return SQLITE_OK;
  sqlite3_free(index->name);
  sqlite3_free(index);
  return SQLITE_OK;
}

static int open_cursor(StorageDb *db, int root_page, int write, BtCursor **out) {
  int rc;
  BtCursor *cur = (BtCursor *)sqlite3_malloc64(sqlite3BtreeCursorSize());
  if (!cur) return SQLITE_NOMEM;
  memset(cur, 0, sqlite3BtreeCursorSize());
  sqlite3_mutex_enter(sqlite3_db_mutex(db->db));
  sqlite3BtreeEnter(db->pBt);
  rc = sqlite3BtreeCursor(db->pBt, (Pgno)root_page, write, NULL, cur);
  if (rc == SQLITE_OK && write) {
    sqlite3BtreeCursorHintFlags(cur, BTREE_BULKLOAD);
  }
  sqlite3BtreeLeave(db->pBt);
  sqlite3_mutex_leave(sqlite3_db_mutex(db->db));
  if (rc != SQLITE_OK) {
    sqlite3_free(cur);
    return rc;
  }
  *out = cur;
  return SQLITE_OK;
}

int storage_cursor_open_table(StorageTable *table, StorageCursor **out_cursor) {
  if (!table || !out_cursor) return SQLITE_MISUSE;
  StorageCursor *c = sqlite3_malloc(sizeof(StorageCursor));
  if (!c) return SQLITE_NOMEM;
  memset(c, 0, sizeof(*c));
  int rc = open_cursor(table->db, table->root_page, 1, &c->cursor);
  if (rc != SQLITE_OK) {
    sqlite3_free(c);
    return rc;
  }
  c->db = table->db;
  c->is_index = 0;
  c->owned = 1;
  *out_cursor = c;
  return SQLITE_OK;
}

int storage_cursor_open_index(StorageIndex *index, StorageCursor **out_cursor) {
  if (!index || !out_cursor) return SQLITE_MISUSE;
  StorageCursor *c = sqlite3_malloc(sizeof(StorageCursor));
  if (!c) return SQLITE_NOMEM;
  memset(c, 0, sizeof(*c));
  int rc = open_cursor(index->db, index->root_page, 1, &c->cursor);
  if (rc != SQLITE_OK) {
    sqlite3_free(c);
    return rc;
  }
  c->db = index->db;
  c->is_index = 1;
  c->owned = 1;
  *out_cursor = c;
  return SQLITE_OK;
}

int storage_cursor_close(StorageCursor *cursor) {
  if (!cursor) return SQLITE_OK;
  if (cursor->owned && cursor->cursor) {
    sqlite3_mutex_enter(sqlite3_db_mutex(cursor->db->db));
    sqlite3BtreeCloseCursor(cursor->cursor);
    sqlite3_mutex_leave(sqlite3_db_mutex(cursor->db->db));
    sqlite3_free(cursor->cursor);
  }
  sqlite3_free(cursor);
  return SQLITE_OK;
}

int storage_cursor_first(StorageCursor *cursor) {
  if (!cursor || !cursor->cursor) return SQLITE_MISUSE;
  int rc;
  sqlite3_mutex_enter(sqlite3_db_mutex(cursor->db->db));
  sqlite3BtreeEnter(cursor->cursor->pBtree);
  rc = sqlite3BtreeFirst(cursor->cursor, 0);
  sqlite3BtreeLeave(cursor->cursor->pBtree);
  sqlite3_mutex_leave(sqlite3_db_mutex(cursor->db->db));
  return rc;
}

int storage_cursor_next(StorageCursor *cursor) {
  if (!cursor || !cursor->cursor) return SQLITE_MISUSE;
  int rc;
  sqlite3_mutex_enter(sqlite3_db_mutex(cursor->db->db));
  sqlite3BtreeEnter(cursor->cursor->pBtree);
  rc = sqlite3BtreeNext(cursor->cursor, 0);
  sqlite3BtreeLeave(cursor->cursor->pBtree);
  sqlite3_mutex_leave(sqlite3_db_mutex(cursor->db->db));
  if (rc == SQLITE_DONE) rc = SQLITE_NOTFOUND;
  return rc;
}

int storage_cursor_seek(StorageCursor *cursor, const StorageRecord *key, int *exact_out) {
  if (!cursor || !cursor->cursor || !key || !key->has_key) return SQLITE_MISUSE;
  int rc;
  int res = 0;
  sqlite3_mutex_enter(sqlite3_db_mutex(cursor->db->db));
  sqlite3BtreeEnter(cursor->cursor->pBtree);
  if (cursor->is_index) {
    UnpackedRecord r;
    Mem mem;
    memset(&r, 0, sizeof(r));
    memset(&mem, 0, sizeof(mem));
    r.nField = 1;
    r.default_rc = 0;
    r.pKeyInfo = cursor->cursor->pKeyInfo;
    r.aMem = &mem;
    mem.flags = MEM_Int;
    mem.u.i = key->key;
    mem.enc = SQLITE_UTF8;
    mem.db = cursor->cursor->pBtree->db;
    rc = sqlite3BtreeIndexMoveto(cursor->cursor, &r, &res);
  } else {
    rc = sqlite3BtreeTableMoveto(cursor->cursor, key->key, 0, &res);
  }
  sqlite3BtreeLeave(cursor->cursor->pBtree);
  sqlite3_mutex_leave(sqlite3_db_mutex(cursor->db->db));
  if (exact_out) *exact_out = (rc == SQLITE_OK && res == 0);
  return rc == SQLITE_OK && res != 0 ? SQLITE_NOTFOUND : rc;
}

static int read_payload(BtCursor *cur, StorageRecord *out_record) {
  u32 sz = sqlite3BtreePayloadSize(cur);
  uint8_t *buf = sqlite3_malloc(sz);
  if (!buf) return SQLITE_NOMEM;
  int rc = sqlite3BtreePayload(cur, 0, sz, buf);
  if (rc != SQLITE_OK) {
    sqlite3_free(buf);
    return rc;
  }
  out_record->data = buf;
  out_record->data_len = (int)sz;
  out_record->has_key = 1;
  out_record->key = sqlite3BtreeIntegerKey(cur);
  return SQLITE_OK;
}

int storage_cursor_read(StorageCursor *cursor, StorageRecord *out_record) {
  if (!cursor || !cursor->cursor || !out_record) return SQLITE_MISUSE;
  int rc;
  sqlite3_mutex_enter(sqlite3_db_mutex(cursor->db->db));
  sqlite3BtreeEnter(cursor->cursor->pBtree);
  rc = sqlite3BtreeEof(cursor->cursor) ? SQLITE_NOTFOUND : SQLITE_OK;
  if (rc == SQLITE_OK) {
    rc = read_payload(cursor->cursor, out_record);
  }
  sqlite3BtreeLeave(cursor->cursor->pBtree);
  sqlite3_mutex_leave(sqlite3_db_mutex(cursor->db->db));
  return rc;
}

int storage_cursor_insert(StorageCursor *cursor, const StorageRecord *record) {
  if (!cursor || !cursor->cursor || !record || !record->has_key) return SQLITE_MISUSE;
  BtreePayload payload;
  memset(&payload, 0, sizeof(payload));
  payload.nKey = record->key;
  payload.pData = record->data;
  payload.nData = record->data_len;
  int rc;
  sqlite3_mutex_enter(sqlite3_db_mutex(cursor->db->db));
  sqlite3BtreeEnter(cursor->cursor->pBtree);
  rc = sqlite3BtreeInsert(cursor->cursor, &payload, 0, 0);
  sqlite3BtreeLeave(cursor->cursor->pBtree);
  sqlite3_mutex_leave(sqlite3_db_mutex(cursor->db->db));
  return rc;
}

int storage_cursor_update(StorageCursor *cursor, const StorageRecord *record) {
  if (!cursor || !cursor->cursor || !record || !record->has_key) return SQLITE_MISUSE;
  /* Use insert with overwrite semantics: move to key then delete+insert. */
  int rc = storage_cursor_seek(cursor, record, NULL);
  if (rc != SQLITE_OK) return rc;
  rc = storage_cursor_delete(cursor);
  if (rc != SQLITE_OK) return rc;
  return storage_cursor_insert(cursor, record);
}

int storage_cursor_delete(StorageCursor *cursor) {
  if (!cursor || !cursor->cursor) return SQLITE_MISUSE;
  int rc;
  sqlite3_mutex_enter(sqlite3_db_mutex(cursor->db->db));
  sqlite3BtreeEnter(cursor->cursor->pBtree);
  rc = sqlite3BtreeDelete(cursor->cursor, 0);
  sqlite3BtreeLeave(cursor->cursor->pBtree);
  sqlite3_mutex_leave(sqlite3_db_mutex(cursor->db->db));
  return rc;
}
