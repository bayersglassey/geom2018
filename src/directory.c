
#include <limits.h>
#include <stddef.h>

#include "directory.h"



int directory_name_match(
    const char *name, const char *name2
){
    while(*name && *name == *name2){
        name++;
        name2++;
    }
    return *name == *name2;
}

int directory_name_match_glob(
    const char *name, const char *glob
){
    while(1){
        char glob_c = *(glob++);
        if(glob_c == '?'){
            /* '?' matches any (non-NUL) character */
            char name_c = *(name++);
            if(!name_c)return 0;
        }else if(glob_c == '*'){
            /* '*' matches 0 or more characters, non-greedily */

            /* Find next non-wildcard glob character (possibly '\0') */
            const char *glob_peek = glob;
            char glob_peek_c;
            while(1){
                glob_peek_c = *(glob_peek++);
                if(glob_peek_c == '*' || glob_peek_c == '?'){
                    continue;
                }else{
                    if(glob_peek_c == '\\'){
                        glob_peek_c = *(glob_peek++);
                    }
                    break;
                }
            }

            /* Match until name_c == glob_peek_c */
            while(1){
                char name_c = *(name++);

                /* End of wildcard */
                if(name_c == glob_peek_c)break;

                /* If we hit end of name without matching glob_peek_c,
                then no match! */
                if(!name_c)return 0;
            }
        }else{
            if(glob_c == '\\'){
                glob_c = *(glob++);
            }

            /* Basic character match (handles '\0' as well) */
            char name_c = *(name++);
            if(name_c != glob_c)return 0;

            /* We reached the end of the glob... so name matches! */
            if(!glob_c)return 1;
        }
    }
}

void directory_parse_path(
    const char *raw, char *path, int *path_parts_len_ptr
){
    int path_parts_len = 1;
    while(1){
        char raw_c = *(raw++);
        if(raw_c == '/'){
            *(path++) = '\0';
            path_parts_len++;
        }else{
            if(raw_c == '\\'){
                raw_c = *(raw++);
            }

            *(path++) = raw_c;
            if(!raw_c)break;
        }
    }

    *path_parts_len_ptr = path_parts_len;
}

directory_entry_t *directory_list_find_name(
    directory_list_t *list, const char *name
){
    for(int i = 0; i < list->entries_len; i++){
        directory_entry_t *entry = &list->entries[i];
        if(directory_name_match(entry->name, name))return entry;
    }
    return NULL;
}

directory_entry_t *directory_entry_find_path(
    directory_entry_t *root, const char *path, int path_parts_len
){
    directory_list_t list;

    while(1){
        /* If path indicates root, return it */
        if(path_parts_len == 0)return root;

        /* If root isn't a directory, no match */
        if(!root->class->list)return NULL;

        for(int page = 0; page <= INT_MAX; page++){
            root->class->list(root, &list, page);
            directory_entry_t *entry = directory_list_find_name(
                &list, path);
            if(entry){
                /* Match! Recurse by advancing path and searching from
                the entry we found */
                root = entry;
                DIRECTORY_PATH_NEXT(path)
                path_parts_len--;

                /* Break out of page loop */
                break;
            }

            /* If this was the last page of entries, no match was found */
            if(list.entries_len < DIRECTORY_LIST_ENTRIES)return NULL;
        }
    }
}

