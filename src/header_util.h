#ifndef HEADER_UTIL
#define HEADER_UTIL

#include <stddef.h>

#include "util.h"

typedef enum mime_type{
    ico = 0,
    jpeg = 1,
    png = 2,
    gif = 3,
    plain = 4,
    html = 5,
    css = 6,
    js = 7,
    mp4 = 8,
}mime_type;

const char *http_code_to_str(int code);
mime_type res_to_mime(char *resource);
int header_set_date(hash_table *ht);
void header_set_content_type(hash_table *ht, mime_type type);
int header_set_content_length(hash_table *ht, size_t length);
void header_set_general(hash_table *ht, char *sname, int keep_alive, int max_nr_req);

int header_len(hash_table *ht, const char* res_code);
int header_build(hash_table *ht, const char* res_code, char *buf, int size);

hash_table header_create(int size);
void header_cleanup(hash_table *ht);
int header_lookup(hash_table *ht, char *key, const char **result);
int header_is_set(hash_table *ht, char *key, char *val);
int header_insert(hash_table *ht, char *key, char *val);
int header_insert_mult(hash_table *ht, char *array[][2], size_t size);

#endif