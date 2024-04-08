#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
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
typedef void (*ParseFn)();

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

Parser parser;
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

static void endCompiler() {
	emitReturn();
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
	disassembleChunk(currentChunk(), "code");
}
#endif
}

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void binary() {
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
static void literal() {
	switch (parser.previous.type) {
		case TOKEN_FALSE: emitByte(OP_FALSE); break;
		case TOKEN_NIL: emitByte(OP_NIL); break;
		case TOKEN_TRUE: emitByte(OP_TRUE); break;
		default: return; // Unreachable
	}
}

static void grouping() {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number() {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void unary() {
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
	[TOKEN_IDENTIFIER]		= {NULL,	NULL,	PREC_NONE},
	[TOKEN_STRING]			= {NULL,	NULL,	PREC_NONE},
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

	prefixRule();

	while(precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule();
	}
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

bool compile(const char* source, Chunk* chunk) {
	initScanner(source); /* the compiler set up the scanner */
	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	advance();	/* primes the pump on the scanner */
	expression();
	consume(TOKEN_EOF, "Expect end of expression.");
	endCompiler();
	return !parser.hadError;
}
