#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "util.h"
#include "http_server_internal.h"

/* char *strdup(char *s){
    int len = strlen(s);
    char *ret = calloc(sizeof(char), len + 1);
    if(ret == NULL){
        perror("failed to allocate memory strdup\n");
        exit(1);
    }
    strncpy(ret, s, len);
    return ret;
} */

/* --------------------------------------------------------------------------- */
/* file methods */

char *combine(char *resource, char *path){
	size_t rlen = strlen(resource), plen = strlen(path);
	char *file = malloc((rlen + plen) * sizeof(char) + 1);
	file[plen] = 0;
	strncpy(file, path, plen);
	strncat(file, resource, rlen);
	file[rlen + plen] = 0;
	return file;
}

int filesize(char *path, size_t *size){
	struct stat st;

	if(stat(path, &st) != 0){
        *size = 0;
		return FILE_ERROR;
	}
    *size = st.st_size;
	return 0;
}

char *load_file(char *path, int filesize){
	FILE *fp = fopen(path,"rb");

	int toread = filesize;
	char *file = malloc(filesize * sizeof(char) + 1);
	int c = 0;
	size_t nread;
	
	while((nread = fread(&file[c],1,toread,fp)) > 0){
		c = c + (int) nread;
		toread = toread - (int) nread;
	}
	fclose(fp);
	file[filesize] = 0;
	return file;
}

/* --------------------------------------------------------------------------- */
/* heap methods */

void heap_init(heap *heap, node_cmp cmp){
    if(heap == NULL){
        return;
    }
    heap->head = NULL;
    heap->nr_nodes = 0;
    heap->cmp = cmp;
}

heap_node* heap_peek_head(heap *heap){
    return heap->head;
}

static void heap_swap_nodes(heap *heap, heap_node *child, heap_node *parent){
    if(child == NULL || parent == NULL){
        return;
    }
    
    heap_node *sibling, t;

    t = *parent;
    *parent = *child;
    *child = t;

    parent->parent = child;
    if(child->left == child){
        child->left = parent;
        sibling = child->right;
    }else{
        child->right = parent;
        sibling = child->left;
    }

    if(sibling != NULL){
        sibling->parent = child;
    }

    if(parent->left != NULL){
        parent->left->parent = parent;
    }
    if(parent->right != NULL){
        parent->right->parent = parent;
    }

    if(child->parent == NULL){
        heap->head = child;
    }else if(child->parent->left == parent){
        child->parent->left = child;
    }else{
        child->parent->right = child;
    }
}

static void heap_decrease(heap *heap, heap_node *start){
    while(start->parent != NULL && heap->cmp(start, start->parent)){
        heap_swap_nodes(heap, start, start->parent);
    }
}

static void heap_heapify(heap *heap, heap_node *start){
    heap_node *tmp = NULL;
    while(1){
        tmp = start;
        if(start->left != NULL && !heap->cmp(tmp, start->left)){
            tmp = start->left;
        }
        if(start->right != NULL && !heap->cmp(tmp, start->right)){
            tmp = start->right;
        }
        if(tmp == start){
            break;
        }
        heap_swap_nodes(heap, tmp, start);
    }    
}

void heap_insert(heap *heap, heap_node *new){
    if(heap == NULL || new == NULL){
        return;
    }

    new->left = NULL;
    new->right = NULL;
    new->parent = NULL;

    //calculate path through heap
    int path = 0, i, j;
    for(i = 0, j = heap->nr_nodes + 1; j>=2; i += 1, j /= 2){
        path = (path << 1) | (j & 1);
    }

    
    heap_node **child, **parent;
    parent = child = &heap->head;
    for(; i>0; i -= 1){
        parent = child;
        if(path & 1){
            child = &(*child)->right;
        }else{
            child = &(*child)->left;
        }
        path >>= 1;
    }

    new->parent = *parent;
    *child = new;
    heap->nr_nodes += 1;

    heap_decrease(heap, new);
}

