#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/common.h"
#include "lib/chunk.h"
#include "lib/debug.h"
#include "lib/vm.h"

static void repl() {
	char line[1024];
	for (;;) {
		printf("> ");

		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}

		interpret(line);
	}
}

static char *readFile(const char *path) {
	FILE *file = fopen(path, "rb");
	if (file == NULL) {	/* If the user doesn't have permission or a wrong path/to/file is provided */
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	char *buffer = (char*)malloc(fileSize + 1);	/* we add 1 for the null byte */ 
	if (buffer == NULL) {	/* If there is not enough memory and we failed to read the Lox script*/
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(74);
	}
	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
		exit(74);
	}
	buffer[bytesRead] = '\0';

	fclose(file);
	return buffer;
}

static void runFile(const char *path) {
	char *source = readFile(path);
	InterpretResult result = interpret(source);
	free(source);

	if (result == INTERPRET_COMPILE_ERROR) exit(65);
	if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

/* From this tiny seed, I will grow my entire VM */
int main(int argc, const char* argv[]) {
	printf("Hello\n");
	initVM();

	if (argc == 1) {
		repl();
	} else if (argc == 2) {
		runFile(argv[1]);
	} else {
		fprintf(stderr, "Usage: clox [path]\n");
		exit(64);
	}

	freeVM();
	return 0;
}
