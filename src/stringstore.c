
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "array.h"
#include "stringstore.h"


bool global_stringstore_initialized = false;
stringstore_t global_stringstore = {0};


void stringstore_entry_cleanup(stringstore_entry_t *entry){
    free(entry->data);
}

void stringstore_cleanup(stringstore_t *store){
    ARRAY_FREE_PTR(stringstore_entry_t*, store->entries, stringstore_entry_cleanup)
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

static int _stringstore_add(stringstore_t *store, const char *data,
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

static int _stringstore_add_donate(stringstore_t *store, char *data,
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

const char *stringstore_findn(stringstore_t *store, const char *data, int data_len){
    if(!data)return NULL;
    for(int i = 0; i < store->entries_len; i++){
        stringstore_entry_t *entry = store->entries[i];
        if(strlen(entry->data) == data_len && !strncmp(entry->data, data, data_len)){
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
    int err = _stringstore_add(store, data, &entry);
    if(err)return NULL;
    return entry->data;
}

const char *stringstore_getn(stringstore_t *store, const char *data, int data_len){
    if(!data)return NULL;

    const char *found_data = stringstore_findn(store, data, data_len);
    if(found_data)return found_data;

    /* We didn't find the string, so we need to add it; but data isn't
    NUL-terminated, so we need to make a NUL-terminated copy of it. */
    char *new_data = malloc(sizeof(*new_data) * (data_len + 1));
    strncpy(new_data, data, data_len);
    new_data[data_len] = '\0';

    stringstore_entry_t *entry;
    int err = _stringstore_add_donate(store, new_data, &entry);
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
    int err = _stringstore_add_donate(store, data, &entry);
    if(err)return NULL;
    return entry->data;
}

static void cleanup_global_stringstore(void){
    stringstore_cleanup(&global_stringstore);
}

stringstore_t *get_global_stringstore(void){
    if(!global_stringstore_initialized){
        stringstore_init(&global_stringstore);
        atexit(&cleanup_global_stringstore);
        global_stringstore_initialized = true;
    }
    return &global_stringstore;
}
