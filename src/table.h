#ifndef clox_table_h
#define clox_table_h


#include "common.h"
#include "value.h"

typedef struct {
	ObjString* key;	/* since the key is always a string, we store the ObjString ptr directly instead of wrapping it in a Value.*/
	Value value;
} Entry;

typedef struct {
	int count;
	int capacity;
	Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

#endif

/*
 1. Allocate an array with a size larger than the number of entries needed but still reasonable in size.
 2. Mapping keys to array indices, each	
 * */

/*
 * am beginning to like the hash table data struct.
 * to store a certain key in the index, you need to
 * implement some kind of algorithm to map the keys to its
 * desired indices.
 * */
