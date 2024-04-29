#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

/* a function to initialize the parts of the table, setting count and capacity initially to 0
 * and entries to NULL, which denotes that they are empty for starter. */
void initTable(Table* table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void freeTable(Table* table) {
	FREE_ARRAY(Entry, table->entries, table->capacity);
	initTable(table);
}

/* real core of the hash table. Responsible for taking a key and array of buckets,
 * and figuring out which bucket the entry belongs in. This function is also
 * where linear probing and collision handling come into play.*/
static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
	uint32_t index = key->hash % capacity;
	Entry* tombstone = NULL;	/* the first time we pass a tombstone, we store it in this local variable.*/

	for (;;) {
		Entry* entry = &entries[index];
		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				// Empty entry.
				return tombstone != NULL ? tombstone : entry;
			} else {
				// We found a tombstone
				if (tombstone == NULL) tombstone = entry;
			}
		} else if (entry->key == key) {
			// We found the key.
			return entry;
		}

		index = (index + 1) % capacity;
	}
}

/* a function for retrieving values*/
bool tableGet(Table* table, ObjString* key, Value* value) {
	if (table->count == 0) return false;	/* If the table is empty, we won't find the entry, so we check that first.*/

	Entry* entry = findEntry(table->entries, table->capacity, key);	/*findEntry does the job for finding a entry and it's key*/
	if (entry->key == NULL) return false;

	*value = entry->value;
	return true;
}

/* A function responsible for Allocating and rezising array of buckets*/
static void adjustCapacity(Table* table, int capacity) {
	Entry* entries = ALLOCATE(Entry, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}

	/* walk through the old array front to back. when found a non-empty bucket,
	 * we insert that into the new array. use findEntry(), passing in the new
	 * array instead of the one currently stored in the Table.*/
	table->count = 0;	/* We recalculate the count since it may change during a resize.*/
	for (int i = 0; i < table->capacity; i++) {
		Entry* entry = &table->entries[i];	/* store the address of per entries in a Entry pointer*/
		if (entry->key == NULL) continue;	/* if the key of that entry pointer points to is NULL then it is empty, we move to the next entry.*/

		Entry* dest = findEntry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;	/* Then each time we find a non-tombstone entry, we increment it.*/
	}

	/* After rebuilding the table, transferring all entries to the new array, we free the memory allocated for the old array.*/
	FREE_ARRAY(Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

/* This function adds the given key/value pair to the given hash table.
 * If an entry for that key is already present, the new value overwrites
 * the old value. The function returns true if a new entry was added.*/
bool tableSet(Table* table, ObjString* key, Value value) {
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {	/* don't grow when capacity is full, we grow when array is at least 75% full. */
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}

	Entry* entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = entry->key == NULL;
	if (isNewKey && IS_NIL(entry->value)) table->count++;	
	/* We only increment the count when an entry key and value is both NULL and NIL*/

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
	if (table->count == 0) return false;

	// Find the entry.
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	// Place a tombstone in the entry.
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

/* helper fucntion for copying all of the entries of one hash table into another.*/
void tableAddAll(Table* from, Table* to) {
	for (int i = 0; i < from->capacity; i++) {
		Entry* entry = &from->entries[i];
		if (entry->key != NULL) {
			tableSet(to, entry->key, entry->value);
		}
	}
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
	if (table->count == 0) return NULL;

	uint32_t index = hash % table->capacity;
	for (;;) {
		Entry* entry = &table->entries[index];
		if (entry->key == NULL) {
			// Stop if we find an empty non-tombstone entry.
			if (IS_NIL(entry->value)) return NULL;
		} else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
			// We found it.
			return entry->key;
		}

		index = (index + 1) % table->capacity;
	}
}
