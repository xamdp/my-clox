#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lib/common.h"
#include "lib/compiler.h"
#include "lib/debug.h"
#include "lib/object.h"
#include "lib/memory.h"
#include "lib/vm.h"

VM vm;

/* Points the *stackTop pointer to the beginning of the array to indicate that
 * the stack is empty. */
static void resetStack() {
	vm.stackTop = vm.stack;
}

static void runtimeError(const char* format, ...) {
	va_list args; /* this let us pass an arbitrary number of arguments to runtimeError(). */
	va_start(args, format);
	vfprintf(stderr, format, args); /* flavor of printf() that takes an explicit va_list */
	va_end(args);
	fputs("\n", stderr);

	size_t instruction = vm.ip - vm.chunk->code - 1;
	int line = vm.chunk->lines[instruction];
	fprintf(stderr, "[line %d] in script\n", line);
	resetStack();
}

void initVM() {
	resetStack();
	vm.objects = NULL;	/* When we first initialize the VM, there are no allocated objects.*/

	initTable(&vm.globals);	/* we need to initialize the hash table to a valid state when the VM boots up */
	initTable(&vm.strings);	/* pass the address of field strings via vm and the & operator.*/
}

void freeVM() {
	freeTable(&vm.globals);	/* also this */
	freeTable(&vm.strings);	/* when the vm is shut down, we clean up any resources used by the table. */
	freeObjects();
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

static Value peek(int distance) {
	return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

/* a function to concatenate strings*/
static void concatenate() {
	ObjString* b = AS_STRING(pop());
	ObjString* a = AS_STRING(pop());

	int length = a->length + b->length;
	char* chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjString* result = takeString(chars, length);
	push(OBJ_VAL(result));
}

/* The beating heart of the VM */
static InterpretResult run() {	/* For other techniques, look up direct threaded code, jump table and computed goto */
#define READ_BYTE() (*vm.ip++)	/* reads the byte currently pointed at by ip and then advances the instruction pointer */
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) /* reads the next byte from the bytecode, treats the resulting number as an index,
and looks up the corresponding Value in the chunk's constant table. */
/* a placeholder for the values and the binary operator */ 
#define READ_STRING()	AS_STRING(READ_CONSTANT())	/* a macro inside a macro that is also inside a macro */
#define BINARY_OP(ValueType, op) \
	do { \
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
			runtimeError("Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
	} \
		double b = AS_NUMBER(pop()); \
		double a = AS_NUMBER(pop()); \
		push(ValueType(a op b)); \
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
			case OP_NIL: push(NIL_VAL); break;
			case OP_TRUE: push(BOOL_VAL(true)); break;
			case OP_FALSE: push(BOOL_VAL(false)); break;
			case OP_POP: pop(); break;	/* as the name implies, it pops the top value off the stack and forgets it.*/
			case OP_GET_GLOBAL: {
				ObjString* name = READ_STRING();
				Value value;
				if (!tableGet(&vm.globals, name, &value)) {
					runtimeError("Undefined variable '%s'.", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				push(value);
				break;
			}
			case OP_DEFINE_GLOBAL: {
				ObjString* name = READ_STRING();
				tableSet(&vm.globals, name, peek(0));
				pop();
				break;
			}
			case OP_SET_GLOBAL: {
				ObjString* name = READ_STRING();
				if (tableSet(&vm.globals, name, peek(0))) {
					tableDelete(&vm.globals, name);
					runtimeError("Undefined variable '%s'.", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_EQUAL: {
				Value b = pop();
				Value a = pop();
				push(BOOL_VAL(valuesEqual(a, b)));
				break;
			}
			case OP_GREATER:	BINARY_OP(BOOL_VAL, >); break; /* we pass in BOOL_VAL since the result value type is Boolean.*/
			case OP_LESS:		BINARY_OP(BOOL_VAL, <); break;	
			case OP_ADD: {	/* If both operands are strings, it concatenates.*/
				if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
					concatenate();
				} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {	/* If they're both numbers, it adds them.*/
					double b = AS_NUMBER(pop());
					double a = AS_NUMBER(pop());
					push(NUMBER_VAL(a + b));
				} else {	/* Any other combination of operand types is a runtime error.*/
					runtimeError("Operands must be two numbers or two strings.");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SUBTRACT:	BINARY_OP(NUMBER_VAL, -); break;
			case OP_MULTIPLY:	BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIVIDE:		BINARY_OP(NUMBER_VAL, /); break;
			case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;
			case OP_NEGATE:
				if (!IS_NUMBER(peek(0))) {
					runtimeError("Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}
				push(NUMBER_VAL(-AS_NUMBER(pop())));
				break;
			case OP_PRINT: {
				printValue(pop());
				printf("\n");
				break;
			}
			case OP_RETURN: {
				// Exit interpreter.
				// printValue(pop());
				// printf("\n");
				return INTERPRET_OK;
			}
		}
	}
	#undef READ_BYTE
	#undef READ_CONSTANT
	#undef READ_STRING
	#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
	Chunk chunk;
	initChunk(&chunk); /* create a new empty chunk and pass it over to the compiler.
	The compiler will take the user's program and fill up the chunk with bytcode. */

	if (!compile(source, &chunk)) {	/* If it does encounter an error, compile() returns false, and we discard the unusable chunk. */
		freeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;	/* Otherwise, we send the completed chunk over to the VM to be executed. */
	vm.ip = vm.chunk->code;

	InterpretResult result = run();

	freeChunk(&chunk);	/* When the VM finishes, we free the chunk and we're done. */
	return result;
	// compile(source);
	// return INTERPRET_OK;
}
