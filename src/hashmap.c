// began at 17:25

#include "hashmap.h"
#include <stdio.h>   // for anomaly logging
#include <stdlib.h>  // for malloc, calloc
#include <assert.h>

struct HashMapEntry {
  HashMapHashValue hashValue;
  const void *key;
  void *value;
};

struct HashMap {
  struct HashMapEntry *items;
  size_t capacity;
  size_t size;
  HashMapComparator cmp;
  HashMapHasher hash;
};

struct HashMapIterator {
  const struct HashMap *map;
  size_t index;
};

#define HASHMAP_INITIAL_CAPACITY 16

static const char HASHMAP_DELETED_ITEM[] = {0};

HashMap *HashMap_new(HashMapComparator cmp, HashMapHasher hash) {
  if (!cmp || !hash) return 0;
  HashMap *self = malloc(sizeof(HashMap));
  if (!self) return 0;
  self->capacity = HASHMAP_INITIAL_CAPACITY;
  self->size = 0;
  self->cmp = cmp;
  self->hash = hash;
  self->items = calloc(self->capacity, sizeof(HashMapEntry));
  if (!self->items) {
    free(self);
    return 0;
  }
  return self;
}

void HashMap_delete(HashMap *self) {
  if (self) free(self->items);
  free(self);
}

void HashMap_clear(HashMap *self) {
  self->size = 0;
  for (size_t i = 0; i < self->capacity; ++i) {
    self->items[i].key = 0;
    self->items[i].value = 0;
  }
}

size_t HashMap_size(const HashMap *self) {
  return self->size;
}

/**
 * Probes through an array, starting at a particular hash value,
 * for a key or NULL or HASHMAP_DELETED_ITEM.
 * @param hashValue must equal self->hash(key)
 * @param delIndex where to write the index of an encountered
 *   HASHMAP_DELETED_ITEM (usually 0 for get or a pointer for put)
 * @return if present: index into self->items of this key;
 *   if absent: index where it could be inserted;
 *   if >= self->capacity: there's no space
 */
static size_t probe(const HashMap *self, const void *key,
                    HashMapHashValue hashValue, size_t *delIndex) {
  // Quadratic probing
  size_t num_skipped = 0;
  size_t index = (hashValue ^ (hashValue >> 23));
  while (num_skipped < self->capacity) {
    index &= self->capacity - 1;
    const void *keyHere = self->items[index].key;
    if (!keyHere) return index;  // Empty
    if (delIndex && key == HASHMAP_DELETED_ITEM) *delIndex = index;
    if (self->items[index].hashValue == hashValue
        && self->cmp(key, keyHere) == 0) {
      return index;
    }
    index += num_skipped * 2 + 1;
    num_skipped += 1;
  }
  return self->capacity;
}

int HashMap_containsKey(const HashMap *self, const void *key) {
  HashMapHashValue hashValue = self->hash(key);
  size_t index = probe(self, key, hashValue, 0);
  return index < self->capacity && self->items[index].key != 0;
}

void *HashMap_getOrDefault(const HashMap *self, const void *key,
                           void *defaultValue) {
  HashMapHashValue hashValue = self->hash(key);
  size_t index = probe(self, key, hashValue, 0);
  if (index >= self->capacity) return defaultValue;
  const void *keyHere = self->items[index].key;
  return keyHere ? self->items[index].value : defaultValue;
}

void *HashMap_get(const HashMap *self, const void *key) {
  return HashMap_getOrDefault(self, key, 0);
}

