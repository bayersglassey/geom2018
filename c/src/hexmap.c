

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
            fprintf(f, "[");
                for(int i = 0; i < 1; i++){
                    fprintf(f, "%c", tile->vert[i].tile_c);}
            fprintf(f, "][");
                for(int i = 0; i < 3; i++){
                    fprintf(f, "%c", tile->edge[i].tile_c);}
            fprintf(f, "][");
                for(int i = 0; i < 2; i++){
                    fprintf(f, "%c", tile->face[i].tile_c);}
            fprintf(f, "] ");
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
            }else if(strchr(" .+/-\\*S%", c) != NULL){
                /* these are all fine */
            }else if(c == '['){
                /* next line plz */
                break;
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
            if(strchr("+/-\\*S", c) != NULL){
                int mx, my; bool is_face1;

                /* savepoint is just a face */
                if(c == 'S')c = '*';

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
    int map_size = map_w * map_h;
    hexcollmap_tile_t *tiles = calloc(map_size, sizeof(*tiles));
    if(tiles == NULL)return 1;

    /* Intermission: Initialize tile elements */
    for(int i = 0; i < map_size; i++){
        for(int j = 0; j < 1; j++)tiles[i].vert[j].tile_c = ' ';
        for(int j = 0; j < 3; j++)tiles[i].edge[j].tile_c = ' ';
        for(int j = 0; j < 2; j++)tiles[i].face[j].tile_c = ' ';
    }

    /* Iteration 3: The meat of it all */
    for(int y = 0; y < lines_len; y++){
        char *line = lines[y];
        int line_len = strlen(line);

        char *tilebucket = NULL;
        bool tilebucket_active = false;
            /* A "tile bucket" is a group of characters at the end
            of the line, surrounded by square brackets, e.g. [1aq].
            While parsing a line, the '%' character indicates that we
            should find the next tile bucket; then, as we encounter
            '-' '/' '\' '+' '*' characters, we will use */

        for(int x = 0; x < line_len; x++){
            char c = line[x];
            if(strchr("+/-\\*S", c) != NULL){
                int mx, my; bool is_face1;

                bool is_savepoint = c == 'S';
                if(is_savepoint)c = '*';

                get_map_coords(x-ox, y-oy, c,
                    &mx, &my, &is_face1);
                mx -= map_l;
                my -= map_t;
                hexcollmap_tile_t *tile = &tiles[my * map_w + mx];

                char tile_c = is_savepoint? 'S': '0';
                    /* The default tile_c is '0'. We could make that
                    more explicit, maybe as a param to this function,
                    attribute of hexmap or submaps, etc?.. */
                    /* NOTE: The way we've implemented this, 'S' can be
                    overwritten by '%'. Maybe that's weird? Maybe if
                    is_savepoint then we should skip the check for
                    tilebucket_active entirely? */

                if(tilebucket_active){
                    /* Get next non-' ' character in current tile bucket. */
                    char c;
                    while(c = *tilebucket, c == ' ')tilebucket++;
                    if(c == ']'){
                        tilebucket_active = false;
                    }else if(!isprint(c)){
                        fprintf(stderr,
                            "Hexcollmap line %i, char %i: char %li: ",
                            y+1, x+1, tilebucket-line+1);
                        if(c == '\0'){
                            fprintf(stderr, "Hit end of line\n");}
                        else{fprintf(stderr,
                            "Hit unprintable character: %x\n", c);}
                        return 2;
                    }else{
                        tile_c = c;
                        tilebucket++;
                    }
                }

                if(c == '+'){
                    tile->vert[0].tile_c = tile_c;
                }else if(c == '-'){
                    tile->edge[0].tile_c = tile_c;
                }else if(c == '/'){
                    tile->edge[1].tile_c = tile_c;
                }else if(c == '\\'){
                    tile->edge[2].tile_c = tile_c;
                }else if(c == '*'){
                    tile->face[is_face1? 1: 0].tile_c = tile_c;
                }
            }else if(c == '%'){
                /* Activate next tile bucket. */

                char c;
                if(tilebucket == NULL){
                    tilebucket = &line[x+1];
                    while(c = *tilebucket, c != '\0' && c != '[')tilebucket++;
                    if(c != '['){
                        fprintf(stderr,
                            "Hexcollmap line %i, char %i: ", y+1, x+1);
                        fprintf(stderr, "Didn't find '[' in line\n");
                        return 2;}
                    tilebucket++;
                }else{
                    while(c = *tilebucket, c == ' ')tilebucket++;
                    if(c != ']'){
                        fprintf(stderr,
                            "Hexcollmap line %i, char %i: char %li: ",
                            y+1, x+1, tilebucket-line+1);
                        fprintf(stderr, "Expected ']', got '%c'\n", c);
                        return 2;}
                    tilebucket++;
                    while(c = *tilebucket, c == ' ')tilebucket++;
                    if(c != '['){
                        fprintf(stderr,
                            "Hexcollmap line %i, char %i: char %li: ",
                            y+1, x+1, tilebucket-line+1);
                        fprintf(stderr, "Expected '[', got '%c'\n", c);
                        return 2;}
                    tilebucket++;
                }
                tilebucket_active = true;
            }else if(c == '['){
                break;
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
    if(err){
        fus_lexer_err_info(lexer);
        fprintf(stderr, "Couldn't parse hexcollmap lines\n");
        return err;}

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

hexcollmap_tile_t *hexcollmap_get_tile(hexcollmap_t *collmap, trf_t *index){
    int x = collmap->ox + index->add[0];
    int y = collmap->oy + index->add[1];
    if(x < 0 || x >= collmap->w || y < 0 || y >= collmap->h)return NULL;
    return &collmap->tiles[y * collmap->w + x];
}

hexcollmap_elem_t *hexcollmap_get_vert(hexcollmap_t *collmap, trf_t *index){
    hexcollmap_tile_t *tile = hexcollmap_get_tile(collmap, index);
    if(tile == NULL)return NULL;
    return &tile->vert[index->rot];
}

hexcollmap_elem_t *hexcollmap_get_edge(hexcollmap_t *collmap, trf_t *index){
    hexcollmap_tile_t *tile = hexcollmap_get_tile(collmap, index);
    if(tile == NULL)return NULL;
    return &tile->edge[index->rot];
}

hexcollmap_elem_t *hexcollmap_get_face(hexcollmap_t *collmap, trf_t *index){
    hexcollmap_tile_t *tile = hexcollmap_get_tile(collmap, index);
    if(tile == NULL)return NULL;
    return &tile->face[index->rot];
}

bool hexcollmap_elem_is_visible(hexcollmap_elem_t *elem){
    if(elem == NULL)return false;
    char tile_c = elem->tile_c;
    return tile_c != ' ';
}

bool hexcollmap_elem_is_solid(hexcollmap_elem_t *elem){
    if(elem == NULL)return false;
    char tile_c = elem->tile_c;
    return tile_c != ' ' && tile_c != 'S';
}




/**********
 * HEXMAP *
 **********/

void hexmap_cleanup(hexmap_t *map){
    free(map->name);

    ARRAY_FREE(hexmap_submap_t, *map, submaps, hexmap_submap_cleanup)

    ARRAY_FREE(hexmap_rgraph_elem_t, *map, rgraph_verts, (void))
    ARRAY_FREE(hexmap_rgraph_elem_t, *map, rgraph_edges, (void))
    ARRAY_FREE(hexmap_rgraph_elem_t, *map, rgraph_faces, (void))
}

int hexmap_init(hexmap_t *map, char *name, vecspace_t *space,
    prismelrenderer_t *prend,
    vec_t unit
){
    int err;

    map->name = name;
    map->space = space;
    map->prend = prend;
    vec_cpy(prend->space->dims, map->unit, unit);
    vec_zero(space->dims, map->spawn);

    ARRAY_INIT(*map, submaps)

    ARRAY_INIT(*map, rgraph_verts)
    ARRAY_INIT(*map, rgraph_edges)
    ARRAY_INIT(*map, rgraph_faces)
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
        /* hexmap's space is always hexspace. we will pass it
        to hexmap_init below, instead of just settings it here,
        which would probably make more sense.
        But ultimately we should really just move the call to
        hexmap_init out of hexmap_parse into hexmap_load. SO DO THAT */

    err = fus_lexer_next(lexer);
    if(err)return err;


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

    /* init the map */
    err = hexmap_init(map, name, space, prend, unit);
    if(err)return err;

    /* parse spawn point */
    err = fus_lexer_expect(lexer, "spawn");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_expect_vec(lexer, space, map->spawn);
    if(err)return err;
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;


    /* parse vert, edge, face, rgraphs */
    #define GET_RGRAPH(TYPE) { \
        err = fus_lexer_expect(lexer, #TYPE"s"); \
        if(err)return err; \
        err = fus_lexer_expect(lexer, "("); \
        if(err)return err; \
        while(1){ \
            err = fus_lexer_next(lexer); \
            if(err)return err; \
            if(fus_lexer_got(lexer, ")"))break; \
            \
            char *name; \
            err = fus_lexer_get_str(lexer, &name); \
            if(err)return err; \
            if(strlen(name) != 1){ \
                fprintf(stderr, "Expected single character, got: %s\n", \
                    name); \
                free(name); return 2;} \
            char tile_c = name[0]; \
            free(name); \
            \
            err = fus_lexer_expect(lexer, "("); \
            if(err)return err; \
            err = fus_lexer_expect_str(lexer, &name); \
            if(err)return err; \
            rendergraph_t *rgraph = \
                prismelrenderer_get_rendergraph(prend, name); \
            if(rgraph == NULL){ \
                fus_lexer_err_info(lexer); \
                fprintf(stderr, "Couldn't find shape: %s\n", name); \
                free(name); return 2;} \
            free(name); \
            ARRAY_PUSH_NEW(hexmap_rgraph_elem_t, *map, rgraph_##TYPE##s, \
                rgraph_##TYPE) \
            rgraph_##TYPE->tile_c = tile_c; \
            rgraph_##TYPE->rgraph = rgraph; \
            err = fus_lexer_expect(lexer, ")"); \
            if(err)return err; \
        } \
    }
    GET_RGRAPH(vert)
    GET_RGRAPH(edge)
    GET_RGRAPH(face)
    #undef GET_RGRAPH


    /* parse areas & submaps */
    err = fus_lexer_expect(lexer, "areas");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;
        if(fus_lexer_got(lexer, ")"))break;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = hexmap_parse_area(map, lexer);
        if(err)return err;
        err = fus_lexer_expect(lexer, ")");
        if(err)return err;
    }

    /* pheeew */
    return 0;
}

int hexmap_parse_area(hexmap_t *map, fus_lexer_t *lexer){
    int err;
    vecspace_t *space = map->space;

    vec_t camera_pos;
    err = fus_lexer_expect(lexer, "camera");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    {
        err = fus_lexer_expect_vec(lexer, space, camera_pos);
        if(err)return err;
    }
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;

    err = fus_lexer_next(lexer);
    if(err)return err;

    prismelmapper_t *mapper = NULL;
    if(fus_lexer_got(lexer, "mapper")){
        err = fus_lexer_expect(lexer, "(");
        if(err)return err;
        err = fus_lexer_expect_mapper(lexer, map->prend, NULL, &mapper);
        if(err)return err;
        err = fus_lexer_expect(lexer, ")");
        if(err)return err;
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

    err = fus_lexer_get(lexer, "submaps");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")"))break;

        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        {
            char *submap_filename;
            err = fus_lexer_expect_str(lexer, &submap_filename);
            if(err)return err;

            vec_t pos;
            err = fus_lexer_expect_vec(lexer, space, pos);
            if(err)return err;

            ARRAY_PUSH_NEW(hexmap_submap_t, *map, submaps, submap)
            err = hexmap_submap_load(map, submap, submap_filename, pos,
                camera_pos, mapper);
            if(err)return err;
        }
        err = fus_lexer_expect(lexer, ")");
        if(err)return err;
    }

    return 0;
}

bool hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all
){
    int ox2 = collmap2->ox;
    int oy2 = collmap2->oy;
    int w2 = collmap2->w;
    int h2 = collmap2->h;

    vecspace_t *space = map->space;

    /* NOTE: for tile coords (ox, oy, x, y, w, h),
    Y is reversed (down is positive, up is negative) */

    for(int y2 = 0; y2 < h2; y2++){
        for(int x2 = 0; x2 < w2; x2++){
            hexcollmap_tile_t *tile2 = &collmap2->tiles[y2 * w2 + x2];

            #define COLLIDE(PART, ROT) \
                for(int r2 = 0; r2 < ROT; r2++){ \
                    if(!hexcollmap_elem_is_solid( \
                        &tile2->PART[r2]))continue; \
                    trf_t index; \
                    hexspace_set(index.add, x2 - ox2, y2 - oy2); \
                    index.rot = r2; \
                    index.flip = false; \
                    \
                    /* And now, because we were fools and defined */ \
                    /* the tile coords such that their Y is flipped */ \
                    /* compared to vecspaces, we need to flip that Y */ \
                    /* before calling trf_apply and then flip it back */ \
                    /* again: */ \
                    index.add[1] = -index.add[1]; \
                    trf_apply(space, &index, trf); \
                    index.add[1] = -index.add[1]; \
                    hexcollmap_normalize_##PART(&index); \
                    \
                    bool collide = false; \
                    for(int i = 0; i < map->submaps_len; i++){ \
                        hexmap_submap_t *submap = map->submaps[i]; \
                        hexcollmap_t *collmap1 = &submap->collmap; \
                        \
                        trf_t subindex; \
                        hexspace_set(subindex.add, \
                            index.add[0] - submap->pos[0], \
                            index.add[1] + submap->pos[1]); \
                        subindex.rot = index.rot; \
                        subindex.flip = index.flip; \
                        \
                        hexcollmap_elem_t *PART = \
                            hexcollmap_get_##PART(collmap1, &subindex); \
                        if(hexcollmap_elem_is_solid(PART)){ \
                            collide = true; break;} \
                    } \
                    if(all && !collide)return false; \
                    if(!all && collide)return true; \
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

rendergraph_t *hexmap_get_rgraph_vert(hexmap_t *map, char tile_c){
    for(int i = 0; i < map->rgraph_verts_len; i++){
        hexmap_rgraph_elem_t *elem = map->rgraph_verts[i];
        if(elem->tile_c == tile_c)return elem->rgraph;
    }
    return NULL;
}

rendergraph_t *hexmap_get_rgraph_edge(hexmap_t *map, char tile_c){
    for(int i = 0; i < map->rgraph_edges_len; i++){
        hexmap_rgraph_elem_t *elem = map->rgraph_edges[i];
        if(elem->tile_c == tile_c)return elem->rgraph;
    }
    return NULL;
}

rendergraph_t *hexmap_get_rgraph_face(hexmap_t *map, char tile_c){
    for(int i = 0; i < map->rgraph_faces_len; i++){
        hexmap_rgraph_elem_t *elem = map->rgraph_faces[i];
        if(elem->tile_c == tile_c)return elem->rgraph;
    }
    return NULL;
}




void hexmap_submap_cleanup(hexmap_submap_t *submap){
    free(submap->filename);
    hexcollmap_cleanup(&submap->collmap);
}

int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, vec_t pos, vec_t camera_pos, prismelmapper_t *mapper
){
    int err;

    submap->filename = filename;
    vec_cpy(MAX_VEC_DIMS, submap->pos, pos);
    vec_cpy(MAX_VEC_DIMS, submap->camera_pos, camera_pos);

    err = hexcollmap_init(&submap->collmap, map->space);
    if(err)return err;

    submap->rgraph_map = NULL;
    submap->mapper = mapper;

    return 0;
}

int hexmap_submap_load(hexmap_t *map, hexmap_submap_t *submap,
    const char *filename, vec_t pos, vec_t camera_pos,
    prismelmapper_t *mapper
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexmap_submap_init(map, submap, strdup(filename), pos,
        camera_pos, mapper);
    if(err)return err;
    err = hexmap_submap_parse(map, submap, &lexer);
    if(err)return err;

    free(text);
    return 0;
}

int hexmap_submap_parse(hexmap_t *map, hexmap_submap_t *submap,
    fus_lexer_t *lexer
){
    int err;

    /* parse collmap */
    err = fus_lexer_expect(lexer, "collmap");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = hexcollmap_parse(&submap->collmap, lexer);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* render rgraph_map */
    err = hexmap_submap_create_rgraph(map, submap);
    if(err)return err;

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

int hexmap_submap_create_rgraph(hexmap_t *map, hexmap_submap_t *submap){
    int err;

    prismelrenderer_t *prend = map->prend;
    vecspace_t *space = prend->space;
    hexcollmap_t *collmap = &submap->collmap;

    /* BIG OL' HACK: If any "tile" rgraphs are animated, we need the
    map's rgraph to be animated also.
    The most correct way to do this is I guess to compute the LCD of
    the tile rgraphs' n_frames, and set the map's rgraph's n_frames
    to that.
    But for now we use 36, because it has "many" divisors.
    That's a lot of bitmaps to cache for the map's rgraph, though...
    if we're going to allow complicated map animations, maybe we
    should disable bitmap caching for it (somehow). */
    int n_frames = 36;

    ARRAY_PUSH_NEW(rendergraph_t, *prend, rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(map->name), space,
        rendergraph_animation_type_default,
        n_frames);
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
                hexcollmap_elem_t *elem = &tile->vert[i];
                if(hexcollmap_elem_is_visible(elem)){
                    err = add_tile_rgraph(rgraph,
                        hexmap_get_rgraph_vert(map, elem->tile_c),
                        space, v, i * 2);
                    if(err)return err;}}
            for(int i = 0; i < 3; i++){
                hexcollmap_elem_t *elem = &tile->edge[i];
                if(hexcollmap_elem_is_visible(elem)){
                    err = add_tile_rgraph(rgraph,
                        hexmap_get_rgraph_edge(map, elem->tile_c),
                        space, v, i * 2);
                    if(err)return err;}}
            for(int i = 0; i < 2; i++){
                hexcollmap_elem_t *elem = &tile->face[i];
                if(hexcollmap_elem_is_visible(elem)){
                    err = add_tile_rgraph(rgraph,
                        hexmap_get_rgraph_face(map, elem->tile_c),
                        space, v, i * 2);
                    if(err)return err;}}
        }
    }

    submap->rgraph_map = rgraph;
    return 0;
}

