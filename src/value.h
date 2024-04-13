#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

/* For now, we have only a couple of cases, but this will grow as we add strings, functions, and classes to clox.*/
typedef enum {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
	VAL_OBJ	/* we refer to this new ValueType case for all heap-allocated types. */
} ValueType;

/* 
 We also need to store the data for the value, the double for a number, true or false for a Boolean.
 We could define a struct with fields for each possible type.
 But this is a waste of memory. A value can't simultaneously be both a number and a Boolean.
 So at any point in time, only one of those fields will be used.
 */
typedef struct {
	ValueType type;	/* a variable to store the type of value */
	union {
		bool boolean;
		double number;
		Obj* obj;	/* when a Value's type is VAL_OBJ, the payload is a pointer to the heap memory.*/
	} as;
} Value;

/* Macros to check the Value's type */
#define IS_BOOL(value)		((value).type == VAL_BOOL)	/* we access fields using (value).type if we are directly accessing it from the struct */
#define IS_NIL(value)		((value).type == VAL_NIL)	/* we use the arrow -> operator when we have a pointer to a struct. */
#define IS_NUMBER(value)	((value).type == VAL_NUMBER)
#define IS_OBJ(value)		((value).type == VAL_OBJ)	/* as we did with other value types, we create a macro for working with Obj values. */

#define AS_OBJ(value)		((value).as.obj)	/* this evaluates to true if the given Value is an Obj. */
#define AS_BOOL(value)		((value).as.boolean)
#define AS_NUMBER(value)	((value).as.number)		

/* 
 BOOL_VAL macro is used to create a Value struct representing a boolean value.
 When you use BOOL_VAL(myValue), it creates a Value struct with type set to VAL_BOOL
 and the boolean field set to the value of the myValue variable.
 */
#define BOOL_VAL(value)		((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL				((Value){VAL_NIL, {.number = 0}})	/* initializing a nil data type set the number field to 0 by default */
#define NUMBER_VAL(value)	((Value){VAL_NUMBER, {.number = value}}) /* casts the provided argument for the macro to a Value type and initializes the number field of the Value struct with the input value. */
#define OBJ_VAL(object)		((Value){VAL_OBJ, {.obj = (Obj*)object}})	/* it extracts the Obj pointer from the value.*/
/* This takes as bare Obj pointer and wraps it in a full Value. */

typedef struct {
	int capacity;
	int count;
	Value *values;	/* a pointer to a type of double */ 
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif
