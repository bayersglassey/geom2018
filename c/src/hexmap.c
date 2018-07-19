

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



/******************
 * HEXMAP TILESET *
 ******************/

void hexmap_tileset_cleanup(hexmap_tileset_t *tileset){
    free(tileset->name);
    ARRAY_FREE(hexmap_tileset_entry_t, tileset->vert_entries, (void))
    ARRAY_FREE(hexmap_tileset_entry_t, tileset->edge_entries, (void))
    ARRAY_FREE(hexmap_tileset_entry_t, tileset->face_entries, (void))
}

int hexmap_tileset_init(hexmap_tileset_t *tileset, char *name){
    tileset->name = name;
    ARRAY_INIT(tileset->vert_entries)
    ARRAY_INIT(tileset->edge_entries)
    ARRAY_INIT(tileset->face_entries)
    return 0;
}

static int hexmap_tileset_parse(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer
){
    int err;

    err = hexmap_tileset_init(tileset, name);
    if(err)return err;

    /* parse vert, edge, face, rgraphs */
    #define GET_RGRAPH(TYPE) { \
        err = fus_lexer_get(lexer, #TYPE"s"); \
        if(err)return err; \
        err = fus_lexer_get(lexer, "("); \
        if(err)return err; \
        while(1){ \
            if(fus_lexer_got(lexer, ")"))break; \
            \
            char tile_c; \
            err = fus_lexer_get_chr(lexer, &tile_c); \
            if(err)return err; \
            \
            err = fus_lexer_get(lexer, "("); \
            if(err)return err; \
            err = fus_lexer_get_str(lexer, &name); \
            if(err)return err; \
            rendergraph_t *rgraph = \
                prismelrenderer_get_rendergraph(prend, name); \
            if(rgraph == NULL){ \
                fus_lexer_err_info(lexer); \
                fprintf(stderr, "Couldn't find shape: %s\n", name); \
                free(name); return 2;} \
            free(name); \
            ARRAY_PUSH_NEW(hexmap_tileset_entry_t, \
                tileset->TYPE##_entries, entry) \
            entry->tile_c = tile_c; \
            entry->rgraph = rgraph; \
            err = fus_lexer_get(lexer, ")"); \
            if(err)return err; \
        } \
        err = fus_lexer_next(lexer); \
        if(err)return err; \
    }
    GET_RGRAPH(vert)
    GET_RGRAPH(edge)
    GET_RGRAPH(face)
    #undef GET_RGRAPH

    return 0;
}

int hexmap_tileset_load(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexmap_tileset_parse(tileset, prend, strdup(filename),
        &lexer);
    if(err)return err;

    free(text);
    return 0;
}

rendergraph_t *hexmap_tileset_get_rgraph_vert(hexmap_tileset_t *tileset,
    char tile_c
){
    for(int i = 0; i < tileset->vert_entries_len; i++){
        hexmap_tileset_entry_t *entry = tileset->vert_entries[i];
        if(entry->tile_c == tile_c)return entry->rgraph;
    }
    return NULL;
}

rendergraph_t *hexmap_tileset_get_rgraph_edge(hexmap_tileset_t *tileset,
    char tile_c
){
    for(int i = 0; i < tileset->edge_entries_len; i++){
        hexmap_tileset_entry_t *entry = tileset->edge_entries[i];
        if(entry->tile_c == tile_c)return entry->rgraph;
    }
    return NULL;
}

rendergraph_t *hexmap_tileset_get_rgraph_face(hexmap_tileset_t *tileset,
    char tile_c
){
    for(int i = 0; i < tileset->face_entries_len; i++){
        hexmap_tileset_entry_t *entry = tileset->face_entries[i];
        if(entry->tile_c == tile_c)return entry->rgraph;
    }
    return NULL;
}



/**************
 * HEXCOLLMAP *
 **************/

int hexcollmap_part_init(hexcollmap_part_t *part,
    char part_c, char *filename
){
    part->part_c = part_c;
    part->filename = filename;
    return 0;
}

void hexcollmap_part_cleanup(hexcollmap_part_t *part){
    free(part->filename);
}


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

static bool represents_vert(char c){return c == '+';}
static bool represents_edge(char c){return strchr("-/\\", c) != NULL;}
static bool represents_face(char c){return c == '*';}

static char get_map_elem_type(int x, int y){
    /* A poorly-named function which figures out whether a vert, edge, or
    face is at the given hexcollmap coordinates */

    /* What a tile looks like in the hexcollmap text format: */
    //   "  + - +    "
    //   "   \*/*\   "
    //   "   (+)- +  "
    /* ...where ( ) indicates the origin (x=0, y=0) */

    /* apply the formula for a vertex */
    int rem_x = _rem(x - y, 4);
    int rem_y = _rem(y, 2);

    if(rem_y == 0){
        // + - + - + - ...
        if(rem_x == 0)return '+';
        if(rem_x == 2)return '-';
    }else{
        // \*/*\*/*\* ...
        if(rem_x == 0)return '\\';
        if(rem_x == 1)return '*';
        if(rem_x == 2)return '/';
        if(rem_x == 3)return '*';
    }
    return ' ';
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

static int hexcollmap_draw(hexcollmap_t *collmap1, hexcollmap_t *collmap2,
    trf_t *trf, int draw_z
){
    int err;

    int ox2 = collmap2->ox;
    int oy2 = collmap2->oy;
    int w2 = collmap2->w;
    int h2 = collmap2->h;

    vecspace_t *space = collmap1->space;

    /* NOTE: for tile coords (ox, oy, x, y, w, h),
    Y is reversed (down is positive, up is negative) */

    for(int y2 = 0; y2 < h2; y2++){
        for(int x2 = 0; x2 < w2; x2++){
            hexcollmap_tile_t *tile2 = &collmap2->tiles[y2 * w2 + x2];

            #define HEXCOLLMAP_DRAW(PART, ROT) \
                for(int r2 = 0; r2 < ROT; r2++){ \
                    hexcollmap_elem_t *elem2 = &tile2->PART[r2]; \
                    if(elem2->tile_c != 'x' && \
                        !hexcollmap_elem_is_visible(elem2)){ \
                            continue; } \
                    \
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
                    hexcollmap_elem_t *elem1 = \
                        hexcollmap_get_##PART(collmap1, &index); \
                    if(elem1 != NULL && draw_z >= elem1->z){ \
                        elem1->tile_c = elem2->tile_c; \
                        elem1->z = draw_z;} \
                }
            HEXCOLLMAP_DRAW(vert, 1)
            HEXCOLLMAP_DRAW(edge, 3)
            HEXCOLLMAP_DRAW(face, 2)
            #undef HEXCOLLMAP_DRAW
        }
    }
    return 0;
}

static int hexcollmap_parse_lines(hexcollmap_t *collmap,
    char **lines, int lines_len, hexcollmap_part_t **parts, int parts_len,
    char default_vert_c, char default_edge_c, char default_face_c
){
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
            }else if(strchr(" x.+/-\\*S%?", c) != NULL){
                /* these are all fine */
            }else if(c == '['){
                /* next line plz, "tilebuckets" don't affect the origin */
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
            if(strchr(".+/-\\*S?", c) != NULL){
                int mx, my; bool is_face1;

                /* savepoint is just a face */
                if(c == 'S')c = '*';

                /* dots & part references are just verts */
                if(c == '.' || c == '?')c = '+';

                char elem_type = get_map_elem_type(x-ox, y-oy);
                if(elem_type != c){
                    fprintf(stderr, "Line %i, char %i: character doesn't "
                        "belong at these coordinates: got %c, expected %c\n",
                        y, x, c, elem_type);
                    return 2;}

                get_map_coords(x-ox, y-oy, c,
                    &mx, &my, &is_face1);
                map_t = _min(map_t, my);
                map_b = _max(map_b, my);
                map_l = _min(map_l, mx);
                map_r = _max(map_r, mx);
            }else if(strchr(" x.+/-\\*S%?()", c) != NULL){
                /* these are all fine */
            }else if(c == '['){
                /* next line plz, "tilebuckets" don't affect bounds */
                break;
            }
        }
    }

    /* Intermission: initialize collmap with empty tile data */
    /* ...Allocate map data */
    int map_w = map_r - map_l + 1;
    int map_h = map_b - map_t + 1;
    int map_size = map_w * map_h;
    hexcollmap_tile_t *tiles = calloc(map_size, sizeof(*tiles));
    if(tiles == NULL)return 1;
    /* ...Initialize tile elements */
    for(int i = 0; i < map_size; i++){
        for(int j = 0; j < 1; j++)tiles[i].vert[j].tile_c = ' ';
        for(int j = 0; j < 3; j++)tiles[i].edge[j].tile_c = ' ';
        for(int j = 0; j < 2; j++)tiles[i].face[j].tile_c = ' ';
    }
    /* ...Assign attributes */
    collmap->ox = -map_l;
    collmap->oy = -map_t;
    collmap->w = map_w;
    collmap->h = map_h;
    collmap->tiles = tiles;

    /* Iterations 3 & 4: The meat of it all - parse tile data */
    for(int iter_i = 0; iter_i < 2; iter_i++){
        /* While iter_i == 0, we parse regular tile data.
        While iter_i == 1, we parse "part references", that is, the '?'
        character, which loads & draws other collmaps over the tile data
        we parsed while iter_i == 0. */
    for(int y = 0; y < lines_len; y++){
        char *line = lines[y];
        int line_len = strlen(line);

        char *tilebucket = NULL;
        bool tilebucket_active = false;
            /* A "tile bucket" is a group of characters at the end
            of the line, surrounded by square brackets, e.g. [1aq].
            While parsing a line, the '%' character indicates that we
            should find the next tile bucket; then, as we encounter
            '-' '/' '\' '+' '*' characters, we will use the characters
            in the bucket one at a time as the tile_c character,
            instead of the default tile_c.
            While parsing a line, '?' works similarly to '%' except
            that instead of modifying tile_c, it loads other collmaps
            over this one.
            TODO: Clarify this comment... */

        for(int x = 0; x < line_len; x++){
            char c = line[x];
            if(strchr("x+/-\\*S", c) != NULL){
                int mx, my; bool is_face1;

                bool is_savepoint = c == 'S';
                if(is_savepoint)c = '*';

                bool is_hard_transparent = c == 'x';
                if(is_hard_transparent)c = get_map_elem_type(x-ox, y-oy);

                char tile_c =
                    is_savepoint? 'S':
                    is_hard_transparent? 'x':
                    represents_vert(c)? default_vert_c:
                    represents_edge(c)? default_edge_c:
                    represents_face(c)? default_face_c:
                    ' ';
                    /* NOTE: The way we've implemented this, 'S' can be
                    overwritten by '%'. Maybe that's weird? Maybe if
                    is_savepoint then we should skip the check for
                    tilebucket_active entirely? */

                int draw_z = 0;

                if(tilebucket_active){
                    /* Get next non-' ' character in current tile bucket. */
                    char c2;
                    while(c2 = *tilebucket, c2 == ' ')tilebucket++;
                    if(c2 == ']'){
                        tilebucket_active = false;
                    }else if(!isprint(c2)){
                        fprintf(stderr,
                            "Hexcollmap line %i, char %i: char %li: ",
                            y+1, x+1, tilebucket-line+1);
                        if(c2 == '\0'){
                            fprintf(stderr, "Hit end of line\n");}
                        else{fprintf(stderr,
                            "Hit unprintable character: %x\n", c2);}
                        return 2;
                    }else{
                        tile_c = c2;
                        tilebucket++;
                        while(1){
                            if(*tilebucket == '|'){
                                tilebucket++;
                                draw_z = atoi(tilebucket);
                                while(isdigit(*tilebucket))tilebucket++;
                            }else break;
                        }
                    }
                }

                if(iter_i == 0){
                    get_map_coords(x-ox, y-oy, c,
                        &mx, &my, &is_face1);
                    mx -= map_l;
                    my -= map_t;
                    hexcollmap_tile_t *tile = &tiles[my * map_w + mx];

                    hexcollmap_elem_t *elem = NULL;
                    if(c == '+'){
                        elem = &tile->vert[0];
                    }else if(c == '-'){
                        elem = &tile->edge[0];
                    }else if(c == '/'){
                        elem = &tile->edge[1];
                    }else if(c == '\\'){
                        elem = &tile->edge[2];
                    }else if(c == '*'){
                        elem = &tile->face[is_face1? 1: 0];
                    }
                    if(elem != NULL){
                        /* We don't expect elem to be NULL, but it never
                        hurts to check */
                        elem->tile_c = tile_c;
                        elem->z = draw_z;
                    }
                }
            }else if(c == '%' || c == '?'){

                /* Find next tile bucket. */
                char c2;
                if(tilebucket == NULL){
                    tilebucket = &line[x+1];
                    while(c2 = *tilebucket, c2 != '\0' && c2 != '['){
                        tilebucket++;}
                    if(c2 != '['){
                        fprintf(stderr,
                            "Hexcollmap line %i, char %i: ", y+1, x+1);
                        fprintf(stderr, "Didn't find '[' in line\n");
                        return 2;}
                    tilebucket++;
                }else{
                    while(c2 = *tilebucket, c2 == ' ')tilebucket++;
                    if(c2 != ']'){
                        fprintf(stderr,
                            "Hexcollmap line %i, char %i: char %li: ",
                            y+1, x+1, tilebucket-line+1);
                        fprintf(stderr, "Expected ']', got '%c'\n", c2);
                        return 2;}
                    tilebucket++;
                    while(c2 = *tilebucket, c2 == ' ')tilebucket++;
                    if(c2 != '['){
                        fprintf(stderr,
                            "Hexcollmap line %i, char %i: char %li: ",
                            y+1, x+1, tilebucket-line+1);
                        fprintf(stderr, "Expected '[', got '%c'\n", c2);
                        return 2;}
                    tilebucket++;
                }

                if(c == '%'){
                    /* Activate the bucket we found. */
                    tilebucket_active = true;
                }else if(c == '?'){
                    /* Load collmaps from the bucket we found and draw them
                    onto current collmap. */

                    /* loop through all characters in tilebucket */
                    while(1){
                        /* Get next non-' ' character in current tile bucket. */
                        char c2;
                        while(c2 = *tilebucket, c2 == ' ')tilebucket++;

                        if(c2 == ']')break;

                        if(!isprint(c2)){
                            fprintf(stderr,
                                "Hexcollmap line %i, char %i: char %li: ",
                                y+1, x+1, tilebucket-line+1);
                            if(c2 == '\0'){
                                fprintf(stderr, "Hit end of line\n");}
                            else{fprintf(stderr,
                                "Hit unprintable character: %x\n", c2);}
                            return 2;
                        }

                        tilebucket++;

                        if(iter_i == 1){
                            /* Find part with given part_c */
                            char *filename = NULL;
                            for(int i = 0; i < parts_len; i++){
                                hexcollmap_part_t *part = parts[i];
                                if(part->part_c == c2){
                                    filename = part->filename; break;}
                            }

                            if(filename == NULL){
                                fprintf(stderr,
                                    "Hexcollmap line %i, char %i: char %li: "
                                    "part not found: %c\n",
                                    y+1, x+1, tilebucket-line+1, c2);
                                return 2;}

                            int mx, my; bool is_face1;
                            get_map_coords(x-ox, y-oy, '+',
                                &mx, &my, NULL);

                            trf_t trf = {0};
                            trf.add[0] = mx;
                            trf.add[1] = -my;
                            int draw_z = 0;
                            while(1){
                                if(*tilebucket == '^'){
                                    tilebucket++;
                                    rot_t rot_add = atoi(tilebucket);
                                    trf.rot += rot_add;
                                    while(isdigit(*tilebucket))tilebucket++;
                                }else if(*tilebucket == '~'){
                                    tilebucket++;
                                    trf.flip = !trf.flip;
                                }else if(*tilebucket == '|'){
                                    tilebucket++;
                                    draw_z = atoi(tilebucket);
                                    while(isdigit(*tilebucket))tilebucket++;
                                }else break;
                            }

                            hexcollmap_t part_collmap;
                            err = hexcollmap_init(&part_collmap,
                                collmap->space);
                            if(err)return err;
                            err = hexcollmap_load(&part_collmap,
                                filename);
                            if(err)return err;
                            err = hexcollmap_draw(collmap, &part_collmap,
                                &trf, draw_z);
                            if(err)return err;
                            hexcollmap_cleanup(&part_collmap);
                        }
                    }
                }

            }else if(c == '['){
                /* We hit a tilebucket, so no more regular tile data on
                this line. Next plz! */
                break;
            }
        }
    }
    }

    /* OKAY */
    return 0;
}

