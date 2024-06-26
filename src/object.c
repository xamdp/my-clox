#include <stdio.h>
#include <string.h>

#include "lib/memory.h"
#include "lib/object.h"
#include "lib/table.h"
#include "lib/value.h"
#include "lib/vm.h"

/* Like the previous macro, this exists mainly to avoid the need to redundantly
 * cast a void* back to the desired type.*/
#define ALLOCATE_OBJ(type, objectType) \
	(type*)allocateObject(sizeof(type), objectType)

/* here is the actual implementation of the macro.
 * It allocates an object of the given size on the heap.
 * Note that the size is not just the size of Obj itself.
 * The caller passes in the numbber of bytes so that
 * there is room for the extra payload fields needed
 * by the specific object type being created.*/
static Obj* allocateObject(size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(NULL, 0, size);
	object->type = type;
	object->next = vm.objects;	/* Every time we allocate an Obj, we insert it in the list.*/
	vm.objects = object;
	return object;
}
/* then it initializes the Obj state -- right now, that's just the type tag.
 * This function returns to allocateString(), which finishes initializing
 * the ObjString fields.*/


/* It creates a new ObjString on the heap and then initializes its fields.
 * It's sort of like a constructor in an OOP language. As such, it first
 * calls the "base class" constructor to initialize the Obj state, using
 * the new macro ALLOCATE_OBJ*/
static ObjString* allocateString(char* chars, int length, uint32_t hash) {	/* Whenever we allocate string, we pass in its hash code.*/
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	tableSet(&vm.strings, string, NIL_VAL);	/* I think string is the key and after setting up the table it returns the */
	return string;
}

static uint32_t hashString(const char* key, int length) {
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t)key[i];
		hash *= 16777619;
	}
	return hash;
}

ObjString* takeString(char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}
	return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) return interned;
	char * heapChars =  ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);	/* destination, source, size*/
	heapChars[length] = '\0';
	return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
		case OBJ_STRING:
			printf("%s", AS_CSTRING(value));
			break;
	}
}
