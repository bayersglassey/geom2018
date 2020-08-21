#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_


#define DIRECTORY_LIST_ENTRIES 64

#define DIRECTORY_PATH_NEXT(PATH) while(*(PATH))(PATH)++; (PATH)++;


struct directory_entry_class;
struct directory_entry;
struct directory_list;


typedef struct directory_entry_class {
    void (*list)(void *self, struct directory_list *list, int page);
} directory_entry_class_t;

typedef struct directory_entry {
    directory_entry_class_t *class;
    const char *name;
    void *self;
} directory_entry_t;

typedef struct directory_list {
    int entries_len;
    directory_entry_t entries[DIRECTORY_LIST_ENTRIES];
} directory_list_t;


int directory_name_match(
    const char *name, const char *name2);
int directory_name_match_glob(
    const char *name, const char *glob);
directory_entry_t *directory_list_find_name(
    directory_list_t *list, const char *name);
directory_entry_t *directory_entry_find_path(
    directory_entry_t *root, const char *path, int path_len);


#endif