#ifndef _STRINGSTORE_H_
#define _STRINGSTORE_H_

#include "array.h"

typedef struct stringstore_entry {
    char *data;
} stringstore_entry_t;

typedef struct stringstore {
    ARRAY_DECL(stringstore_entry_t*, entries)
} stringstore_t;

void stringstore_entry_cleanup(stringstore_entry_t *entry);
void stringstore_cleanup(stringstore_t *store);
void stringstore_dump(stringstore_t *store);
void stringstore_init(stringstore_t *store);
int stringstore_add(stringstore_t *store, const char *data,
    stringstore_entry_t **entry_ptr);
const char *stringstore_get(stringstore_t *store, const char *data);

#endif