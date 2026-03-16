// copied and modified, from https://github.com/goldsborough/dyn_htable

#ifndef dyn_htable_H
#define dyn_htable_H

#include <stdbool.h>
#include <stddef.h>

/****************** DEFINTIIONS ******************/

#define HT_MINIMUM_CAPACITY 8
#define HT_LOAD_FACTOR 5
#define HT_MINIMUM_THRESHOLD (HT_MINIMUM_CAPACITY) * (HT_LOAD_FACTOR)

#define HT_GROWTH_FACTOR 2
#define HT_SHRINK_THRESHOLD (1 / 4)

#define HT_ERROR -1
#define HT_SUCCESS 0

#define HT_UPDATED 1
#define HT_INSERTED 0

#define HT_NOT_FOUND 0
#define HT_FOUND 01

#define HT_INITIALIZER {0, 0, 0, 0, 0, NULL, NULL, NULL};

typedef int (*comparison_t)(void*, void*, size_t);
typedef size_t (*hash_t)(void*, size_t);

/****************** STRUCTURES ******************/

typedef struct HTNode {
	struct HTNode* next;
	void* key;
	void* value;

} HTNode;

typedef struct dyn_htable {
	size_t size;
	size_t threshold;
	size_t capacity;

	size_t key_size;
	size_t value_size;

	comparison_t compare;
	hash_t hash;

	HTNode** nodes;

} dyn_htable;

/****************** INTERFACE ******************/

/* Setup */
int ht_setup(dyn_htable* table,
				size_t key_size,
				size_t value_size,
				size_t capacity);

int ht_copy(dyn_htable* first, dyn_htable* second);
int ht_move(dyn_htable* first, dyn_htable* second);
int ht_swap(dyn_htable* first, dyn_htable* second);

/* Destructor */
int ht_destroy(dyn_htable* table);

int ht_insert(dyn_htable* table, void* key, void* value);

int ht_contains(dyn_htable* table, void* key);
void* ht_lookup(dyn_htable* table, void* key);
const void* ht_const_lookup(const dyn_htable* table, void* key);

int ht_erase(dyn_htable* table, void* key);
int ht_clear(dyn_htable* table);

int ht_is_empty(dyn_htable* table);
bool ht_is_initialized(dyn_htable* table);

int ht_reserve(dyn_htable* table, size_t minimum_capacity);

/****************** PRIVATE ******************/

void _ht_int_swap(size_t* first, size_t* second);
void _ht_pointer_swap(void** first, void** second);

size_t _ht_default_hash(void* key, size_t key_size);
int _ht_default_compare(void* first_key, void* second_key, size_t key_size);

size_t _ht_hash(const dyn_htable* table, void* key);
bool _ht_equal(const dyn_htable* table, void* first_key, void* second_key);

bool _ht_should_grow(dyn_htable* table);
bool _ht_should_shrink(dyn_htable* table);

HTNode* _ht_create_node(dyn_htable* table, void* key, void* value, HTNode* next);
int _ht_push_front(dyn_htable* table, size_t index, void* key, void* value);
void _ht_destroy_node(HTNode* node);

int _ht_adjust_capacity(dyn_htable* table);
int _ht_allocate(dyn_htable* table, size_t capacity);
int _ht_resize(dyn_htable* table, size_t new_capacity);
void _ht_rehash(dyn_htable* table, HTNode** old, size_t old_capacity);

#endif /* dyn_htable_H */