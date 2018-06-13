

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexcollmap.h"
#include "util.h"


/* What a tile looks like in the hexcollmap text format: */
//   "  + - +    "
//   "   \*/*\   "
//   "   (+)- +  "
/* ...where ( ) indicates the origin (x=0, y=0) */


static int _min(int x, int y){
    return x < y? x: y;
}

static int _max(int x, int y){
    return x > y? x: y;
}

static int _div(int x, int y){
    /* y is assumed to be non-negative */
    if(x < 0)return (x - (y-1)) / y;
    return x / y;
}

static int _rem(int x, int y){
    /* y is assumed to be non-negative */
    if(x < 0)return (y - 1) + (x - (y-1)) % y;
    return x % y;
}


static void get_map_coords(int x, int y, char c,
    int *mx_ptr, int *my_ptr, bool *is_face1_ptr
){
    bool is_face1 = false;

    /* Step 1: find x, y of vertex */
    if(c == '+'){
    }else if(c == '-'){
        x -= 1;
    }else if(c == '/'){
        x -= 1;
        y += 1;
    }else if(c == '\\'){
        x += 1;
        y += 1;
    }else if(c == '*'){
        /* assume we're the right-hand triangle */
        x -= 2;
        y += 1;
        if(_rem(x + y, 4) != 0){
            /* oh, actually we were the left-hand triangle */
            x += 2;
            is_face1 = true;
        }
    }

    /* Step 2: apply the formula for a vertex */
    *mx_ptr = _div(x - y, 4);
    *my_ptr = _div(y, 2);
    if(is_face1_ptr != NULL)*is_face1_ptr = is_face1;
}



void hexcollmap_cleanup(hexcollmap_t *collmap){
    free(collmap->name);
    free(collmap->tiles);
}

int hexcollmap_init(hexcollmap_t *collmap, char *name){
    collmap->name = name;
    return 0;
}

void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces){
    char spaces[20];
    get_spaces(spaces, 20, n_spaces);

    fprintf(f, "%shexcollmap: %p\n", spaces, collmap);
    if(collmap == NULL)return;
    if(collmap->name != NULL){
        fprintf(f, "%s  name: %s\n", spaces, collmap->name);}
    fprintf(f, "%s  origin: %i %i\n", spaces, collmap->ox, collmap->oy);
    fprintf(f, "%s  tiles:\n", spaces);
    for(int y = 0; y < collmap->h; y++){
        fprintf(f, "%s    ", spaces);
        for(int x = 0; x < collmap->w; x++){
            hexcollmap_tile_t *tile = &collmap->tiles[y * collmap->w + x];
            fprintf(f, "[%i][%i%i%i][%i%i] ",
                tile->vert[0],
                tile->edge[0], tile->edge[1], tile->edge[2],
                tile->face[0], tile->face[1]);
        }
        fprintf(f, "\n");
    }
}

int hexcollmap_parse_lines(hexcollmap_t *collmap,
    char **lines, int lines_len
){
    /* We iterate through the lines 3 times!.. can that be improved?.. */
    int err;

    /* Iteration 1: Find origin */
    int ox = -1;
    int oy = -1;
    for(int y = 0; y < lines_len; y++){
        char *line = lines[y];
        int line_len = strlen(line);
        for(int x = 0; x < line_len; x++){
            char c = line[x];
            if(c == '('){
                if(x+2 >= line_len || line[x+2] != ')'){
                    fprintf(stderr, "Line %i, char %i: '(' without "
                        "matching ')'. Line: %s\n",
                        y, x, line);
                    return 2;
                }
                if(oy != -1){
                    fprintf(stderr, "Line %i, char %i: another '('."
                        " Line: %s\n", y, x, line);
                    return 2;
                }
                ox = x + 1;
                oy = y;
                x += 2;
            }else if(strchr(" .+/-\\*", c) != NULL){
                /* these are all fine */
            }else{
                fprintf(stderr, "Line %i, char %i: unexpected character."
                    " Line: %s\n", y, x, line);
                return 2;
            }
        }
    }

    /* Iteration 2: Find map bounds */
    int map_t = 0;
    int map_b = 0;
    int map_l = 0;
    int map_r = 0;
    for(int y = 0; y < lines_len; y++){
        char *line = lines[y];
        int line_len = strlen(line);
        for(int x = 0; x < line_len; x++){
            char c = line[x];
            if(strchr("+/-\\*", c) != NULL){
                int mx, my; bool is_face1;
                get_map_coords(x-ox, y-oy, c,
                    &mx, &my, &is_face1);
                map_t = _min(map_t, my);
                map_b = _max(map_b, my);
                map_l = _min(map_l, mx);
                map_r = _max(map_r, mx);
            }
        }
    }

    /* Intermission: Allocate map data */
    int map_w = map_r - map_l + 1;
    int map_h = map_b - map_t + 1;
    hexcollmap_tile_t *tiles = calloc(map_w * map_h, sizeof(*tiles));
    if(tiles == NULL)return 1;

    /* Iteration 3: The meat of it all */
    for(int y = 0; y < lines_len; y++){
        char *line = lines[y];
        int line_len = strlen(line);
        for(int x = 0; x < line_len; x++){
            char c = line[x];
            if(strchr("+/-\\*", c) != NULL){
                int mx, my; bool is_face1;
                get_map_coords(x-ox, y-oy, c,
                    &mx, &my, &is_face1);
                mx -= map_l;
                my -= map_t;
                hexcollmap_tile_t *tile = &tiles[my * map_w + mx];
                if(c == '+'){
                    tile->vert[0] = true;
                }else if(c == '-'){
                    tile->edge[0] = true;
                }else if(c == '/'){
                    tile->edge[1] = true;
                }else if(c == '\\'){
                    tile->edge[2] = true;
                }else if(c == '*'){
                    tile->face[is_face1? 1: 0] = true;
                }
            }
        }
    }

    /* OKAY */
    collmap->ox = -map_l;
    collmap->oy = -map_t;
    collmap->w = map_w;
    collmap->h = map_h;
    collmap->tiles = tiles;
    return 0;
}