void heap_remove(heap *heap, heap_node *node){
    if(heap == NULL || heap->nr_nodes == 0){
        return;
    }

    //calculate path through heap
    int path = 0, i, j;
    for(i = 0, j = heap->nr_nodes; j>=2; i += 1, j /= 2){
        path = (path << 1) | (j & 1);
    }

    heap_node **last = &heap->head;
    for(; i>0; i -= 1){
        if(path & 1){
            last = &(*last)->right;
        }else{
            last = &(*last)->left;
        }
        path >>= 1;
    }

    heap->nr_nodes -= 1;

    heap_node *tmp = *last;
    *last = NULL;

    if(tmp == node){
        if(heap->head == node){
            heap->head = NULL;
        }
        return;
    }

    tmp->left = node->left;
    tmp->right = node->right;
    tmp->parent = node->parent;

    if(tmp->left != NULL){
        tmp->left->parent = tmp;
    }
    if(tmp->right != NULL){
        tmp->right->parent = tmp;
    }

    if(node->parent == NULL){
        heap->head = tmp;
    }else if(node->parent->left == node){
        node->parent->left = tmp;
    }else{
        node->parent->right = tmp;
    }

    heap_heapify(heap, tmp);
    heap_decrease(heap, tmp);
}

heap_node* heap_dequeue_head(heap *heap){
    heap_node *tmp = heap_peek_head(heap);
    heap_remove(heap, tmp);
    return tmp;
}

/* --------------------------------------------------------------------------- */
/* timer methods */

int timer_check(heap_node *n, time_t now){
	timer *t = container_of(n, timer, node);
	return now >= t->end;
}

int timer_cmp(heap_node *n1, heap_node *n2){
	timer *t1 = container_of(n1, timer, node);
	timer *t2 = container_of(n2, timer, node);
	/* if(t1 == NULL){
		return 0;
	}
	if(t2 == NULL){
		return 1;
	} */
    if(t1->end <= t2->end){
        return 1;
    }
    return 0;
}

void timer_init(heap *h, timer *t, time_t sec){
	if(h == NULL || t == NULL || sec < 0){
		return;
	}

	time_t now = time(NULL);
	t->duration = sec;
	t->end = now + sec;
	heap_insert(h, &t->node);
}

void timer_set(heap *h, timer *t, time_t sec){
	if(h == NULL || t == NULL || sec < 0){
		return;
	}

	heap_remove(h, &t->node);
	time_t now = time(NULL);
	t->duration = sec;
	t->end = now + sec;
	heap_insert(h, &t->node);
}

void timer_restart(heap *h, timer *t){
	timer_set(h, t, t->duration);
}

void timer_remove(heap *h, timer *t){
	heap_remove(h, &t->node);
	t->duration = -1;
	t->end = -1;
	t->node = (heap_node) {0};
}

/* --------------------------------------------------------------------------- */
/* hashtable methods */

static unsigned int hash(char *s, int hashsize){
    unsigned int hashval;
    for (hashval = 0; *s != '\0'; s++){
      hashval = *s + 31 * hashval;
    }
    return hashval % hashsize;
}

static int hash_maybe_resize(hash_table *head){
    if(head == NULL){
        return HASH_EINPUT;
    }

    if(head->count != head->hashsize){
        return HASH_OK;
    }

    if(head->hashsize * 2 > HASH_MAX){
        return HASH_EMAX;
    }

    hash_entry **tmp = head->table;
    head->table =  realloc(head->table, 2*(head->hashsize));
    if(head->table == NULL){
        head->table = tmp;
        return HASH_ENOMEM;
    }

    head->hashsize = 2*(head->hashsize);
    memset(&head->table[head->count], 0, head->hashsize - head->count);
    unsigned int hashval;
    for(int i = 0; i<head->count; i++){
        hash_entry *entry = tmp[i];
        hashval = hash(entry->key, head->hashsize);
        head->table[hashval] = entry;
        tmp[i] = NULL;
    }
    return HASH_OK;
}

