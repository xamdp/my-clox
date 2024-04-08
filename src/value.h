#ifndef clox_value_h
#define clox_value_h

#include "common.h"

/* For now, we have only a couple of cases, but this will grow as we add strings, functions, and classes to clox.*/
typedef enum {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
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
	} as;
} Value;

/* Macros to check the Value's type */
#define IS_BOOL(value)		((value).type == VAL_BOOL)	/* we access fields using (value).type if we are directly accessing it from the struct */
#define IS_NIL(value)		((value).type == VAL_NIL)	/* we use the arrow -> operator when we have a pointer to a struct. */
#define IS_NUMBER(value)	((value).type == VAL_NUMBER)

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
