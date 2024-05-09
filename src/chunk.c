#include <stdlib.h>

#include "lib/chunk.h"
#include "lib/memory.h"

void initChunk(Chunk *chunk) {
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lines = NULL;
	initValueArray(&chunk->constants);	// Initializes constant list when a new chunk is initialized
}

void freeChunk(Chunk *chunk) { 
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(int, chunk->lines, chunk->capacity);
	freeValueArray(&chunk->constants);	// frees the constants when we free the chunk
	initChunk(chunk);
}
void writeChunk(Chunk *chunk, uint8_t byte, int line) {	/* writeChunk() can write opcodes or operands. It's all raw bytes as fas as that function is concerned. */
	if (chunk->capacity < chunk->count + 1) {
		int oldCapacity = chunk->capacity;
		chunk->capacity =  GROW_CAPACITY(oldCapacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity); /* When we allocate or grow the array,*/
		chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity); /* we do the same for the line info too */
	}

	chunk->code[chunk->count] = byte;
	chunk->lines[chunk->count] = line;	/* we store the line number in the array */
	chunk->count++;
}

int addConstant(Chunk *chunk, Value value) {
	writeValueArray(&chunk->constants, value);
	return chunk->constants.count - 1;	/* chunk ptr is accessing a field of ValueArray struct that is within the Chunk struct */
	/* after we add the constant, we return the index where was appended so that we can locate that same constant later*/
}
