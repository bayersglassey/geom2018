
#include <stdlib.h>

#include "frozen_string.h"

static void _frozen_string_init(frozen_string_t *z,
    const char *data, char *managed_data
){
    z->data = data;
    z->managed_data = managed_data;
    z->refcount = 1;
}

void frozen_string_init(frozen_string_t *z, const char *data){
    _frozen_string_init(z, data, NULL);
}

void frozen_string_init_managed(frozen_string_t *z, char *managed_data){
    _frozen_string_init(z, managed_data, managed_data);
}

frozen_string_t *_frozen_string_create(
    const char *data, char *managed_data
){
    frozen_string_t *z = malloc(sizeof(*z));
    if(!z)return NULL;
    _frozen_string_init(z, data, managed_data);
    return z;
}

frozen_string_t *frozen_string_create(const char *data){
    return _frozen_string_create(data, NULL);
}

frozen_string_t *frozen_string_create_managed(char *managed_data){
    return _frozen_string_create(managed_data, managed_data);
}

void frozen_string_cleanup(frozen_string_t *z){
    z->refcount--;
    if(z->refcount <= 0){
        free(z->managed_data);
    }
}

void frozen_string_free(frozen_string_t *z){
    if(!z)return;
    frozen_string_cleanup(z);
    if(z->refcount <= 0)free(z);
}

frozen_string_t *frozen_string_dup(frozen_string_t *z){
    z->refcount++;
    return z;
}