int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer,
    bool just_coll
){
    int err;

    char default_vert_c = '0';
    char default_edge_c = '0';
    char default_face_c = '0';

    ARRAY_DECL(hexcollmap_part_t, parts)
    ARRAY_INIT(parts)

    if(!just_coll){
        if(fus_lexer_got(lexer, "parts")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            while(1){
                if(fus_lexer_got(lexer, ")"))break;

                char part_c;
                err = fus_lexer_get_chr(lexer, &part_c);
                if(err)return err;

                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                {
                    char *filename;
                    err = fus_lexer_get_str(lexer, &filename);
                    if(err)return err;
                    ARRAY_PUSH_NEW(hexcollmap_part_t, parts, part)
                    err = hexcollmap_part_init(part, part_c, filename);
                    if(err)return err;
                }
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }
            err = fus_lexer_next(lexer);
            if(err)return err;
        }

        if(fus_lexer_got(lexer, "default_vert")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_chr(lexer, &default_vert_c);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }

        if(fus_lexer_got(lexer, "default_edge")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_chr(lexer, &default_edge_c);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }

        if(fus_lexer_got(lexer, "default_face")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_chr(lexer, &default_face_c);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }

        err = fus_lexer_get(lexer, "collmap");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
    }

    /* set up dynamic array of lines */
    int collmap_lines_len = 0;
    int collmap_lines_size = 8;
    char **collmap_lines = calloc(collmap_lines_size,
        sizeof(*collmap_lines));
    if(collmap_lines == NULL)return 1;

    /* read in lines */
    while(1){
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
        collmap_lines, collmap_lines_len, parts, parts_len,
        default_vert_c, default_edge_c, default_face_c);
    if(err){
        fus_lexer_err_info(lexer);
        fprintf(stderr, "Couldn't parse hexcollmap lines\n");
        return err;}

    if(!just_coll){
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    /* free lines and dynamic array thereof */
    for(int i = 0; i < collmap_lines_len; i++){
        free(collmap_lines[i]);
        collmap_lines[i] = NULL;}
    free(collmap_lines);


    ARRAY_FREE(hexcollmap_part_t, parts, hexcollmap_part_cleanup)

    return 0;
}

int hexcollmap_load(hexcollmap_t *collmap, const char *filename){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexcollmap_parse(collmap, &lexer, false);
    if(err)return err;

    free(text);
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
    return tile_c != ' ' && tile_c != 'x';
}

bool hexcollmap_elem_is_solid(hexcollmap_elem_t *elem){
    if(elem == NULL)return false;
    char tile_c = elem->tile_c;
    return tile_c != ' ' && tile_c != 'x' && tile_c != 'S';
}




/**********
 * HEXMAP *
 **********/

void hexmap_cleanup(hexmap_t *map){
    free(map->name);

    ARRAY_FREE(hexmap_submap_t, map->submaps, hexmap_submap_cleanup)

    ARRAY_FREE(char, map->recording_filenames, (void))
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

    ARRAY_INIT(map->submaps)

    ARRAY_INIT(map->recording_filenames)
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


    /* parse unit */
    vec_t unit;
    err = fus_lexer_get(lexer, "unit");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    for(int i = 0; i < prend->space->dims; i++){
        err = fus_lexer_get_int(lexer, &unit[i]);
        if(err)return err;
    }
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* init the map */
    err = hexmap_init(map, name, space, prend, unit);
    if(err)return err;

    /* parse spawn point */
    char *spawn_filename = NULL;
    err = fus_lexer_get(lexer, "spawn");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    if(fus_lexer_got_str(lexer)){
        err = fus_lexer_get_str(lexer, &spawn_filename);
        if(err)return err;
    }else{
        err = fus_lexer_get_vec(lexer, space, map->spawn);
        if(err)return err;
    }
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* default palette */
    char *default_palette_filename;
    err = fus_lexer_get(lexer, "default_palette");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &default_palette_filename);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* default tileset */
    char *default_tileset_filename;
    err = fus_lexer_get(lexer, "default_tileset");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &default_tileset_filename);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;


    /* parse submaps */
    err = fus_lexer_get(lexer, "submaps");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    while(1){
        if(fus_lexer_got(lexer, ")"))break;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = hexmap_parse_submap(map, lexer,
            (vec_t){0}, (vec_t){0}, 0, NULL,
            default_palette_filename, default_tileset_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;


    /* maybe get spawn point from a submap */
    if(spawn_filename != NULL){
        hexmap_submap_t *spawn_submap = NULL;
        for(int i = 0; i < map->submaps_len; i++){
            hexmap_submap_t *submap = map->submaps[i];
            if(strcmp(submap->filename, spawn_filename) == 0){
                spawn_submap = submap; break;}
        }
        if(spawn_submap == NULL){
            fprintf(stderr, "Couldn't find submap with filename: %s\n",
                spawn_filename);
            return 2;}
        vec_cpy(map->space->dims, map->spawn, spawn_submap->pos);
        free(spawn_filename);
    }


    /* pheeew */
    return 0;
}

int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer,
    vec_t parent_pos, vec_t parent_camera_pos, int parent_camera_type,
    prismelmapper_t *parent_mapper, char *palette_filename,
    char *tileset_filename
){
    int err;
    vecspace_t *space = map->space;

    if(fus_lexer_got(lexer, "skip")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_parse_silent(lexer);
        if(err)return err;
        return 0;
    }

    char *submap_filename = NULL;
    if(fus_lexer_got(lexer, "file")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &submap_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    vec_t pos = {0};
    if(fus_lexer_got(lexer, "pos")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_vec(lexer, space, pos);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }
    vec_add(space->dims, pos, parent_pos);

    int camera_type = parent_camera_type;
    vec_t camera_pos;
    vec_cpy(space->dims, camera_pos, parent_camera_pos);
    if(fus_lexer_got(lexer, "camera")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        if(fus_lexer_got(lexer, "follow")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            camera_type = 1;
        }else{
            err = fus_lexer_get_vec(lexer, space, camera_pos);
            if(err)return err;
            vec_add(space->dims, camera_pos, pos);
        }
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    prismelmapper_t *mapper = parent_mapper;
    if(fus_lexer_got(lexer, "mapper")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_mapper(lexer, map->prend, NULL, &mapper);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "palette")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &palette_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "tileset")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &tileset_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(submap_filename != NULL){
        ARRAY_PUSH_NEW(hexmap_submap_t, map->submaps, submap)
        err = hexmap_submap_init(map, submap, strdup(submap_filename), pos,
            camera_type, camera_pos, mapper, palette_filename, tileset_filename);
        if(err)return err;

        /* load collmap */
        err = hexcollmap_load(&submap->collmap, submap_filename);
        if(err)return err;

        /* render submap->rgraph_map */
        err = hexmap_submap_create_rgraph(map, submap);
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "recordings")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;

            char *recording_filename;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_str(lexer, &recording_filename);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
            ARRAY_PUSH(char, map->recording_filenames,
                recording_filename)
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "submaps")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = hexmap_parse_submap(map, lexer, pos,
                camera_pos, camera_type, mapper,
                palette_filename, tileset_filename);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        err = fus_lexer_next(lexer);
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

            #define HEXCOLLMAP_COLLIDE(PART, ROT) \
                for(int r2 = 0; r2 < ROT; r2++){ \
                    if(!hexcollmap_elem_is_solid( \
                        &tile2->PART[r2]))continue; \
                    \
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
                        hexcollmap_elem_t *elem = \
                            hexcollmap_get_##PART(collmap1, &subindex); \
                        if(hexcollmap_elem_is_solid(elem)){ \
                            collide = true; break;} \
                    } \
                    if(all && !collide)return false; \
                    if(!all && collide)return true; \
                }
            HEXCOLLMAP_COLLIDE(vert, 1)
            HEXCOLLMAP_COLLIDE(edge, 3)
            HEXCOLLMAP_COLLIDE(face, 2)
            #undef HEXCOLLMAP_COLLIDE
        }
    }
    if(all)return true;
    else return false;
}




