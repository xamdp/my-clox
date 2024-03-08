#include <stdlib.h>

#include "memory.h"

void* reallocate(void *pointer, size_t oldSize, size_t newSize) {
	if (newSize == 0) {
		free(pointer);	// When newSize is zero, we handle the deallocation case ourselves by calling free()
		return NULL;
	}

	// Otherwise, we rely on the C standard library's realloc()
	void *result = realloc(pointer, newSize);
	if (result == NULL) exit(1);	// allocation can fail if there isn't enough memory and realloc() will return NULL
	return result;
}
