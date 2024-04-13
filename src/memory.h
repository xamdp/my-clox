#ifndef clox_memory_h
#define clox_memory_h


#include "common.h"
#include "object.h"

/* we allocate a new array on the heap, just big enough for the string's 
 * characters and the trailing terminator, using this low-level macro that
 * allocates an array with a given element type and count: */
#define ALLOCATE(type, count) \
	(type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

/* This macro calculates a new capacity based on a given current capacity.
 * It also handles when the current capacity is zero, it jumps straight to
 * eight elements instead of starting to one.*/
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8: (capacity) * 2)

/* Once we know the desired capacity, we create or grow the array to that size using GROW_ARAYY() */ 

/* This macro pretties up a function call to reallocate() where the real work happens. The macro itself takes care of getting the size of array's element
 * type and casting the resulting void* back to a pointer of the right type. */
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
(type*)reallocate(pointer, sizeof(type) * (oldCount), \
				  sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
reallocate(pointer, sizeof(type) * (oldCount), 0)

/* This reallocate() is the single function we'll use for all dynamic memory management in clox --allocating memory, freeing it, and changing the size of
 * an existing allocation. Routing all of those operations through a single 
 * function will be important later when we add a garbage collector that needs to keep track of how much memory is in use. */ 

/* A function for all dynamic memory management, the two size arguments passed control which operation to perform. */
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void freeObjects();

#endif
