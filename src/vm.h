#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
	// a pointer to Chunk struct
	Chunk *chunk; /* This is the chunk that my VM will executes */
	uint8_t *ip; /* a 8bit/byte pointer, instruction pointer */
	Value stack[STACK_MAX];
	Value *stackTop;
	Table strings;
	Obj* objects;	/* the vm stores a pointer to the head of the list.*/
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
/* Accepts a pointer that contains the source code */
InterpretResult interpret(const char *source); /* responsible for interpreting the code contained in the Chunk struct */

/* The stack protocol supports two operations */

void push(Value value);
Value pop();

#endif
