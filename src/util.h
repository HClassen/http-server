#ifndef UTIL_HEADER
#define UTIL_HEADER

#include <stddef.h>

//char *strdup(char *s);

/* --------------------------------------------------------------------------- */
/* file methods */

#define FILE_ERROR -1

char *combine(char *resource, char *path);

/* @param path - path to the file
 * @param size - size_t pointer where the size gets stored
 * @return 0 or -1 on error
 */
int filesize(char *path, size_t *size);

/* @param path - path to the file
 * @param filesize - size of the file
 * @return char pointer with the file content, must be freed by caller
 */
char *load_file(char *path, int filesize);

/* --------------------------------------------------------------------------- */
/* heap methods */

typedef struct heap_node_{
    struct heap_node_ *left;
    struct heap_node_ *right;
    struct heap_node_ *parent;
}heap_node;

typedef int (*node_cmp)(heap_node *a, heap_node *b); /* return 1 if a comes before b, 0 else */

typedef struct heap_{
    heap_node *head;
    int nr_nodes;
    node_cmp cmp;
}heap;

/* inits a heap
 * @param heap - pointer to preallocated memory for a heap struct
 * @param cmp - function pointer to a compare function
 * @return void
 */
void heap_init(heap *heap, node_cmp cmp);

/* peek first element in heap
 * @param heap - pointer to heap
 * @return pointer to first element in heap
 */
heap_node* heap_peek_head(heap *heap);

/* remove first element in heap
 * @param heap - pointer to heap
 * @return pointer to first element in heap
 */
heap_node* heap_dequeue_head(heap *heap);

/* inesrt element into heap
 * @param heap - pointer to heap
 * @param new - pointer to new heap_node
 * @return void
 */
void heap_insert(heap *heap, heap_node *new);

/* removes element from heap
 * @param heap - pointer to heap
 * @param new - pointer to heap_node to remove
 * @return void
 */
void heap_remove(heap *heap, heap_node *node);

/* --------------------------------------------------------------------------- */
/* timer methods */

typedef struct timer_{
    time_t duration;
    time_t end;
    heap_node node;
}timer;

/* called when first adding a timer to the heap
 * @param h - heap where the timer gets inserted
 * @param t - the timer, not NULL 
 * @param sec - duration of the timer
 * @return void
 */
void timer_init(heap *h, timer *t, time_t sec);

/* called when adding an allready used timer to the heap with a new duration
 * @param h - heap where the timer gets inserted
 * @param t - the timer, not NULL 
 * @param sec - duration of the timer
 * @return void
 */
void timer_set(heap *h, timer *t, time_t sec);

/* called when adding an allready used timer to the heap with the same duration
 * @param h - heap where the timer gets inserted
 * @param t - the timer, not NULL 
 * @param sec - duration of the timer
 * @return void
 */
void timer_restart(heap *h, timer *t);

/* compares to timers
 * @param n1 - heap_node of the first timer
 * @param n2 - heap_node of the second timer
 * @return n1 == NULL => 0, n2 == NULL => 1, n1 ends before n2 => 1, else => 0
 */
int timer_cmp(heap_node *n1, heap_node *n2);

/* checks if timer is finished
 * @param n - heap_node of the timer
 * @param now - reference time
 * @return timer is done => 1, else => 0
 */
int timer_check(heap_node *n, time_t now);


/* remove a timer from the heap 
 * @param h - heap where the timer gets removed from
 * @param t - the timer
 * @return void
 */
void timer_remove(heap *h, timer *t);

/* --------------------------------------------------------------------------- */
/* hashtable methods */

/* error codes */
#define HASH_OK      0
#define HASH_EINPUT -1
#define HASH_ENOMEM -2
#define HASH_EMAX   -4

#define HASH_MAX    64

#define HASH_FREE_NO  0 /* free neither the key or the value of an hash entry */
#define HASH_FREE_KEY 1 /* free the key of an hash entry */
#define HASH_FREE_VAL 2 /* free the value of an hash entry */

#define hash_cast(type, ptr) ((type *) (ptr))

typedef struct hash_entry_ {
    char *key;
    void *value;
    int free_flags;
}hash_entry;

typedef struct hash_table_ { 
    hash_entry **table;
    int hashsize;
    int count;
    void *data;
}hash_table;

/* creates an new hash table 
 * @param head - pointer to preallocated memory for a hash_table struct
 * @param size - the original size of the hashmap
 * @param data - some data
 * @return error code as defined above
 */
int hash_init(hash_table *head, int size, void *data);

/* frees a hash table, but not head
 * @param head - pointer to a hash_table struct
 * @return error code as defined above
 */
int hash_cleanup(hash_table *head);

/* looks up a key 
 * @param head - hash table
 * @param key - key to lookup
 * @return either an entry or NULL
 */
hash_entry *hash_lookup(hash_table *head, char *key);

/* inserts a new element, resizes the hash table if necessary
 * @param head - hash table
 * @param key - the key
 * @param value - the value to be stored
 * @param free_flags - one or a combination of the flags defined above, combine by or
 * @return error code as defined above
 */
int hash_insert(hash_table *head, char *key, void *value, int free_flags);

/* deletes an element by key
 * @param head - hash table
 * @param key - the key
 * @return error code as defined above
 */
int hash_delete(hash_table *head, char *key);



#endif