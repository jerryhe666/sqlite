#include "include/storage.h"

#include <stdlib.h>
#include <string.h>

int record_codec_encode(const StorageRecord *record, uint8_t **out_bytes, int *out_len) {
  if (!record || !out_bytes || !out_len) {
    return SQLITE_MISUSE;
  }
  if (record->data_len < 0) {
    return SQLITE_MISMATCH;
  }
  uint8_t *buffer = NULL;
  if (record->data_len > 0) {
    buffer = (uint8_t *)malloc((size_t)record->data_len);
    if (!buffer) {
      return SQLITE_NOMEM;
    }
    memcpy(buffer, record->data, (size_t)record->data_len);
  }
  *out_bytes = buffer;
  *out_len = record->data_len;
  return SQLITE_OK;
}

int record_codec_decode(const uint8_t *bytes, int len, StorageRecord *out_record) {
  if (!out_record) {
    return SQLITE_MISUSE;
  }
  if (len < 0) {
    return SQLITE_MISMATCH;
  }
  uint8_t *buffer = NULL;
  if (bytes && len > 0) {
    buffer = (uint8_t *)malloc((size_t)len);
    if (!buffer) {
      return SQLITE_NOMEM;
    }
    memcpy(buffer, bytes, (size_t)len);
  }
  out_record->data = buffer;
  out_record->data_len = len;
  return SQLITE_OK;
}
