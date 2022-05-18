#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>  // for uint_fast32_t
#include <stdlib.h>  // for size_t

typedef uint_fast32_t HashMapHashValue;
typedef struct HashMapEntry HashMapEntry;
typedef struct HashMapIterator HashMapIterator;
typedef struct HashMap HashMap;

/**
 * Returns 0 if two keys are equal or nonzero if not.
 */
typedef int (*HashMapComparator)(const void *, const void *);

/**
 * Returns a hash code associated with the object.
 * If two keys are equal, they MUST have the same hash value.
 */
typedef HashMapHashValue (*HashMapHasher)(const void *);

/**
 * Creates a new dictionary.
 * @param cmp a 2-argument function that returns 0 if and only if
 * two keys are equal (similar to bsearch() or qsort() comparator)
 * @param hash a 1-argument function returning a value that is the
 * same for keys where cmp returns 0 and different for most keys
 * where cmp returns nonzero
 * @return pointer to the new object or NULL if an error occurred
 */
HashMap *HashMap_new(HashMapComparator cmp, HashMapHasher hash);

/**
 * Destroys a dictionary.
 */
void HashMap_delete(HashMap *self);

/**
 * Destroys all associations in a dictionary, setting size to 0.
 */
void HashMap_clear(HashMap *self);

/**
 * Returns how many different keys are associated with a value.
 */
size_t HashMap_size(const HashMap *self);

/**
 * Returns nonzero if the given key is associated with a value.
 */
int HashMap_containsKey(const HashMap *self, const void *key);

/**
 * Returns the value associated with the given key, or defaultValue
 * if none.
 */
void *HashMap_getOrDefault(const HashMap *self, const void *key,
                           void *defaultValue);

/**
 * Returns the value mapped to the given key, or NULL if none.
 * Equivalent to HashMap_getOrDefault(self, key, NULL).
 */
void *HashMap_get(const HashMap *self, const void *key);

/**
 * Maps key to value, replacing any existing mapping.
 * @return value
 */
void *HashMap_put(HashMap *self, const void *key, void *value);

/**
 * Maps key to value only if no existing mapping for key exists.
 * @return the new value
 */
void *HashMap_setdefault(HashMap *self, const void *key,
                         void *value);

/**
 * Maps key to value only if no existing mapping for key exists or
 * the key is mapped to NULL.
 * @return the new value
 */
void *HashMap_putIfAbsent(HashMap *self, const void *key,
                          void *value);

/**
 * Maps key to nothing.
 * @return the old value, or NULL if absent
 */
void *HashMap_remove(HashMap *self, const void *key);

/**
 * Makes an iterator over the items in a map.  If items are added or removed, the behavior is undefined.
 */
HashMapIterator *HashMap_iter(const HashMap *self);

/**
 * Moves to the next 
 */
void HashMapIterator_next(HashMapIterator *self, const void **out_key,
                          void **out_value);

/**
 * Returns nonzero if there are more items.
 */
int HashMapIterator_hasNext(const HashMapIterator *self);

#endif
