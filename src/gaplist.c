/* GapList

Copyright 2007 Damian Yerrick

*/

#include "gaplist.h"


/* gaplist.c *******************************************************/

#include <stdlib.h>
#include <string.h>

struct GapList {

  /* Elements from 0 to (insertionPoint - 1) and from
     (capacity - nEls + insertionPoint) to (capacity - 1) are valid.
  */
  void *data;  // the backing array
  size_t elSize;  // size of an element in bytes
  size_t nEls;  // number of valid elements
  size_t insertionPoint;  // point where elements can be inserted
  size_t capacity;  // number of elements in the backing array
};

void Gap_clear(GapList *v) {
  if (v) {
    v->nEls = 0;
    v->insertionPoint = 0;
  }
}

void *Gap_addAll(GapList *restrict v, const void *restrict src, size_t n) {
  if (!v) {
    return NULL;
  }

  // Expand the array's capacity if necessary.  First try growing
  // the array with some room for growth, with a growth policy
  // that allows for constant amortized time cost.  If this fails,
  // try growing the array shrink wrapped.
  if (n + v->nEls > v->capacity
      && !Gap_ensureCapacity(v, n + v->nEls + v->nEls / 2)
      && !Gap_ensureCapacity(v, n + v->nEls)) {
    return NULL;
  }
  v->nEls += n;

  // Move the insertion point forward past some uninitialized
  // elements, and overwrite these elements.
  size_t oldIP = v->insertionPoint;
  v->insertionPoint = oldIP + n;
  Gap_setRange(v, oldIP, oldIP + n, src);
  return Gap_get(v, oldIP);
}

bool Gap_ensureCapacity(GapList *v, size_t capacity) {
  if (!v) {
    return false;
  }
  
  // Don't try shrinking the list past the minimum number
  // of elements needed to store the data.
  if (capacity < v->nEls) {
    capacity = v->nEls;
  }

  size_t oldIP = v->insertionPoint;

  // If insertion point is not at end, move to end.
  // This is suboptimal but should satisfy the test suite.
  Gap_seek(v, v->nEls);

  // Try reallocating the memory.
  void *newMem = realloc(v->data, v->elSize * capacity);

  // If reallocation succeeded, use the new memory.
  if (newMem) {
    v->data = newMem;
    v->capacity = capacity;
  }

  Gap_seek(v, oldIP);
  return newMem ? true : false;
}

GapList *Gap_clone(GapList *v) {
  if (!v) {
    return NULL;
  }

  GapList *clone = Gap_new(v->elSize, v->nEls);
  if (!clone) {
    return NULL;
  }
  Gap_addAll(clone,
             v->data,
             v->insertionPoint);
  Gap_addAll(clone,
             Gap_get(v, v->insertionPoint),
             v->nEls - v->insertionPoint);
  return clone;
}

GapList *Gap_new(size_t elSize, size_t capacity) {
  GapList *v = malloc(sizeof(GapList));
  if (!v) {
    return NULL;
  }

  v->data = malloc(capacity * elSize);
  if (!v->data) {
    free(v);
    return NULL;
  }

  v->elSize = elSize;
  v->capacity = capacity;
  Gap_clear(v);

  return v;
}

void *Gap_get(GapList *v, size_t i) {
  if (!v) {
    return NULL;
  }
  if (i >= v->nEls) {
    return NULL;
  }
  if (i >= v->insertionPoint) {
    i += v->capacity - v->nEls;
  }
  return (char *)(v->data) + i * v->elSize;
}

void *Gap_getRange(GapList *v, size_t fromIndex, size_t toIndex) {
  if (!v) {
    return NULL;
  }
  if (v->insertionPoint > fromIndex && v->insertionPoint < toIndex) {
    Gap_seek(v, toIndex);
  }
  return Gap_get(v, fromIndex);
}

size_t Gap_size(GapList *v) {
  return v ? v->nEls : 0;
}

size_t Gap_elSize(GapList *v) {
  return v ? v->elSize : 0;
}

bool Gap_removeBefore(GapList *v, size_t n) {
  if (!v) {
    return false;
  }
  if (v->insertionPoint < n) {
    return false;
  }

  // forget n elements before the insertion point
  v->nEls -= n;
  v->insertionPoint -= n;
  return true;
}

bool Gap_removeAfter(GapList *v, size_t n) {
  if (!v) {
    return false;
  }

  // make sure there are enough elements to delete
  if (v->nEls < v->insertionPoint + n) {
    return false;
  }

  // forget n elements after the insertion point
  v->nEls -= n;
  return true;  
}

bool Gap_removeRange(GapList *v, size_t fromIndex, size_t toIndex) {
  if (!v || toIndex <= fromIndex || toIndex > v->nEls) {
    return false;
  }

  // First seek to somewhere in this range
  if (fromIndex > v->insertionPoint) {
    Gap_seek(v, fromIndex);
  } else if (toIndex < v->insertionPoint) {
    Gap_seek(v, toIndex);
  }

  Gap_removeAfter(v, toIndex - v->insertionPoint);
  Gap_removeBefore(v, v->insertionPoint - fromIndex);
  return true;
}

void Gap_setRange(GapList *restrict v, size_t fromIndex, size_t toIndex, const void *restrict src) {
  if (!v) {
    return;
  }
  if (toIndex <= fromIndex || toIndex > v->nEls) {
    return;
  }

  // If copy needs to be split across the insertion point,
  // copy the portion before the insertion point
  if (fromIndex < v->insertionPoint && toIndex >= v->insertionPoint) {
    size_t nBytesToCopy = (v->insertionPoint - fromIndex) * v->elSize;
    memcpy(Gap_get(v, fromIndex), src, nBytesToCopy);
    src = (char *)src + nBytesToCopy;
    fromIndex = v->insertionPoint;
  }

  if (fromIndex < toIndex) {
    size_t nBytesToCopy = (toIndex - fromIndex) * v->elSize;
    memcpy(Gap_get(v, fromIndex), src, nBytesToCopy);
  }
}

void Gap_seek(GapList *v, size_t newIP) {
  if (!v) {
    return;
  }
  if (newIP > v->nEls) {
    newIP = v->nEls;
  }
  size_t oldIP = v->insertionPoint;

  if (newIP < oldIP) {

    // Seeking backward.  Move elements starting at newIP
    // to end of gap.
    void *src = Gap_get(v, newIP);
    v->insertionPoint = newIP;
    void *dst = Gap_get(v, newIP);
    memmove(dst, src, (oldIP - newIP) * v->elSize);
  } else if (newIP > oldIP) {

    // Seeking forward.  Move elements starting at oldIP
    // to start of gap.
    void *src = Gap_get(v, oldIP);
    v->insertionPoint = newIP;
    void *dst = Gap_get(v, oldIP);
    memmove(dst, src, (newIP - oldIP) * v->elSize);
  }
}

size_t Gap_tell(GapList *v) {
  return v ? v->insertionPoint : 0;
}

void Gap_delete(GapList *v)
{
  if (v) {

    // Extra check here because some libc implementations
    // crash on free(NULL).
    if (v->data) {
      free(v->data);
    }
    free(v);
  }
}
