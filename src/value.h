#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;	/* an alias for double data type */

typedef struct {
	int capacity;
	int count;
	Value *values;	/* a pointer to a type of double */ 
} ValueArray;

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
void printValue(Value value);

#endif
