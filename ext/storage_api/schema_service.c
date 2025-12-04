#include "include/storage.h"
#include "storage_internal.h"

#include "vdbeInt.h"

#include <string.h>

struct StorageSchema {
  int entry_count;
  StorageSchemaEntry *entries;
};

static int schema_service_append(StorageSchema *schema, const StorageSchemaEntry *entry) {
  if (!schema || !entry) return SQLITE_MISUSE;
  StorageSchemaEntry *new_entries = (StorageSchemaEntry *)sqlite3_realloc(
      schema->entries, (size_t)(schema->entry_count + 1) * sizeof(StorageSchemaEntry));
  if (!new_entries) return SQLITE_NOMEM;
  schema->entries = new_entries;
  schema->entries[schema->entry_count] = *entry;
  schema->entry_count += 1;
  return SQLITE_OK;
}

int schema_service_load(StorageDb *db, StorageSchema **out_schema) {
  if (!db || !out_schema) return SQLITE_MISUSE;
  StorageSchema *schema = (StorageSchema *)sqlite3_malloc(sizeof(StorageSchema));
  if (!schema) return SQLITE_NOMEM;
  memset(schema, 0, sizeof(*schema));

  int rc = SQLITE_OK;
  sqlite3 *sdb = db->db;
  sqlite3_mutex_enter(sqlite3_db_mutex(sdb));

  rc = sqlite3Init(sdb, NULL);
  if (rc == SQLITE_OK) {
    Schema *s = sdb->aDb[0].pSchema;
    for (HashElem *e = sqliteHashFirst(&s->tblHash); e && rc == SQLITE_OK;
         e = sqliteHashNext(e)) {
      Table *t = (Table *)sqliteHashData(e);
      StorageSchemaEntry entry = {0};
      entry.name = sqlite3_mprintf("%s", t->zName);
      entry.type = sqlite3_mprintf("table");
      entry.root_page = t->tnum;
      entry.is_index = 0;
      if (!entry.name || !entry.type) {
        rc = SQLITE_NOMEM;
        sqlite3_free((void *)entry.name);
        sqlite3_free((void *)entry.type);
        break;
      }
      rc = schema_service_append(schema, &entry);
      if (rc != SQLITE_OK) break;
      for (Index *idx = t->pIndex; idx && rc == SQLITE_OK; idx = idx->pNext) {
        StorageSchemaEntry ientry = {0};
        ientry.name = sqlite3_mprintf("%s", idx->zName);
        ientry.type = sqlite3_mprintf("index");
        ientry.root_page = idx->tnum;
        ientry.is_index = 1;
        if (!ientry.name || !ientry.type) {
          rc = SQLITE_NOMEM;
          sqlite3_free((void *)ientry.name);
          sqlite3_free((void *)ientry.type);
          break;
        }
        rc = schema_service_append(schema, &ientry);
      }
    }
  }

  sqlite3_mutex_leave(sqlite3_db_mutex(sdb));

  if (rc != SQLITE_OK) {
    schema_service_free(schema);
    return rc;
  }

  *out_schema = schema;
  return SQLITE_OK;
}

int schema_service_find(const StorageSchema *schema, const char *name, StorageSchemaEntry *out_entry) {
  if (!schema || !name || !out_entry) return SQLITE_MISUSE;
  for (int i = 0; i < schema->entry_count; ++i) {
    const StorageSchemaEntry *entry = &schema->entries[i];
    if (entry->name && sqlite3StrICmp(entry->name, name) == 0) {
      *out_entry = *entry;
      return SQLITE_OK;
    }
  }
  return SQLITE_NOTFOUND;
}

int schema_service_free(StorageSchema *schema) {
  if (!schema) return SQLITE_OK;
  for (int i = 0; i < schema->entry_count; ++i) {
    sqlite3_free((void *)schema->entries[i].name);
    sqlite3_free((void *)schema->entries[i].type);
  }
  sqlite3_free(schema->entries);
  sqlite3_free(schema);
  return SQLITE_OK;
}

int storage_schema_load(StorageDb *db, StorageSchema **out_schema) {
  return schema_service_load(db, out_schema);
}

int storage_schema_free(StorageSchema *schema) {
  return schema_service_free(schema);
}
