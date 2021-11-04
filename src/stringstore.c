
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "array.h"
#include "stringstore.h"

void stringstore_entry_cleanup(stringstore_entry_t *entry){
    free(entry->data);
}

void stringstore_cleanup(stringstore_t *store){
    ARRAY_FREE_PTR(entry_t*, store->entries, stringstore_entry_cleanup)
}

void stringstore_dump(stringstore_t *store){
    fprintf(stderr, "STRING STORE (%p) (%i ENTRIES):\n", store,
        store->entries_len);
    for(int i = 0; i < store->entries_len; i++){
        stringstore_entry_t *entry = store->entries[i];
        fprintf(stderr, "  ENTRY %i (%p): (%p) %s\n",
            i, entry, entry->data, entry->data);
    }
}

void stringstore_init(stringstore_t *store){
    ARRAY_INIT(store->entries)
}

int stringstore_add(stringstore_t *store, const char *data,
    stringstore_entry_t **entry_ptr
){
    ARRAY_PUSH_NEW(stringstore_entry_t*, store->entries, entry)
    size_t len = strlen(data);
    char *entry_data = malloc(len + 1);
    if(!entry_data)return 1;
    strcpy(entry_data, data);
    entry->data = entry_data;
    *entry_ptr = entry;
    return 0;
}

const char *stringstore_get(stringstore_t *store, const char *data){
    if(!data)return NULL;
    for(int i = 0; i < store->entries_len; i++){
        stringstore_entry_t *entry = store->entries[i];
        if(entry->data == data || !strcmp(entry->data, data)){
            return entry->data;
        }
    }
    stringstore_entry_t *entry;
    int err = stringstore_add(store, data, &entry);
    if(err)return NULL;
    return entry->data;
}
