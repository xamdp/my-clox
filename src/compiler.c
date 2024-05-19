#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/common.h"
#include "lib/compiler.h"
#include "lib/scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "lib/debug.h"
#endif

typedef struct {
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_OR, // or
	PREC_AND, // and
	PREC_EQUALITY, // == !=
	PREC_COMPARISON, // < > <= >=
	PREC_TERM, // + -
	PREC_FACTOR, // * /
	PREC_UNARY, // ! -
	PREC_CALL, // . ()
	PREC_PRIMARY
} Precedence;

/*
 ParseFn type is a simple typedef for a function type 
 that takes no arguments and returns nothing.
 */
typedef void (*ParseFn)(bool canAssign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

/* each local in the array is one of these: */
typedef struct {
	Token name;
	int depth;
} Local;

/* states for defining local variables */
typedef struct {
	Local locals[UINT8_COUNT];
	int localCount;
	int scopeDepth;
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

/*
 The chunk that we're writing gets passed into compile(), but it needs to make
 its way to emitByte(). To do that, we rely on this intermediary function.
 */
static Chunk* currentChunk() {
	return compilingChunk;
}

/*
 The actual work happens here:
 First, we print where the error occurred. We try to show the lexeme
 if it's human-readable. Then we print the error message itself. After that,
 we set this hadError flag. That records whether any error occurred during
 compilation. This field hadError lives in the parser struct.
 */
static void errorAt(Token* token, const char* message) {
	if (parser.panicMode) return;	/* While the panic mode flag is set, we simply suppress any other errors that get detected. */
	parser.panicMode = true;
	fprintf(stderr, "[line %d] Error", token->line);

	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
		// Nothing
	} else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

static void error(const char* message) {
	errorAt(&parser.previous, message);
}

/*
 If the scanner hand us an error token, we need to actually tell the user.
 We pull the location out the current token in order to tell the user where
 the error occurred and forward it ot errorAt(). 
 */
static void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

/* 
 just like in jlox, it steps forward through the token stream. 
 It asks the scanner for the next token and stores it for later use.
 Before doing that, it takes the old current token and stashes 
 that in a previous field. That will come in handy later so that we 
 can get at the lexeme after we match a token.
 */
static void advance() {	
	parser.previous = parser.current;

	for (;;) {
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR) break;

		errorAtCurrent(parser.current.start);
	}
}

/*
 It's similar to advance() in that it reads the next token.
 But it also validates that the token has an expected type.
 If not, it reports an error. This function is the foundation
 of most syntax errors in the compiler.
 */
static void consume(TokenType type, const char* message) {
	if (parser.current.type == type) {
		advance();
		return;
	}

	errorAtCurrent(message);
}

static bool check(TokenType type) {	/* returns true if the current passed token has the given type.*/
	return parser.current.type == type;
}


static bool match(TokenType type) {
	if (!check(type)) return false;
	advance();
	return true;
}

/*
 After we parse and understand a piece of the user's program, the next step is to 
 translate that to a series of bytecode instructions. It starts with the easiest
 possible step: appending a single byte to the chunk.
 */
static void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);	/* emitByte() sends in the previous token's line information so that runtime errors are 
	associated with that line. */
}


/*
 Over time, we'll have enough cases where we need to write an opcode followed
 by a one-byte operand that it's worth defining this convenience function.
 */
static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitReturn() {
	emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT8_MAX) {
		error("Too many constants in one chunk.");
		return 0;
	}

	return (uint8_t)constant;
}


static void emitConstant(Value value) {
	emitBytes(OP_CONSTANT, makeConstant(value));
}

/* little function to initialize the compiler which initializes to 0 the localCount
 * and scopeDepth and sets the compiler to NULL */
static void initCompiler(Compiler* compiler) {
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
}

static void endCompiler() {
	emitReturn();
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
	disassembleChunk(currentChunk(), "code");
}
#endif
}

static void beginScope() {
	current->scopeDepth++;
}

static void endScope() {
	current->scopeDepth--;

	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(OP_POP);
		current->localCount--;
	}
}

/* these are function prototypes, a heads up for the compiler */
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

/* this function takes the given token and add its lexeme to the chunk's 
 * constant table as a string. It then returns the index of that constant
 * in the constant table.*/