static size_t expand(HashMap *self) {
  // Allocate new pool of items
  if (self->capacity >= SIZE_MAX / 2) return 0;
  HashMap tempMap;
  tempMap.capacity = self->capacity * 2;
  tempMap.size = 0;
  tempMap.cmp = self->cmp;
  tempMap.hash = self->hash;
  tempMap.items = calloc(tempMap.capacity, sizeof(HashMapEntry));
  if (!tempMap.items) return 0;
  
  // Copy all items that aren't NULL or tombstone
  for (size_t i = 0; i < self->capacity; ++i) {
    const void *key = self->items[i].key;
    if (key == 0 || key == HASHMAP_DELETED_ITEM) continue;
    size_t tempIndex = probe(&tempMap, key, self->items[i].hashValue,
                             0);
    if (tempIndex >= tempMap.capacity) {
      fprintf(stderr, "anomaly while resizing map from capacity %zu to %zu: probe could not find space for item at %zu\n",
              self->capacity, tempMap.capacity, i);
      free(tempMap.items);
      return 0;
    }
    tempMap.items[tempIndex] = self->items[i];
  }

  if (self->size != tempMap.size) {
    fprintf(stderr, "anomaly while resizing map from capacity %zu to %zu: size from %zu to %zu\n",
            self->capacity, tempMap.capacity, self->size, tempMap.size);
    free(tempMap.items);
    return 0;
  }

  // Items are copied; switch to the new backing storage
  free(self->items);
  self->items = tempMap.items;
  self->capacity = tempMap.capacity;
  return tempMap.capacity;
}

void *put_impl(HashMap *self, const void *key, void *value,
               int replaceNull, int replaceNonnull) {
  assert(self->capacity >= self->size);
  if ((self->capacity - self->size) > (self->capacity / 4)) {
    fprintf(stderr, "expanding map at %zu of %zu filled\n",
            self->size, self->capacity);
    if (!expand(self)) {
      fprintf(stderr, "failed to expand map at %zu of %zu filled\n",
              self->size, self->capacity);
      return 0;
    }
  }

  // Find the item
  HashMapHashValue hashValue = self->hash(key);
  size_t delIndex = self->capacity;
  size_t index = probe(self, key, hashValue, &delIndex);
  // It's possible for index to be invalid (>= self->capacity) and
  // delIndex to be valid (< self->capacity) if the table is getting
  // full of deleted item markers.  Usually this means an expand()
  // is warranted soon.
  
  // Add or replace the item.
  const void *keyHere = index < self->capacity
                        ? self->items[index].key : 0;
  if (keyHere != 0) {
    // Key exists; replace its value if requested
    void *previousValue = self->items[index].value;
    if (previousValue && !replaceNonnull) return previousValue;
    if (!previousValue && !replaceNull) return previousValue;
    self->items[index].value = value;
    return previousValue;
  } else {
    // Adding item that doesn't currently exist.
    // Try to replace deleted item markers if possible
    if (delIndex < self->capacity) index = delIndex;
    if (index >= self->capacity) {
      fprintf(stderr, "anomaly when probing to add item at %zu of %zu filled; delIndex was %zu\n",
              self->size, self->capacity, delIndex);
      return 0;
    }

    self->items[index].hashValue = hashValue;
    self->items[index].key = key;
    self->items[index].value = value;
    self->size += 1;
    return 0;
  }
}

void *HashMap_put(HashMap *self, const void *key, void *value) {
  return put_impl(self, key, value, 1, 1);
}

void *HashMap_setdefault(HashMap *self, const void *key,
                         void *value) {
  return put_impl(self, key, value, 0, 0);
}

void *HashMap_putIfAbsent(HashMap *self, const void *key,
                          void *value) {
  return put_impl(self, key, value, 1, 0);
}

void *HashMap_remove(HashMap *self, const void *key) {
  // Find the item
  HashMapHashValue hashValue = self->hash(key);
  size_t index = probe(self, key, hashValue, 0);
  if (index >= self->capacity) return 0;
  const void *keyHere = self->items[index].key;
  if (keyHere == 0) return 0;

  // Remove the item
  if (self->size > 0) {
    self->size -= 1;
  } else {
    fprintf(stderr, "anomaly when probing to add item at %zu of %zu filled\n",
            self->size, self->capacity);
  }
  void *previousValue = self->items[index].value;
  self->items[index].hashValue = index * 33;
  self->items[index].key = HASHMAP_DELETED_ITEM;
  self->items[index].value = 0;
  return previousValue;
}
