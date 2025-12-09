#ifndef STORAGE_INTERNAL_H
#define STORAGE_INTERNAL_H

#include "include/storage.h"
#include "sqliteInt.h"
#include "btreeInt.h"

struct StorageDb {
  sqlite3 *db;
  Btree *pBt;
};

struct StorageTxn {
  StorageDb *db;
  int flags;
  int active;
};

struct StorageTable {
  StorageDb *db;
  int root_page;
  char *name;
};

struct StorageIndex {
  StorageDb *db;
  int root_page;
  char *name;
};

struct StorageCursor {
  StorageDb *db;
  BtCursor *cursor;
  int is_index;
  int owned;
};

#endif /* STORAGE_INTERNAL_H */
