#ifndef _ARRAY_H_
#define _ARRAY_H_

#define ARRAY_DECL(T, array) \
    T **array; \
    int array##_len; \
    int array##_size;

#define ARRAY_INIT(parent, array) \
    (parent).array = NULL; \
    (parent).array##_len = 0; \
    (parent).array##_size = 0;

#define ARRAY_GROW(T, parent, array) \
{ \
    int new_size; \
    T **new_array; \
    if((parent).array##_size == 0){ \
        new_size = 8; \
        new_array = malloc(sizeof(*new_array) * new_size); \
    }else{ \
        new_size = (parent).array##_size * 2; \
        new_array = realloc((parent).array, \
            sizeof(*new_array) * new_size); \
    } \
    if(new_array == NULL)return 1; \
    for(int i = (parent).array##_size; i < new_size; i++){ \
        new_array[i] = NULL;} \
    (parent).array = new_array; \
    (parent).array##_size = new_size; \
}

#define ARRAY_PUSH(T, parent, array, new_elem) \
{ \
    if((parent).array##_len >= (parent).array##_size) \
        ARRAY_GROW(T, parent, array) \
    (parent).array##_len++; \
    (parent).array[(parent).array##_len - 1] = (new_elem); \
}

#define ARRAY_PUSH_NEW(T, parent, array, new_elem) \
T *new_elem = NULL; \
{ \
    if((parent).array##_len >= (parent).array##_size) \
        ARRAY_GROW(T, parent, array) \
    new_elem = calloc(1, sizeof(*new_elem)); \
    if(new_elem == NULL)return 1; \
    (parent).array##_len++; \
    (parent).array[(parent).array##_len - 1] = new_elem; \
}

#define ARRAY_FREE(T, parent, array, elem_cleanup) \
{ \
    for(int i = 0; i < (parent).array##_len; i++){ \
        elem_cleanup((parent).array[i]); \
        free((parent).array[i]);} \
    free((parent).array); \
    (parent).array##_len = 0; \
    (parent).array##_size = 0; \
}

#endif