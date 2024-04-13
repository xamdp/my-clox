#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)		(AS_OBJ(value)->type)	/* this is an abstraction for accessing the type field of Obj struct. */

#define IS_STRING(value)	isObjType(value, OBJ_STRING)

#define AS_STRING(value)	((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)	(((ObjString*)AS_OBJ(value))->chars)
/* These two macro take a Value that is expected to contain a pointer to a valid
 * ObjString on the heap. The first one returns a the ObjString* pointer. The
 * second one steps through that to return the character array itself, since that's 
 * often what we'll end up needing.*/

typedef enum {
	OBJ_STRING,
} ObjType;

struct Obj {
	ObjType type;
	struct Obj* next;	/* The Obj struct itsefl will be the linked list node. Each Obj gets a pointer to the next Obj in the chain.*/
};

struct ObjString {
	Obj obj;	/* instance of Obj struct, we use OBJ_TYPE to access the type field.*/
	int length;
	char* chars;
};

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

/* I think what this function does is that it checks if the given value
 * is equal to VAL_OBJ and it also checks if the type field   */
static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