static uint8_t identifierConstant(Token* name) {
	return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {	/* if the variable has the sentinel depth, it must be a reference to a variable in its own initializer */
				error("Can't read local variable in its own initializer.");	/* and we report that as an error. */
			}
			return i;
		}
	}

	return -1;
}

/* adds a new local variable to the current compiler's list
 * of locals, storing its name and depth of its scope. */
static void addLocal(Token name) {
	if (current->localCount == UINT8_COUNT) {
		error("Too many local variables in function.");
		return;
	}
	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;	/* special sentinel value, reads as initialized */
}

static void declareVariable() {
	if (current->scopeDepth == 0) return;

	Token* name = &parser.previous;
	for (int i = current->localCount -1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}

		if (identifiersEqual(name, &local->name)) {
			error("Already a variable with this name in this scope.");
		}
	}
	addLocal(*name);
}

static void binary(bool canAssign) {
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = getRule(operatorType);	/* Look up the precedence of the current operator. */
	parsePrecedence((Precedence)(rule->precedence + 1));

	switch (operatorType) {
		case TOKEN_BANG_EQUAL:		emitBytes(OP_EQUAL, OP_NOT); break;
		case TOKEN_EQUAL_EQUAL:		emitByte(OP_EQUAL); break;
		case TOKEN_GREATER:			emitByte(OP_GREATER); break;
		case TOKEN_GREATER_EQUAL:	emitBytes(OP_LESS, OP_NOT); break;
		case TOKEN_LESS:			emitByte(OP_LESS); break;
		case TOKEN_LESS_EQUAL:		emitBytes(OP_GREATER, OP_NOT); break;
		case TOKEN_PLUS:			emitByte(OP_ADD); break;
		case TOKEN_MINUS:			emitByte(OP_SUBTRACT); break;
		case TOKEN_STAR:			emitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH:			emitByte(OP_DIVIDE); break;
		default: return; // Unreachable.
	}
}

/*
 When the parser encounters false, nil, or true, in prefix postion,
 it calls this new parser function:
 */
static void literal(bool canAssign) {
	switch (parser.previous.type) {
		case TOKEN_FALSE: emitByte(OP_FALSE); break;
		case TOKEN_NIL: emitByte(OP_NIL); break;
		case TOKEN_TRUE: emitByte(OP_TRUE); break;
		default: return; // Unreachable
	}
}

static void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

/* This takes the string's characters directly from the lexeme. The + 1 and - 2
 * parts trim the leading and trailing quotation marks. It then creates a string
 * object, wraps it in a Value, and stuffs it into the constant table.*/
static void string(bool canAssign) {
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else {
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitBytes(setOp, (uint8_t)arg);
	} else {
		emitBytes(getOp, (uint8_t)arg);
	}
}

