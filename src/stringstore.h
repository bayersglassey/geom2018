#ifndef _STRINGSTORE_H_
#define _STRINGSTORE_H_

#include <string.h>
#include <stdlib.h>

#include "array.h"

typedef struct stringstore_entry {
    char *data;
} stringstore_entry_t;

typedef struct stringstore {
    ARRAY_DECL(stringstore_entry_t*, entries)
} stringstore_t;

void stringstore_entry_cleanup(stringstore_entry_t *entry){
    free(entry->data);
}

void stringstore_cleanup(stringstore_t *store){
    ARRAY_FREE_PTR(entry_t*, store->entries, stringstore_entry_cleanup)
}

void stringstore_init(stringstore_t *store){
    ARRAY_INIT(store->entries)
}

char *stringstore_add(stringstore_t *store, const char *data){
    ARRAY_PUSH_NEW(stringstore_entry_t*, store->entries, entry)
    size_t len = strlen(data);
    char *entry_data = malloc(len + 1);
    if(!entry_data)return NULL;
    strcpy(entry_data, data);
    return entry_data;
}

char *stringstore_get(stringstore_t *store, const char *data){
    if(!data)return NULL;
    for(int i = 0; i < store->entries_len; i++){
        stringstore_entry_t *entry = store->entries[i];
        if(!strcmp(entry->data, data))return data;
    }
    return stringstore_add(store, data);
}

#endif