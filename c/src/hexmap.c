

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexmap.h"
#include "lexer.h"
#include "util.h"
#include "geom.h"
#include "vec4.h"
#include "hexspace.h"
#include "prismelrenderer.h"


#define DEBUG_COLLIDE false



/**************
 * MATH UTILS *
 **************/

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



/**************
 * HEXCOLLMAP *
 **************/

void hexcollmap_cleanup(hexcollmap_t *collmap){
    free(collmap->tiles);
}

int hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space){
    memset(collmap, 0, sizeof(*collmap));
    collmap->space = space;
    return 0;
}

void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces){
    char spaces[20];
    get_spaces(spaces, 20, n_spaces);

    fprintf(f, "%shexcollmap: %p\n", spaces, collmap);
    if(collmap == NULL)return;
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

static void get_map_coords(int x, int y, char c,
    int *mx_ptr, int *my_ptr, bool *is_face1_ptr
){
    bool is_face1 = false;

    /* What a tile looks like in the hexcollmap text format: */
    //   "  + - +    "
    //   "   \*/*\   "
    //   "   (+)- +  "
    /* ...where ( ) indicates the origin (x=0, y=0) */

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

static int hexcollmap_parse_lines(hexcollmap_t *collmap,
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

void hexcollmap_normalize_vert(trf_t *index){
    index->rot = 0;
    index->flip = false;
}

void hexcollmap_normalize_edge(trf_t *index){
    index->flip = false;
    if(index->rot == 3){
        index->rot = 0; index->add[0]--;
    }else if(index->rot == 4){
        index->rot = 1; index->add[0]--; index->add[1]++;
    }else if(index->rot == 5){
        index->rot = 2; index->add[1]++;
    }
}

void hexcollmap_normalize_face(trf_t *index){
    if(index->flip){
        index->flip = false;
        index->rot = rot_contain(6, index->rot - 1);
    }
    if(index->rot == 2){
        index->rot = 0; index->add[0]--;
    }else if(index->rot == 3){
        index->rot = 1; index->add[0]--; index->add[1]++;
    }else if(index->rot == 4){
        index->rot = 0; index->add[0]--; index->add[1]++;
    }else if(index->rot == 5){
        index->rot = 1; index->add[1]++;
    }
}

bool hexcollmap_collide(hexcollmap_t *map1, hexcollmap_t *map2,
    trf_t *trf, bool all
){
    int ox1 = map1->ox;
    int oy1 = map1->oy;
    int w1 = map1->w;
    int h1 = map1->h;
    int ox2 = map2->ox;
    int oy2 = map2->oy;
    int w2 = map2->w;
    int h2 = map2->h;
    vecspace_t *space = map1->space;
    if(DEBUG_COLLIDE)printf("COLLIDE: "
        "map1=%p[%i,%i,%i,%i] map2=%p[%i,%i,%i,%i] all=%c\n",
        map1, ox1, oy1, w1, h1, map2, ox2, oy2, w2, h2, all? 'y': 'n');
    if(DEBUG_COLLIDE)printf("  trf = (%i %i) %i %c\n",
        trf->add[0], trf->add[1],
        trf->rot, trf->flip? 'y': 'n');

    /* NOTE: for tile coords (ox, oy, x, y, w, h),
    Y is reversed (down is positive, up is negative) */

    for(int y2 = 0; y2 < h2; y2++){
        for(int x2 = 0; x2 < w2; x2++){
            hexcollmap_tile_t *tile2 = &map2->tiles[y2 * w2 + x2];
            if(DEBUG_COLLIDE)printf("    (%i, %i):\n", x2, y2);
            #define COLLIDE(PART, ROT) \
                for(int r2 = 0; r2 < ROT; r2++){ \
                    if(!tile2->PART[r2])continue; \
                    if(DEBUG_COLLIDE){ \
                        printf("      %s %i:\n", #PART, r2);} \
                    \
                    trf_t index; \
                    hexspace_set(index.add, x2 - ox2, y2 - oy2); \
                    index.rot = r2; \
                    index.flip = false; \
                    if(DEBUG_COLLIDE){ \
                        printf("        index = (%i %i) %i %c\n", \
                            index.add[0], index.add[1], \
                            index.rot, index.flip? 'y': 'n');} \
                    \
                    /* And now, because we were fools and defined */ \
                    /* the tile coords such that their Y is flipped */ \
                    /* compared to vecspaces, we need to flip that Y */ \
                    /* before calling trf_apply and then flip it back */ \
                    /* again: */ \
                    index.add[1] = -index.add[1]; \
                    trf_apply(space, &index, trf); \
                    index.add[1] = -index.add[1]; \
                    \
                    if(DEBUG_COLLIDE){ \
                        printf("        index = (%i %i) %i %c\n", \
                            index.add[0], index.add[1], \
                            index.rot, index.flip? 'y': 'n');} \
                    hexcollmap_normalize_##PART(&index); \
                    if(DEBUG_COLLIDE){ \
                        printf("        index = (%i %i) %i %c\n", \
                            index.add[0], index.add[1], \
                            index.rot, index.flip? 'y': 'n');} \
                    \
                    int x1 = ox1 + index.add[0]; \
                    int y1 = oy1 + index.add[1]; \
                    int r1 = index.rot; \
                    if(DEBUG_COLLIDE)printf("        x1 = %i\n", x1); \
                    if(DEBUG_COLLIDE)printf("        y1 = %i\n", y1); \
                    if(DEBUG_COLLIDE)printf("        r1 = %i\n", r1); \
                    if(x1 < 0 || x1 >= w1 || y1 < 0 || y1 >= h1){ \
                        goto nocoll_##PART;} \
                    \
                    hexcollmap_tile_t *tile1 = \
                        &map1->tiles[y1 * w1 + x1]; \
                    bool collide = tile1->PART[r1]; \
                    if(collide){ \
                        if(DEBUG_COLLIDE)printf("        HIT\n"); \
                        if(!all)return true; \
                        continue; \
                    } \
                nocoll_##PART: \
                    if(all)return false; \
                }
            COLLIDE(vert, 1)
            COLLIDE(edge, 3)
            COLLIDE(face, 2)
            #undef COLLIDE
        }
    }
    if(all)return true;
    else return false;
}




/**********
 * HEXMAP *
 **********/

void hexmap_cleanup(hexmap_t *map){
    free(map->name);
}

int hexmap_init(hexmap_t *map, char *name, vecspace_t *space,
    prismelrenderer_t *prend,
    prismelmapper_t *mapper,
    vec_t unit,
    rendergraph_t *rgraph_vert,
    rendergraph_t *rgraph_edge,
    rendergraph_t *rgraph_face
){
    int err;

    map->name = name;
    map->prend = prend;
    map->mapper = mapper;
    vec_cpy(prend->space->dims, map->unit, unit);

    err = hexcollmap_init(&map->collmap, space);
    if(err)return err;

    map->rgraph_vert = rgraph_vert;
    map->rgraph_edge = rgraph_edge;
    map->rgraph_face = rgraph_face;
    map->rgraph_map = NULL;
    return 0;
}

static int add_tile_rgraph(rendergraph_t *rgraph, rendergraph_t *rgraph2,
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

int hexmap_create_rgraph(hexmap_t *map, rendergraph_t **rgraph_ptr){
    int err;

    prismelrenderer_t *prend = map->prend;
    vecspace_t *space = prend->space;
    hexcollmap_t *collmap = &map->collmap;

    ARRAY_PUSH_NEW(rendergraph_t, *prend, rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(map->name), space,
        rendergraph_animation_type_default,
        rendergraph_n_frames_default);
    if(err)return err;

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            int px = x - collmap->ox;
            int py = y - collmap->oy;

            vec_t v;
            vec4_set(v, px + py, 0, -py, 0);
            vec_mul(space, v, map->unit);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            for(int i = 0; i < 1; i++){
                if(tile->vert[i]){
                    err = add_tile_rgraph(rgraph, map->rgraph_vert,
                        space, v, i * 2);
                    if(err)return err;}}
            for(int i = 0; i < 3; i++){
                if(tile->edge[i]){
                    err = add_tile_rgraph(rgraph, map->rgraph_edge,
                        space, v, i * 2);
                    if(err)return err;}}
            for(int i = 0; i < 2; i++){
                if(tile->face[i]){
                    err = add_tile_rgraph(rgraph, map->rgraph_face,
                        space, v, i * 2);
                    if(err)return err;}}
        }
    }

    *rgraph_ptr = rgraph;
    return 0;
}

int hexmap_load(hexmap_t *map, prismelrenderer_t *prend,
    const char *filename
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexmap_parse(map, prend, strdup(filename), &lexer);
    if(err)return err;

    free(text);
    return 0;
}

int hexmap_parse(hexmap_t *map, prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer
){
    int err;
    vecspace_t *space = &hexspace;
    prismelmapper_t *mapper = NULL;

    err = fus_lexer_next(lexer);
    if(err)return err;


    /* (maybe) get mapper */

    if(fus_lexer_got(lexer, "map")){
        err = fus_lexer_expect(lexer, "(");
        if(err)return err;
        err = fus_lexer_expect_mapper(lexer, prend, NULL, &mapper);
        if(err)return err;
        err = fus_lexer_expect(lexer, ")");
        if(err)return err;
        err = fus_lexer_next(lexer);
        if(err)return err;
    }


    /* parse unit */

    vec_t unit;
    err = fus_lexer_get(lexer, "unit");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    for(int i = 0; i < prend->space->dims; i++){
        err = fus_lexer_expect_int(lexer, &unit[i]);
        if(err)return err;
    }
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;


    /* parse vert, edge, face, player */

    rendergraph_t *rgraph_vert;
    rendergraph_t *rgraph_edge;
    rendergraph_t *rgraph_face;

    #define GET_RGRAPH(TYPE) { \
        err = fus_lexer_expect(lexer, #TYPE); \
        if(err)return err; \
        err = fus_lexer_expect(lexer, "("); \
        if(err)return err; \
        \
        char *name; \
        err = fus_lexer_expect_str(lexer, &name); \
        if(err)return err; \
        rgraph_##TYPE = prismelrenderer_get_rendergraph( \
            prend, name); \
        if(rgraph_##TYPE == NULL){ \
            fus_lexer_err_info(lexer); \
            fprintf(stderr, "Couldn't find shape: %s\n", name); \
            free(name); return 2;} \
        free(name); \
        \
        err = fus_lexer_expect(lexer, ")"); \
        if(err)return err; \
    }
    GET_RGRAPH(vert)
    GET_RGRAPH(edge)
    GET_RGRAPH(face)
    #undef GET_RGRAPH


    /* home stretch! init the map */
    err = hexmap_init(map, name, space, prend, mapper, unit,
        rgraph_vert, rgraph_edge, rgraph_face);
    if(err)return err;

    /* parse collmap */
    err = fus_lexer_expect(lexer, "collmap");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = hexcollmap_parse(&map->collmap, lexer);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* render rgraph_map */
    err = hexmap_create_rgraph(map, &map->rgraph_map);
    if(err)return err;

    /* pheeew */
    return 0;
}