static void variable(bool canAssign) {
	namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
	TokenType operatorType = parser.previous.type;

	// Compile the operand.
	parsePrecedence(PREC_UNARY);

	// Emit the operator instruction.
	switch (operatorType) {
		case TOKEN_BANG: emitByte(OP_NOT); break;
		case TOKEN_MINUS: emitByte(OP_NEGATE); break;
		default: return; // Unreachable.
	}
}

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN]		= {grouping, NULL,	PREC_NONE},
	[TOKEN_RIGHT_PAREN]		= {NULL,	NULL,	PREC_NONE},
	[TOKEN_LEFT_BRACE]		= {NULL,	NULL,	PREC_NONE},
	[TOKEN_RIGHT_BRACE]		= {NULL,	NULL,	PREC_NONE},
	[TOKEN_COMMA]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_DOT]				= {NULL,	NULL,	PREC_NONE},
	[TOKEN_MINUS]			= {unary,	binary, PREC_TERM},
	[TOKEN_PLUS]			= {NULL,	binary, PREC_TERM},
	[TOKEN_SEMICOLON]		= {NULL,	NULL,	PREC_NONE},
	[TOKEN_SLASH]			= {NULL,	binary,	PREC_FACTOR},
	[TOKEN_STAR]			= {NULL,	binary,	PREC_FACTOR},
	[TOKEN_BANG]			= {unary,	NULL,	PREC_NONE},
	[TOKEN_BANG_EQUAL]		= {NULL,	binary,	PREC_EQUALITY},
	[TOKEN_EQUAL]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_EQUAL_EQUAL]		= {NULL,	binary,	PREC_EQUALITY},
	[TOKEN_GREATER]			= {NULL,	binary,	PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL]	= {NULL,	binary,	PREC_COMPARISON},
	[TOKEN_LESS]			= {NULL,	binary,	PREC_COMPARISON},
	[TOKEN_LESS_EQUAL]		= {NULL,	binary,	PREC_COMPARISON},
	[TOKEN_IDENTIFIER]		= {variable, NULL,	PREC_NONE},
	[TOKEN_STRING]			= {string,	NULL,	PREC_NONE},
	[TOKEN_NUMBER]			= {number,	NULL,	PREC_NONE},
	[TOKEN_AND]				= {NULL,	NULL,	PREC_NONE},
	[TOKEN_CLASS]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_ELSE]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_FALSE]			= {literal,	NULL,	PREC_NONE},
	[TOKEN_FOR]				= {NULL,	NULL,	PREC_NONE},
	[TOKEN_FUN]				= {NULL,	NULL,	PREC_NONE},
	[TOKEN_IF]				= {NULL,	NULL,	PREC_NONE},
	[TOKEN_NIL]				= {literal,	NULL,	PREC_NONE},
	[TOKEN_OR]				= {NULL,	NULL,	PREC_NONE},
	[TOKEN_PRINT]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_RETURN]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_SUPER]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_THIS]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_TRUE]			= {literal,	NULL,	PREC_NONE},
	[TOKEN_VAR]				= {NULL,	NULL,	PREC_NONE},
	[TOKEN_WHILE]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_ERROR]			= {NULL,	NULL,	PREC_NONE},
	[TOKEN_EOF]				= {NULL,	NULL,	PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL) {
		error("Expect expression.");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);	/* since assignment is the lowest-precedence expression, the only time we allow an assignment is when parsing */
	/* an assignment expression or top-level expression like in an expression statement. */

	while(precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid assignment target.");
	}
}


static uint8_t parseVariable(const char* errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);

	declareVariable();
	if (current->scopeDepth > 0) return 0;

	return identifierConstant(&parser.previous);
}

static void markInitialized() {
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}

	emitBytes(OP_DEFINE_GLOBAL, global);
}

/* 
 It simply returns the rule at the given index.
 It's called by binary() to look up the precedence of the current operator. 
 */
static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

/*
 For this chapter 17, we're only going to worry about four:
 Number literals: 123
 Parentheses for grouping: (123)
 Unary negation: -123
 The four horsemen of the arithmetic: +, -, *, /
 */
static void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

/* if we managed to identify a var keyword we call this function */
static void varDeclaration() {
	uint8_t global = parseVariable("Expect variable name.");	/* the keyword is followed by the variable name, which is compiled by parseVariable() */

	if (match(TOKEN_EQUAL)) {	/* we look for an = sign followed by initializer expression.*/
		expression();
	} else {	/* if the user doesn't initialize the variable, the compiler implicitly initializes it to nil by emitting an OP_NIL opcode */
		emitByte(OP_NIL);
	}
	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");	/* either way we expect the statement to be terminated with (;)*/

	defineVariable(global);
}


static void expressionStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(OP_POP);
}

static void printStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
}

static void synchronize() {
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON) return;
		switch (parser.current.type) {
			case TOKEN_CLASS:
			case TOKEN_FUN:
			case TOKEN_VAR:
			case TOKEN_FOR:
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_PRINT:
			case TOKEN_RETURN:
				return;

			default:
				; // Do nothing.
		}

		advance();
	}
}

/* three operations we need to support:
 * - declaring a new variable using a var statement
 * - accesing the value of a variable using an identifier expression
 * - storing a new value in an existing variable using an assignment expression */

static void declaration() {
	if (match(TOKEN_VAR)) {
		varDeclaration();
	} else {
		statement();
	}

	if (parser.panicMode) synchronize();
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		printStatement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	} else {	/* if we don't see a print keyword, then we must be looking at an expression stmt.*/
		expressionStatement();
	}
}

bool compile(const char* source, Chunk* chunk) {
	initScanner(source); /* the compiler set up the scanner */
	Compiler compiler;
	initCompiler(&compiler);
	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	advance();	/* primes the pump on the scanner */
	while (!match(TOKEN_EOF)) {	/* while match to TOKEN_EOF is false we call declaration */
		declaration();
	}
	// expression();
	// consume(TOKEN_EOF, "Expect end of expression.");
	endCompiler();
	return !parser.hadError;
}
