

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexcollmap.h"
#include "lexer.h"
#include "util.h"
#include "geom.h"
#include "vec4.h"
#include "prismelrenderer.h"


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




/**************
 * HEXCOLLMAP *
 **************/

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

int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer){
    int err;

    /* set up dynamic array of lines */
    int collmap_lines_len = 0;
    int collmap_lines_size = 8;
    char **collmap_lines = calloc(collmap_lines_size,
        sizeof(*collmap_lines));
    if(collmap_lines == NULL)return 1;

    /* read in lines */
    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")"))break;

        /* resize array of lines, if necessary */
        if(collmap_lines_len >= collmap_lines_size){
            int new_lines_size = collmap_lines_size * 2;
            char **new_lines = realloc(collmap_lines,
                sizeof(*collmap_lines) * new_lines_size);
            if(new_lines == NULL)return 1;
            for(int i = collmap_lines_size;
                i < new_lines_size; i++){
                    new_lines[i] = NULL;}
            collmap_lines_size = new_lines_size;
            collmap_lines = new_lines;
        }

        /* get new line from lexer */
        collmap_lines_len++;
        err = fus_lexer_get_str(lexer,
            &collmap_lines[collmap_lines_len - 1]);
        if(err)return err;
    }

    /* parse lines */
    err = hexcollmap_parse_lines(collmap,
        collmap_lines, collmap_lines_len);
    if(err)return err;

    /* free lines and dynamic array thereof */
    for(int i = 0; i < collmap_lines_len; i++){
        free(collmap_lines[i]);
        collmap_lines[i] = NULL;}
    free(collmap_lines);

    return 0;
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

int add_tile_rgraph(rendergraph_t *rgraph, rendergraph_t *rgraph2,
    vecspace_t *space, vec_t add, rot_t rot
){
    int err;
    rendergraph_trf_t *rendergraph_trf;
    err = rendergraph_push_rendergraph_trf(rgraph, &rendergraph_trf);
    if(err)return err;
    rendergraph_trf->rendergraph = rgraph2;
    rendergraph_trf->trf.rot = rot;
    vec_cpy(space->dims, rendergraph_trf->trf.add, add);
    return 0;
}

int hexcollmap_create_rgraph(hexcollmap_t *collmap,
    prismelrenderer_t *prend,
    rendergraph_t *rgraph_vert,
    rendergraph_t *rgraph_edge,
    rendergraph_t *rgraph_face,
    vecspace_t *space, vec_t mul
){
    int err;

    ARRAY_PUSH_NEW(rendergraph_t, *prend, rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(collmap->name), space,
        rendergraph_animation_type_default,
        rendergraph_n_frames_default);
    if(err)return err;

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            int px = x - collmap->ox;
            int py = y - collmap->oy;

            vec_t v;
            vec4_set(v, px + py, 0, -py, 0);
            vec_mul(prend->space, v, mul);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            for(int i = 0; i < 1; i++){
                if(tile->vert[i]){
                    err = add_tile_rgraph(rgraph, rgraph_vert,
                        prend->space, v, i * 2);
                    if(err)return err;}}
            for(int i = 0; i < 3; i++){
                if(tile->edge[i]){
                    err = add_tile_rgraph(rgraph, rgraph_edge,
                        prend->space, v, i * 2);
                    if(err)return err;}}
            for(int i = 0; i < 2; i++){
                if(tile->face[i]){
                    err = add_tile_rgraph(rgraph, rgraph_face,
                        prend->space, v, i * 2);
                    if(err)return err;}}
        }
    }

    return 0;
}




/*****************
 * HEXCOLLMAPSET *
 *****************/

void hexcollmapset_cleanup(hexcollmapset_t *collmapset){
    ARRAY_FREE(hexcollmap_t, *collmapset, collmaps, hexcollmap_cleanup)
}

int hexcollmapset_init(hexcollmapset_t *collmapset){
    ARRAY_INIT(*collmapset, collmaps)
    return 0;
}

void hexcollmapset_dump(hexcollmapset_t *collmapset, FILE *f){
    fprintf(f, "hexcollmapset: %p\n", collmapset);
    for(int i = 0; i < collmapset->collmaps_len; i++){
        hexcollmap_dump(collmapset->collmaps[i], f, 2);
    }
}

int hexcollmapset_load(hexcollmapset_t *collmapset, const char *filename){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexcollmapset_init(collmapset);
    if(err)return err;

    while(1){
        err = fus_lexer_next(&lexer);
        if(err)return err;

        if(fus_lexer_done(&lexer))break;

        char *name;
        err = fus_lexer_get_name(&lexer, &name);
        if(err)return err;

        ARRAY_PUSH_NEW(hexcollmap_t, *collmapset, collmaps, collmap)
        err = hexcollmap_init(collmap, name);
        if(err)return err;

        err = fus_lexer_expect(&lexer, "(");
        if(err)return err;
        err = hexcollmap_parse(collmap, &lexer);
        if(err)return err;
    }

    free(text);
    return 0;
}

hexcollmap_t *hexcollmapset_get_collmap(hexcollmapset_t *collmapset,
    const char *name
){
    for(int i = 0; i < collmapset->collmaps_len; i++){
        hexcollmap_t *collmap = collmapset->collmaps[i];
        if(!strcmp(collmap->name, name))return collmap;
    }
    return NULL;
}

