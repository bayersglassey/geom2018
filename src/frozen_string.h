#ifndef _CONST_STRING_H_
#define _CONST_STRING_H_

typedef struct frozen_string {
    const char *data;
    char *managed_data;
    int refcount;
} frozen_string_t;

void frozen_string_init(frozen_string_t *z, const char *data);
void frozen_string_init_managed(frozen_string_t *z, char *managed_data);
frozen_string_t *frozen_string_create(const char *data);
frozen_string_t *frozen_string_create_managed(char *managed_data);
void frozen_string_cleanup(frozen_string_t *z);
void frozen_string_free(frozen_string_t *z);
frozen_string_t *frozen_string_dup(frozen_string_t *z);

#endif