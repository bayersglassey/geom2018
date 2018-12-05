

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexmap.h"
#include "lexer.h"
#include "util.h"
#include "mathutil.h"
#include "geom.h"
#include "vec4.h"
#include "hexspace.h"
#include "prismelrenderer.h"



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
    free(collmap->name);
    free(collmap->tiles);
}

int hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    char *name
){
    memset(collmap, 0, sizeof(*collmap));
    collmap->name = name;
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
        bool parsing_part_references = iter_i == 1;
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

                if(!parsing_part_references){
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

                        if(parsing_part_references){
                            /* Find part with given part_c */
                            char *filename = NULL;
                            bool found = false;
                            for(int i = 0; i < parts_len; i++){
                                hexcollmap_part_t *part = parts[i];
                                if(part->part_c == c2){
                                    found = true;
                                    filename = part->filename;
                                    break;
                                }
                            }

                            if(!found){
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

                            /* If "empty" was specified for this part, then
                            filename will be NULL and we shouldn't do
                            anything. */
                            if(filename != NULL){
                                hexcollmap_t part_collmap;
                                err = hexcollmap_init(&part_collmap,
                                    collmap->space, strdup(filename));
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

    ARRAY_DECL(hexcollmap_part_t*, parts)
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
                if(fus_lexer_got(lexer, "empty")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    ARRAY_PUSH_NEW(hexcollmap_part_t*, parts, part)
                    err = hexcollmap_part_init(part, part_c, NULL);
                    if(err)return err;
                }else{
                    char *filename;
                    err = fus_lexer_get_str(lexer, &filename);
                    if(err)return err;
                    ARRAY_PUSH_NEW(hexcollmap_part_t*, parts, part)
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


    ARRAY_FREE_PTR(hexcollmap_part_t*, parts, hexcollmap_part_cleanup)

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

static int hexcollmap_collide_elem(hexcollmap_t *collmap1, bool all,
    vecspace_t *space, int x, int y, trf_t *trf,
    hexcollmap_elem_t *elems2, int rot,
    void (*normalize_elem)(trf_t *index),
    hexcollmap_elem_t *(get_elem)(
        hexcollmap_t *collmap, trf_t *index)
){
    /* Returns true (1) or false (0), or 2 if caller should continue
    checking for a collision. */

    for(int r2 = 0; r2 < rot; r2++){
        if(!hexcollmap_elem_is_solid(&elems2[r2]))continue;

        trf_t index;
        hexspace_set(index.add, x, y);
        index.rot = r2;
        index.flip = false;

        /* And now, because we were fools and defined */
        /* the tile coords such that their Y is flipped */
        /* compared to vecspaces, we need to flip that Y */
        /* before calling trf_apply and then flip it back */
        /* again: */
        index.add[1] = -index.add[1];
        trf_apply(space, &index, trf);
        index.add[1] = -index.add[1];
        normalize_elem(&index);

        bool collide = false;
        hexcollmap_elem_t *elem = get_elem(collmap1, &index);
        collide = hexcollmap_elem_is_solid(elem);

        if((all && !collide) || (!all && collide))return collide;
    }
    return 2; /* Caller should keep looking for a collision */
}

bool hexcollmap_collide(
    hexcollmap_t *collmap1, trf_t *trf1,
    hexcollmap_t *collmap2, trf_t *trf2,
    vecspace_t *space, bool all
){
    int ox2 = collmap2->ox;
    int oy2 = collmap2->oy;
    int w2 = collmap2->w;
    int h2 = collmap2->h;

    //return false;

    /* NOTE: for tile coords (ox, oy, x, y, w, h),
    Y is reversed (down is positive, up is negative) */

    trf_t trf;
    trf_cpy(space, &trf, trf2);
    trf_apply_inv(space, &trf, trf1);

    for(int y2 = 0; y2 < h2; y2++){
        for(int x2 = 0; x2 < w2; x2++){
            hexcollmap_tile_t *tile2 = &collmap2->tiles[y2 * w2 + x2];

            int collide;
            int x = x2 - ox2;
            int y = y2 - oy2;
            collide = hexcollmap_collide_elem(collmap1, all,
                space, x, y, &trf,
                tile2->vert, 1,
                hexcollmap_normalize_vert,
                hexcollmap_get_vert);
            if(collide != 2)return collide;
            collide = hexcollmap_collide_elem(collmap1, all,
                space, x, y, &trf,
                tile2->edge, 3,
                hexcollmap_normalize_edge,
                hexcollmap_get_edge);
            if(collide != 2)return collide;
            collide = hexcollmap_collide_elem(collmap1, all,
                space, x, y, &trf,
                tile2->face, 2,
                hexcollmap_normalize_face,
                hexcollmap_get_face);
            if(collide != 2)return collide;
        }
    }
    if(all)return true;
    else return false;
}