void hexmap_submap_cleanup(hexmap_submap_t *submap){
    free(submap->filename);
    hexcollmap_cleanup(&submap->collmap);
    palette_cleanup(&submap->palette);
    hexmap_tileset_cleanup(&submap->tileset);
}

int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, vec_t pos, int camera_type, vec_t camera_pos,
    prismelmapper_t *mapper, char *palette_filename, char *tileset_filename
){
    int err;

    submap->filename = filename;
    vec_cpy(MAX_VEC_DIMS, submap->pos, pos);

    submap->camera_type = camera_type;
    vec_cpy(MAX_VEC_DIMS, submap->camera_pos, camera_pos);

    err = hexcollmap_init(&submap->collmap, map->space);
    if(err)return err;

    submap->rgraph_map = NULL;
    submap->mapper = mapper;

    err = palette_load(&submap->palette, palette_filename);
    if(err)return err;

    err = hexmap_tileset_load(&submap->tileset, map->prend,
        tileset_filename);
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
    But for now we use a magic number which has "many" divisors.
    That's a lot of bitmaps to cache for the map's rgraph, though...
    if we're going to allow complicated map animations, maybe we
    should disable bitmap caching for it (somehow). */
    int n_frames = 24;

    ARRAY_PUSH_NEW(rendergraph_t, prend->rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(map->name), prend,
        rendergraph_animation_type_default,
        n_frames);
    if(err)return err;

    hexmap_tileset_t *tileset = &submap->tileset;

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            int px = x - collmap->ox;
            int py = y - collmap->oy;

            vec_t v;
            vec4_set(v, px + py, 0, -py, 0);
            vec_mul(space, v, map->unit);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            #define HEXMAP_ADD_TILE(PART, ROT) \
                for(int i = 0; i < ROT; i++){ \
                    hexcollmap_elem_t *elem = &tile->PART[i]; \
                    if(!hexcollmap_elem_is_visible(elem))continue; \
                    rendergraph_t *rgraph_tile = \
                        hexmap_tileset_get_rgraph_##PART( \
                            tileset, elem->tile_c); \
                    if(rgraph_tile == NULL){ \
                        fprintf(stderr, "Couldn't find " #PART " tile " \
                            "for character: %c\n", elem->tile_c); \
                        return 2;} \
                    err = add_tile_rgraph(rgraph, rgraph_tile, \
                        space, v, i * 2); \
                    if(err)return err; \
                }
            HEXMAP_ADD_TILE(vert, 1)
            HEXMAP_ADD_TILE(edge, 3)
            HEXMAP_ADD_TILE(face, 2)
            #undef HEXMAP_ADD_TILE
        }
    }

    submap->rgraph_map = rgraph;
    return 0;
}

