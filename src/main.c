#include "common.h"
#include "chunk.h"
#include "debug.h"

/* From this tiny seed, I will grow my entire VM */
int main(int argc, const char* argv[]) {
	Chunk chunk;
	initChunk(&chunk);
	
	int constant = addConstant(&chunk, 1.2); /* We add the constant value itself to the chunk's constant pool. That returns the index of the constant in the array */
	writeChunk(&chunk, OP_CONSTANT, 123); /* Then we write the constant instruction, starting with its opcode.*/	
	writeChunk(&chunk, constant, 123); /* After that, we write the one-byte constant index operand*/

	writeChunk(&chunk, OP_RETURN, 0);
	disassembleChunk(&chunk, "test chunk");
	freeChunk(&chunk);
	return 0;
}
