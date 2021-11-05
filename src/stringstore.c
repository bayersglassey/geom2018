
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

void stringstore_dump(stringstore_t *store, FILE *f){
    fprintf(f, "STRING STORE (%p) (%i ENTRIES):\n", store,
        store->entries_len);
    for(int i = 0; i < store->entries_len; i++){
        stringstore_entry_t *entry = store->entries[i];
        fprintf(f, "  ENTRY %i: %s\n",
            i, entry->data);
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

int stringstore_add_donate(stringstore_t *store, char *data,
    stringstore_entry_t **entry_ptr
){
    /* Caller "donates" (passes ownership of) data to store */
    ARRAY_PUSH_NEW(stringstore_entry_t*, store->entries, entry)
    entry->data = data;
    *entry_ptr = entry;
    return 0;
}

const char *stringstore_find(stringstore_t *store, const char *data){
    if(!data)return NULL;
    for(int i = 0; i < store->entries_len; i++){
        stringstore_entry_t *entry = store->entries[i];
        if(entry->data == data || !strcmp(entry->data, data)){
            return entry->data;
        }
    }
    return NULL;
}

const char *stringstore_get(stringstore_t *store, const char *data){
    if(!data)return NULL;

    const char *found_data = stringstore_find(store, data);
    if(found_data)return found_data;

    stringstore_entry_t *entry;
    int err = stringstore_add(store, data, &entry);
    if(err)return NULL;
    return entry->data;
}

const char *stringstore_get_donate(stringstore_t *store, char *data){
    /* Caller "donates" (passes ownership of) data to store */
    if(!data)return NULL;

    const char *found_data = stringstore_find(store, data);
    if(found_data){

        /* We don't need data, since we already had a copy of this string
        in the store.
        But caller passed us ownership of data, so we must free it if we're
        not going to store it. */
        free(data);

        return found_data;
    }

    stringstore_entry_t *entry;
    int err = stringstore_add_donate(store, data, &entry);
    if(err)return NULL;
    return entry->data;
}
