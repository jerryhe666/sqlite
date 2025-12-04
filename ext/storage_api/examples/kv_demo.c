#include "../include/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump_status(const char *label, int rc) {
  printf("%s => %d\n", label, rc);
}

int main(void) {
  StorageDb *db = NULL;
  StorageTxn *txn = NULL;
  StorageTable *table = NULL;
  StorageCursor *cursor = NULL;

  StorageOpenConfig cfg = {0};
  int rc = storage_open("test.db", &cfg, &db);
  dump_status("storage_open", rc);
  if (rc != SQLITE_OK) {
    return EXIT_FAILURE;
  }

  rc = storage_txn_begin(db, STORAGE_TXN_WRITE, &txn);
  dump_status("storage_txn_begin", rc);
  if (rc != SQLITE_OK) goto done;

  rc = storage_table_open(db, "kv", &table);
  dump_status("storage_table_open", rc);
  if (rc != SQLITE_OK) goto done;

  rc = storage_cursor_open_table(table, &cursor);
  dump_status("storage_cursor_open_table", rc);
  if (rc != SQLITE_OK) goto done;

  StorageRecord key = { .has_key = 1, .key = 1, .data = (uint8_t *)"demo", .data_len = 4 };
  int exact = 0;
  rc = storage_cursor_seek(cursor, &key, &exact);
  dump_status("storage_cursor_seek", rc);

  StorageRecord value = { .has_key = 1, .key = 1, .data = (uint8_t *)"value", .data_len = 5 };
  rc = storage_cursor_insert(cursor, &value);
  dump_status("storage_cursor_insert", rc);
  if (rc != SQLITE_OK) goto done;

  storage_cursor_close(cursor);
  cursor = NULL;

  rc = storage_txn_commit(txn);
  dump_status("storage_txn_commit", rc);

done:
  if (txn && rc != SQLITE_OK) {
    storage_txn_rollback(txn);
  }
  storage_cursor_close(cursor);
  storage_table_close(table);
  storage_close(db);
  return rc == SQLITE_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
