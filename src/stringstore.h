#ifndef _STRINGSTORE_H_
#define _STRINGSTORE_H_

#include <stdio.h>

#include "array.h"

typedef struct stringstore_entry {
    char *data;
} stringstore_entry_t;

typedef struct stringstore {
    ARRAY_DECL(stringstore_entry_t*, entries)
} stringstore_t;

void stringstore_entry_cleanup(stringstore_entry_t *entry);
void stringstore_cleanup(stringstore_t *store);
void stringstore_dump(stringstore_t *store, FILE *f);
void stringstore_init(stringstore_t *store);
const char *stringstore_find(stringstore_t *store, const char *data);
const char *stringstore_findn(stringstore_t *store, const char *data, int data_len);
const char *stringstore_get(stringstore_t *store, const char *data);
const char *stringstore_getn(stringstore_t *store, const char *data, int data_len);
const char *stringstore_get_donate(stringstore_t *store, char *data);
stringstore_t *get_global_stringstore(void);

#endif
