/*

A gap list generalizes an array list to support fast insertion at
a point other than the end.  It does so by leaving a gap between
the elements before the insertion point and the elements after the
insertion point.  An ordinary array list always places the gap at
the end, but in an editor, it is more useful to place the gap
near the buffer's insertion point.  For example, the text editor
GNU Emacs represents a text buffer as a gap list of characters.

Further reading:
http://en.wikipedia.org/wiki/Gap_buffer
http://www.lazyhacker.com/gapbuffer/gapbuffer.htm
http://www.cs.cmu.edu/~wjh/papers/byte.html

*/


#include <stdbool.h>
#include <sys/types.h>
typedef struct GapList GapList;


/**
 * Creates a new list with elements of the given size.
 * @param elSize sizeof(an element)
 * @param capacity the expected number of elements
 */
GapList *Gap_new(size_t elSize, size_t capacity);

/**
 * Destroys a list, freeing the memory associated with it
 * and its elements.
 */
void Gap_delete(GapList *v);

/**
 * Sets the insertion point to the left of an element.
 * @param v this list
 * @param newIP the position of the insertion point
 */
void Gap_seek(GapList *v, size_t newIP);

/**
 * Returns the current insertion point.
 * @param v this list
 * @return the element before which elements will be inserted
 */
size_t Gap_tell(GapList *v);

/**
 * Copies an array of elements into this list at the insertion point.
 * Adding elements has constant amortized time cost.
 * @param v this list
 * @param src a pointer to the first element
 * @param n the number of elements in the array
 * @return a pointer to the first copied element, or NULL if not added
 */
void *Gap_addAll(GapList *v, const void *src, size_t n);

/**
 * Copies a single element into this list at the insertion point.
 * @param v this list
 * @param src a pointer to the element
 * @return a pointer to the copied element, or NULL if not added
 */
static inline void *Gap_add(GapList *v, const void *src) {
  return Gap_addAll(v, src, 1);
}

/**
 * Removes all elements from this list.
 * @param v this list
 */
void Gap_clear(GapList *v);

/**
 * Returns the number of elements in this list.
 * @param v this list
 * @return the number of elements
 */
size_t Gap_size(GapList *v);

/**
 * Tests if this list has no elements.
 * @param v this list
 * @return true if the number of elements is zero; false otherwise
 */
static inline bool Gap_isEmpty(GapList *v) {
  return Gap_size(v) == 0;
}

/**
 * Increases the capacity of the list to hold at least a minimum
 * number of elements.
 * @return true if successfully resized; false if out of memory.
 * @param v this list
 * @param capacity the minimum number of elements
 */
bool Gap_ensureCapacity(GapList *v, size_t capacity);

/**
 * Trims the capacity of this list to equal the number of elements
 * in this list.
 */
static inline void Gap_trimToSize(GapList *v) {
  Gap_ensureCapacity(v, Gap_size(v));
}

/**
 * Returns a copy of this list and all its elements. The insertion
 * point in the copy is unspecified.
 * @param v this list
 * @return a copy of v
 */
GapList *Gap_clone(GapList *v);

/**
 * Returns a pointer to the element at the specified index in this list.
 * @param v this list
 * @param i the index
 */
void *Gap_get(GapList *v, size_t i);

/**
 * Overwrites a block of elements. For instance, setting elements
 * "from" 2 "to" 6 will replace elements 2, 3, 4, and 5 with the
 * content of a 4-element array.
 * @param v this list
 * @param fromIndex the index of the first element to be set
 * @param toIndex the index of the first element after those that shall be set
 * (same numbering as Python "slice notation")
 * @param src the array to copy from, which must not overlap fromIndex to toIndex of this list
 */
void Gap_setRange(GapList *v, size_t fromIndex, size_t toIndex, const void *src);

/**
 * Overwrites a single element.
 * @param v this list
 * @param i the index of the element to overwrite
 * @param src the element to write
 */
static inline void Gap_set(GapList *v, size_t i, const void *src) {
  Gap_setRange(v, i, i + 1, src);
}

/**
 * Returns a pointer to contiguous elements at the specified index
 * range in this list.  If the insertion point is in the middle,
 * moves the insertion point outside of the range (whose exact
 * location is unspecified).
 * @param v this list
 * @param fromIndex the index of the first element to be removed
 * @param toIndex the index of the first element after those that shall be removed
 * @return a pointer to element fromIndex
 */
void *Gap_getRange(GapList *v, size_t fromIndex, size_t toIndex);

/**
 * Removes elements preceding this list's insertion point.
 * @param v this list
 * @param n the number of elements to remove
 * @return true if at least n elements were there to remove; false if not
 */
bool Gap_removeBefore(GapList *v, size_t n);

/**
 * Removes elements following this list's insertion point.
 * @param v this list
 * @param n the number of elements to remove
 * @return true if at least n elements were there to remove; false if not
 */
bool Gap_removeAfter(GapList *v, size_t n);

/**
 * Removes a block of elements. For instance, removing elements
 * "from" 2 "to" 6 will remove elements 2, 3, 4, and 5.
 * @param v this list
 * @param fromIndex the index of the first element to be removed
 * @param toIndex the index of the first element after those that shall be removed
 * @return true if the specified elements were removed; false if not
 */
bool Gap_removeRange(GapList *v, size_t fromIndex, size_t toIndex);

/**
 * Removes an element.
 * @param v this list
 * @param fromIndex the index of the element to be removed
 * @return true if the specified element was removed; false if not
 */
static inline bool Gap_remove(GapList *v, size_t fromIndex) {
  return Gap_removeRange(v, fromIndex, fromIndex + 1);
}

