#ifndef _ARRAY_H_
#define _ARRAY_H_

#define ARRAY_DECL(T, array) \
    T *array; \
    int array##_len; \
    int array##_size;

#define ARRAY_INIT(array) \
    array = NULL; \
    array##_len = 0; \
    array##_size = 0;

#define ARRAY_COPY(array, array2) \
    array = array2; \
    array##_len = array2##_len; \
    array##_size = array##_size;

#define ARRAY_GROW(T, array) \
{ \
    int new_size; \
    T *new_array; \
    if(array##_size == 0){ \
        new_size = 8; \
        new_array = malloc(sizeof(*new_array) * new_size); \
    }else{ \
        new_size = array##_size * 2; \
        new_array = realloc(array, \
            sizeof(*new_array) * new_size); \
    } \
    if(new_array == NULL)return 1; \
    for(int i = array##_size; i < new_size; i++){ \
        new_array[i] = 0;} \
    array = new_array; \
    array##_size = new_size; \
}

#define ARRAY_PUSH_MANY(T, array, new_elem, _n) \
{ \
    int n = _n; \
    while(array##_len + n - 1 >= array##_size) \
        ARRAY_GROW(T, array) \
    int old_len = array##_len; \
    int new_len = old_len + n; \
    array##_len = new_len; \
    for(int i = old_len; i < new_len; i++){ \
        array[i] = (new_elem);} \
}

#define ARRAY_PUSH(T, array, new_elem) \
{ \
    if(array##_len >= array##_size) \
        ARRAY_GROW(T, array) \
    array##_len++; \
    array[array##_len - 1] = (new_elem); \
}

#define ARRAY_PUSH_NEW(T, array, new_elem) \
T new_elem = NULL; \
{ \
    if(array##_len >= array##_size) \
        ARRAY_GROW(T, array) \
    new_elem = calloc(1, sizeof(*new_elem)); \
    if(new_elem == NULL)return 1; \
    array##_len++; \
    array[array##_len - 1] = new_elem; \
}

#define ARRAY_FREE(T, array, elem_cleanup) \
{ \
    for(int i = 0; i < array##_len; i++){ \
        elem_cleanup(array[i]);} \
    free(array); \
    array##_len = 0; \
    array##_size = 0; \
}

#define ARRAY_FREE_PTR(T, array, elem_cleanup) \
{ \
    for(int i = 0; i < array##_len; i++){ \
        elem_cleanup(array[i]); \
        free(array[i]);} \
    free(array); \
    array##_len = 0; \
    array##_size = 0; \
}

#endif