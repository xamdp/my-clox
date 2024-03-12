#include <stdio.h>
#include "common.h"
#include "debug.h"
#include "vm.h"

VM vm;

/* Points the *stackTop pointer to the beginning of the array to indicate that
 * the stack is empty. */
static void resetStack() {
	vm.stackTop = vm.stack;
}

void initVM() {
	resetStack();
}

void freeVM() {
}

/* Push a new value onto the top of the stack */
void push(Value value) {
	*vm.stackTop = value;
	vm.stackTop++;
}

Value pop() {
	vm.stackTop--;
	return *vm.stackTop;

}

/* The beating heart of the VM */
static InterpretResult run() {	/* For other techniques, look up direct threaded code, jump table and computed goto */
#define READ_BYTE() (*vm.ip++)	/* reads the byte currently pointed at by ip and then advances the instruction pointer */
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) /* reads the next byte from the bytecode, treats the resulting number as an index,
and looks up the corresponding Value in the chunk's constant table. */
/* a placeholder for the values and the binary operator */
#define BINARY_OP(op) \
	do { \
		double b = pop(); \
		double a = pop(); \
		push(a op b); \
	} while (false)

	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("		");
		for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
			printf("[");
			printValue(*slot);
			printf("]");
		}
		printf("\n");
		disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t instruction;
		switch (instruction = READ_BYTE()) {
			case OP_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(constant);
				break;
			}
			case OP_ADD:		BINARY_OP(+); break;
			case OP_SUBTRACT:	BINARY_OP(-); break;
			case OP_MULTIPLY:	BINARY_OP(*); break;
			case OP_DIVIDE:		BINARY_OP(/); break;
			case OP_NEGATE:	push(-pop()); break;
			case OP_RETURN: {
				printValue(pop());
				printf("\n");
				return INTERPRET_OK;
			}
		a}
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(Chunk *chunk) {
	vm.chunk = chunk;
	vm.ip = vm.chunk->code;
	return run();
}
