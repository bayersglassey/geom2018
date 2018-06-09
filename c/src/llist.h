#ifndef _LLIST_H_
#define _LLIST_H_

#define LLIST_DECL(T, list) \
    T *list; \
    T **list##_last;

#define LLIST_INIT(parent, list) \
    (parent).list = NULL; \
    (parent).list##_last = &(parent).list;

#define LLIST_PUSH(T, parent, list, new_node) \
    T *new_node = calloc(1, sizeof(*new_node)); \
    if(new_node == NULL)return 1; \
    *(parent).list##_last = new_node; \
    (parent).list##_last = &new_node->next;

#define LLIST_FREE(T, parent, list, node_cleanup) { \
    T *node = (parent).list; \
    while(node != NULL){ \
        T *next = node->next; \
        node_cleanup(node); \
        free(node); \
        node = next; \
    } \
    (parent).list = NULL; \
    (parent).list##_last = &(parent).list; \
}

#endif