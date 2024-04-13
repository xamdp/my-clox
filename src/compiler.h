#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

bool compile(const char* source, Chunk* chunk);	/* We pass in the chunk where the compiler will write the code
and then compile() returns wheter or not compilation succeeded. */

#endif
