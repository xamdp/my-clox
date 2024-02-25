/* A module to define my code representation */
/* chunk - refers to sequences of bytecode */
/* bytecode - resembles machine code, linear sequence of binary instructions.
 * (In many bytecode formats, each instruction is only a single byte long,
 * hence "bytecode".)*/
#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

/* Each instruction has a one-byte operation code (universally shortened 
 * to opcode). That number controls what kind of instruction we're dealing 
 * with --add, subtract, look up variable, etc. Those are defined here: */
typedef enum {
	OP_RETURN,	/* return from the current function */
} OpCode;

/*
 * 1. Allocate a new array with more capacity.
 * 2. Copy the existing elements from the old array to the new one.
 * 3. Store the new capacity.
 * 4. Delete the old array.
 * 5. Update code to point to the new array.
 * 6. Store the element in the new array.
 * 7. Update the count.
 * */

/* A struct to hold the series of instructions, Bytecode and some other data */
typedef struct {
	int capacity;	/* the number of elements in the array we have allocated */
	int count;	/* how many of those allocated entries are actually in use */
	uint8_t* code;
} Chunk;

/* Initializes a new chunk */
void initChunk(Chunk* chunk);
/* Appends a byte to the end of the chunk */
void writeChunk(Chunk* chunk, uint8_t byte);

#endif
