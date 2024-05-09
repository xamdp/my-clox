#include <stdlib.h>

#include "lib/memory.h"
#include "lib/vm.h"

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

static void freeObject(Obj* object) {
	switch (object->type) {
		case OBJ_STRING: {
			ObjString* string = (ObjString*)object;
			FREE_ARRAY(char, string->chars, string->length + 1);
			FREE(ObjString, object);
			break;
		}
	}
}

/* A function for freeing the objects */
void freeObjects() {
	Obj* object = vm.objects;
	while (object != NULL) {	/*if it hasn't reached yet the end of the list, i suppose?*/
		Obj* next = object->next;	/* retrieves the pointer to the next object in the linked list, freeds the current and move to the next.*/
		freeObject(object);	/* This is responsible for freeing the object.*/
		object = next; /* after freeing, update the object pointer to point to the next object in the linked list, which continues the loop.*/
	}
}