static hash_entry *hash_get_index(hash_table *head, char *key, unsigned int *index){
	unsigned int hashval = hash(key, head->hashsize);
    hash_entry *entry = head->table[hashval];
    int len = strlen(key);

    if(entry != NULL){
        if(strncmp(entry->key, key, len) == 0){
			*index = hashval;
            return entry;
        }
    }

    for(unsigned int i = (hashval + 1) % head->hashsize; i != hashval; i = (i + 1) % head->hashsize){
        entry = head->table[i];
        if(entry == NULL){
            return NULL;
        }
        if(strncmp(key, entry->key, len) == 0){
			*index = i;
            return entry;
        }
    }
	return NULL;
}

int hash_init(hash_table *head, int size, void *data){
    if(head == NULL){
        return HASH_EINPUT;
    }
    head->count = 0;
    head->hashsize = size;
    head->data = data;
    head->table = calloc(size, sizeof(hash_entry*));
    if(head->table == NULL){
        return HASH_ENOMEM;
    }
    return HASH_OK;
}

int hash_cleanup(hash_table *head){
    if(head == NULL){
        return HASH_EINPUT;
    }
    if(head->table == NULL){
        return HASH_OK;
    }
    for(int i = 0; i<head->hashsize; i++){
        hash_entry *entry = head->table[i];
        if(entry != NULL){
            if(entry->free_flags & HASH_FREE_KEY){
                free(entry->key);
            }
            if(entry->free_flags & HASH_FREE_VAL){
                free(entry->value);
            }
            free(entry);
        }
    }
    free(head->table);
    head->table = NULL;
    return HASH_OK;
}

hash_entry *hash_lookup(hash_table *head, char *key){
	if(head == NULL || key == NULL){
        return NULL;
    }
	unsigned int tmp;
	return hash_get_index(head, key, &tmp);
}

int hash_insert(hash_table *head, char *key, void *value, int free_flags){
    if(head == NULL || key == NULL || value == NULL){
        return HASH_EINPUT;
    }

    hash_entry *entry = hash_lookup(head, key);
    if(entry != NULL){
        if(entry->free_flags & HASH_FREE_VAL){
            free(entry->value);
        }
        if(entry->free_flags & HASH_FREE_KEY){
            free(entry->key);
        }
        entry->key = key;
        entry->value = value;
        entry->free_flags = free_flags;
        return HASH_OK;
    }
    
    int err = hash_maybe_resize(head);
    if(err != HASH_OK){
        return err;
    }
    entry = malloc(sizeof(hash_entry));
    if (entry == NULL){
        return HASH_ENOMEM;
    }
    entry->key = key;
    entry->value = value;
    entry->free_flags = free_flags;
    head->count += 1;
    unsigned int hashval = hash(key, head->hashsize);
    if(head->table[hashval] == NULL) {
        head->table[hashval] = entry;
        return HASH_OK;
    }

    for(unsigned int i = (hashval + 1) % head->hashsize; i != hashval; i = (i + 1) % head->hashsize){
        if(head->table[i] == NULL){
            head->table[i] = entry;
            return HASH_OK;
        }
    }
    return HASH_EMAX;
}

int hash_delete(hash_table *head, char *key){
	if(head == NULL || key == NULL){
		return HASH_EINPUT;
	}

	unsigned int index = 0;
	if(hash_get_index(head, key, &index) != NULL){
		hash_entry *e = head->table[index];
		if(e->free_flags & HASH_FREE_KEY){
			free(e->key);
		}

		if(e->free_flags & HASH_FREE_VAL){
			free(e->value);
		}

		free(e);
		head->table[index] = NULL;
		head->count -= 1;
	}
	return HASH_OK;
}
