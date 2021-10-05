

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "str_utils.h"
#include "file_utils.h"
#include "hexcollmap.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "vars.h"
#include "var_utils.h"
#include "mathutil.h"
#include "geom.h"
#include "hexspace.h"
#include "hexbox.h"


static void _debug_print_line(const char *line, int x, int y){
    fprintf(stderr, "%s\n", line);

    for(int i = 0; i < x; i++)fputc(' ', stderr);
    fputs("^\n", stderr);

    fprintf(stderr, "hexcollmap_parse: [line=%i char=%i] ", y+1, x+1);
}

static int _parse_trf(fus_lexer_t *lexer, vecspace_t *space,
    trf_t *trf, int *draw_z_ptr
){
    /* NOTE: Caller guarantees trf is zeroed */
    /* TODO: use trf_rot, trf_flip, etc -- instead of just twiddling trf's
    fields directly as we currently do... */
    INIT
    while(1){
        if(GOT("^")){
            NEXT
            GET_INT(trf->rot)
        }else if(GOT("~")){
            trf->flip = !trf->flip;
        }else if(GOT("|")){
            NEXT
            GET_INT(*draw_z_ptr)
        }else if(GOT("move")){
            GET_VEC(space, trf->add)
        }else break;
    }
    return 0;
}

void hexcollmap_vert_rot(
    int *x_ptr, int *y_ptr, int *i_ptr, rot_t addrot
){
    /* Does nothing!.. unlike hexcollmap_edge/face_rot, leaves its inputs
    unchaged.
    But we include this function so we can refer to hexcollmap_##PART##_rot
    in a macro, even when PART is "vert". */
}

void hexcollmap_edge_rot(
    int *x_ptr, int *y_ptr, int *i_ptr, rot_t addrot
){
    int x = *x_ptr;
    int y = *y_ptr;
    rot_t rot_from = *i_ptr;
    rot_t rot_to = rot_contain(HEXSPACE_ROT_MAX, rot_from + addrot);

    static const int leupsticoedophaeia[6][3] = {
        /*

            This table uses the following data structure:

            {x, y, i}

            ...to represent the following diagram:

               \ /
            . -(.)-
               / \
              .   .

            ...where the x, y axes are:

            (.)- . X
              \
               .
                Y

            ...and i is an index into the 3 edges stored at each coordinate:

            2   1
             \ /
             (.)- 0

        */
        { 0,  0, 0},
        { 0,  0, 1},
        { 0,  0, 2},
        {-1,  0, 0},
        {-1,  1, 1},
        { 0,  1, 2}
    };

    const int *leupsticoedophum_from = leupsticoedophaeia[rot_from];
    const int *leupsticoedophum_to = leupsticoedophaeia[rot_to];
    *x_ptr = x - leupsticoedophum_from[0] + leupsticoedophum_to[0];
    *y_ptr = y - leupsticoedophum_from[1] + leupsticoedophum_to[1];
    *i_ptr = leupsticoedophum_to[2];
}

void hexcollmap_face_rot(
    int *x_ptr, int *y_ptr, int *i_ptr, rot_t addrot
){
    int x = *x_ptr;
    int y = *y_ptr;
    rot_t rot_from = *i_ptr;
    rot_t rot_to = rot_contain(HEXSPACE_ROT_MAX, rot_from + addrot);

    static const int hypsofactodontaseri[6][3] = {
        /*

            This table uses the following data structure:

            {x, y, i}

            ...to represent the following diagram:

              * * *
            .  (.)
              * * *
              .   .

            ...where the x, y axes are:

            (.)- . X
              \
               .
                Y

            ...and i is an index into the 2 faces stored at each coordinate:

              1 0
              * *
             (.)

        */
        { 0,  0, 0},
        { 0,  0, 1},
        {-1,  0, 0},
        {-1,  1, 1},
        {-1,  1, 0},
        { 0,  1, 1}
    };

    const int *hypsofactodontaserus_from = hypsofactodontaseri[rot_from];
    const int *hypsofactodontaserus_to = hypsofactodontaseri[rot_to];
    *x_ptr = x - hypsofactodontaserus_from[0] + hypsofactodontaserus_to[0];
    *y_ptr = y - hypsofactodontaserus_from[1] + hypsofactodontaserus_to[1];
    *i_ptr = hypsofactodontaserus_to[2];
}

static char get_elem_type(char c){
    switch(c){
        case '.':
        case '+':
        case '?':
        case '!':
            return '+';
        case '-':
        case '/':
        case '\\':
            return c;
        case '*':
        case 'S':
        case 'D':
        case 'w':
            return '*';
        case ' ':
        case '(':
        case ')':
        case '%':
        case 'x':
            /* Valid, but unknown whether vert, edge, or face, or none */
            return ' ';
        default:
            /* Invalid */
            return '\0';
    }
}
static bool elem_is_visible(char c){
    return strchr("+!-/\\*SDw", c);
}
static bool represents_vert(char c){return get_elem_type(c) == '+';}
static bool represents_edge(char c){return strchr("-/\\", get_elem_type(c)) != NULL;}
static bool represents_face(char c){return get_elem_type(c) == '*';}

static char get_map_elem_type(int x, int y){
    /* A poorly-named function which figures out whether a vert, edge, or
    face is at the given hexcollmap coordinates.
    This is what a tile looks like in the hexcollmap text format:
         .   .
          \1/0
          (+)- .
    */

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

static void get_map_coords(int x, int y, char elem_type,
    int *mx_ptr, int *my_ptr, bool *is_face1_ptr
){
    /* Given (x, y) coords and elem_type (one of '+' '-' '/' '\\') within
    the text representation (lines, etc) of a collmap, return the map coords
    (mx, my) of the corresponding tile's vertex.
    This is what a tile looks like in the hexcollmap text format:
         .   .
          \1/0
          (+)- .

    ...where 0 and 1 represent the faces (which have index 0 and 1).
    If elem_type == '*', we set *is_face1_ptr to true if the face in
    question was face 1, else false.
    */

    /* NOTE: elem_type must be one of: '+' '-' '/' '\\' */

    bool is_face1 = false;

    /* Step 1: find x, y of vertex */
    if(elem_type == '+'){
    }else if(elem_type == '-'){
        x -= 2;
    }else if(elem_type == '/'){
        x -= 1;
        y += 1;
    }else if(elem_type == '\\'){
        x += 1;
        y += 1;
    }else if(elem_type == '*'){
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

static void update_hexbox(hexbox_t *hexbox, int x, int y, char elem_type) {
    /* Given (x, y) coords and elem_type (one of '+' '-' '/' '\\') within
    the text representation (lines, etc) of a collmap, update the given
    hexbox to contain the indicated element (vert/edge/face).
    This is what a tile looks like in the hexcollmap text format:
         .   .
          \1/0
          (+)- .
    */

    int mx, my;
    bool is_face1;
    get_map_coords(x, y, elem_type, &mx, &my, &is_face1);

    /* We were fools: tile coords have Y flipped compared
    to vecspaces... thus, vy = -my */
    int vx =  mx;
    int vy = -my;

    if(elem_type == '+'){
        hexbox_point_union(hexbox, vx    , vy    );
    }else if(elem_type == '-'){
        hexbox_point_union(hexbox, vx    , vy    );
        hexbox_point_union(hexbox, vx + 1, vy    );
    }else if(elem_type == '/'){
        hexbox_point_union(hexbox, vx    , vy    );
        hexbox_point_union(hexbox, vx + 1, vy + 1);
    }else if(elem_type == '\\'){
        hexbox_point_union(hexbox, vx    , vy    );
        hexbox_point_union(hexbox, vx    , vy + 1);
    }else if(elem_type == '*'){
        if(!is_face1){
            hexbox_point_union(hexbox, vx    , vy    );
            hexbox_point_union(hexbox, vx + 1, vy    );
            hexbox_point_union(hexbox, vx + 1, vy + 1);
        }else{
            hexbox_point_union(hexbox, vx    , vy    );
            hexbox_point_union(hexbox, vx + 1, vy + 1);
            hexbox_point_union(hexbox, vx    , vy + 1);
        }
    }
}

static int hexcollmap_draw(hexcollmap_t *collmap1, hexcollmap_t *collmap2,
    trf_t *trf, int draw_z
){
    /* Draw collmap2 onto collmap1 */

    int err;

    /* Resize collmap1 */
    hexbox_t hexbox2 = collmap2->hexbox;
    hexbox_apply(&hexbox2, trf);
    err = hexcollmap_union_hexbox(collmap1, &hexbox2);
    if(err)return err;

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

    /* "Draw" recordings from collmap2 onto collmap1, in other words copy
    them while adjusting recording->trf appropriately */
    for(int i = 0; i < collmap2->recordings_len; i++){
        hexmap_recording_t *recording2 = collmap2->recordings[i];

        char *filename = recording2->filename?
            strdup(recording2->filename): NULL;
        char *palmapper_name = recording2->palmapper_name?
            strdup(recording2->palmapper_name): NULL;

        ARRAY_PUSH_NEW(hexmap_recording_t*, collmap1->recordings,
            recording1)
        err = hexmap_recording_init(recording1, recording2->type,
            filename, palmapper_name, recording2->frame_offset);
        if(err)return err;
        trf_apply(space, &recording1->trf, trf);

        err = valexpr_copy(&recording1->visible_expr, &recording2->visible_expr);
        if(err)return err;
        recording1->visible_not = recording2->visible_not;

        err = vars_copy(&recording1->vars, &recording2->vars);
        if(err)return err;
        err = vars_copy(&recording1->bodyvars, &recording2->bodyvars);
        if(err)return err;
    }

    /* "Draw" rendergraphs from collmap2 onto collmap1, in other words copy
    them while adjusting rendergraph->trf appropriately */
    for(int i = 0; i < collmap2->rendergraphs_len; i++){
        hexmap_rendergraph_t *rendergraph2 = collmap2->rendergraphs[i];

        char *name = rendergraph2->name?
            strdup(rendergraph2->name): NULL;
        char *palmapper_name = rendergraph2->palmapper_name?
            strdup(rendergraph2->palmapper_name): NULL;

        ARRAY_PUSH_NEW(hexmap_rendergraph_t*, collmap1->rendergraphs,
            rendergraph1)
        err = hexmap_rendergraph_init(rendergraph1,
            name, palmapper_name);
        if(err)return err;
        trf_apply(space, &rendergraph1->trf, trf);
    }

    for(int i = 0; i < collmap2->text_exprs_len; i++){
        valexpr_t *text_expr2 = collmap2->text_exprs[i];
        ARRAY_PUSH_NEW(valexpr_t*, collmap1->text_exprs, text_expr1)
        err = valexpr_copy(text_expr1, text_expr2);
        if(err)return err;
    }

    return 0;
}

static int hexcollmap_draw_part(hexcollmap_t *collmap,
    hexcollmap_part_t *part, trf_t *trf, int draw_z
){
    int err;
    vecspace_t *space = collmap->space;

    trf_t trf2;
    trf_cpy(collmap->space, &trf2, &part->trf);
    trf_apply(collmap->space, &trf2, trf);
    draw_z += part->draw_z;

    if(part->type == HEXCOLLMAP_PART_TYPE_HEXCOLLMAP){
        /* If "empty" was specified for this part, then filename will
        be NULL and we shouldn't do anything. */
        if(part->filename == NULL)return 0;

        hexcollmap_t part_collmap;
        hexcollmap_init(&part_collmap,
            collmap->space, strdup(part->filename));
        err = hexcollmap_load(&part_collmap,
            part->filename, NULL);
        if(err)return err;
        err = hexcollmap_draw(collmap, &part_collmap,
            &trf2, draw_z);
        if(err)return err;
        hexcollmap_cleanup(&part_collmap);
    }else if(
        part->type == HEXCOLLMAP_PART_TYPE_RECORDING ||
        part->type == HEXCOLLMAP_PART_TYPE_ACTOR
    ){
        char *filename = part->filename?
            strdup(part->filename): NULL;
        char *palmapper_name = part->palmapper_name?
            strdup(part->palmapper_name): NULL;
        ARRAY_PUSH_NEW(hexmap_recording_t*, collmap->recordings,
            recording)
        err = hexmap_recording_init(recording,
            part->type == HEXCOLLMAP_PART_TYPE_RECORDING?
                HEXMAP_RECORDING_TYPE_RECORDING:
                HEXMAP_RECORDING_TYPE_ACTOR,
            filename, palmapper_name, part->frame_offset);
        if(err)return err;

        /* hexmap_recording_init set up recording->trf already, and now
        we modify it by applying trf2 to it */
        trf_apply(space, &recording->trf, &trf2);

        err = valexpr_copy(&recording->visible_expr, &part->visible_expr);
        if(err)return err;
        recording->visible_not = part->visible_not;

        err = vars_copy(&recording->vars, &part->vars);
        if(err)return err;
        err = vars_copy(&recording->bodyvars, &part->bodyvars);
        if(err)return err;
    }else if(part->type == HEXCOLLMAP_PART_TYPE_RENDERGRAPH){
        char *filename = part->filename?
            strdup(part->filename): NULL;
        char *palmapper_name = part->palmapper_name?
            strdup(part->palmapper_name): NULL;
        ARRAY_PUSH_NEW(hexmap_rendergraph_t*, collmap->rendergraphs,
            rendergraph)
        err = hexmap_rendergraph_init(rendergraph,
            filename, palmapper_name);
        if(err)return err;
        trf_cpy(space, &rendergraph->trf, &trf2);
    }else{
        fprintf(stderr, "Unrecognized part type: %i\n", part->type);
        return 2;
    }

    return 0;
}

static int _hexcollmap_parse_lines_origin(
    char **lines, int lines_len, int *ox_ptr, int *oy_ptr
){
    int err;
    int ox = -1;
    int oy = -1;
    for(int y = 0; y < lines_len; y++){
        char *line = lines[y];
        int line_len = strlen(line);
        for(int x = 0; x < line_len; x++){
            char c = line[x];
            char elem_type = get_elem_type(c);
            if(c == '('){
                if(x+2 >= line_len || line[x+2] != ')'){
                    _debug_print_line(line, x, y);
                    fprintf(stderr, "'(' without matching ')'.\n");
                    return 2;
                }
                if(oy != -1){
                    _debug_print_line(line, x, y);
                    fprintf(stderr, "another '('.");
                    return 2;
                }
                ox = x + 1;
                oy = y;
                x += 2;
            }else if(c == '[' || c == ';'){
                /* next line plz, "tilebuckets" don't affect the origin */
                break;
            }else if(!elem_type){
                _debug_print_line(line, x, y);
                fprintf(stderr, "unexpected character (%c).",
                    isgraph(c)? c: ' ');
                return 2;
            }
        }
    }
    if(ox == -1){
        fprintf(stderr, "Missing origin (indicate it with \"( )\" "
            "around a vertex).\n");
        return 2;
    }
    *ox_ptr = ox;
    *oy_ptr = oy;
    return 0;
}

static int _hexcollmap_parse_lines_hexbox(
    char **lines, int lines_len, int ox, int oy,
    hexbox_t *hexbox
){
    /* Sets the values of hexbox */

    int err;
    hexbox_zero(hexbox);
    for(int y = 0; y < lines_len; y++){
        char *line = lines[y];
        int line_len = strlen(line);
        for(int x = 0; x < line_len; x++){
            char c = line[x];
            char elem_type = get_elem_type(c);
            if(elem_type && elem_type != ' '){
                char map_elem_type = get_map_elem_type(x-ox, y-oy);
                if(elem_type != map_elem_type){
                    _debug_print_line(line, x, y);
                    fprintf(stderr,
                        "character doesn't belong at these coordinates: "
                        "got [%c] (type [%c]), expected type [%c]\n",
                        c, elem_type, map_elem_type);
                    return 2;}

                update_hexbox(hexbox, x-ox, y-oy, elem_type);
            }else if(c == '[' || c == ';'){
                /* next line plz, "tilebuckets" don't affect bounds */
                break;
            }
        }
    }
    return 0;
}

static int _hexcollmap_parse_lines_tiles(hexcollmap_t *collmap,
    char **lines, int lines_len, hexcollmap_part_t **parts, int parts_len,
    char default_vert_c, char default_edge_c, char default_face_c,
    int ox, int oy, bool parsing_part_references
){
    /* If !parsing_part_references, we parse regular tile data.
    If parsing_part_references, we parse "part references", that is,
    the '?' character, which loads & draws other collmaps over the
    tile data we parsed while !parsing_part_references. */
    int err;

    int map_w = collmap->w;
    int map_h = collmap->h;
    int map_l = -collmap->ox;
    int map_t = -collmap->oy;
    hexcollmap_tile_t *tiles = collmap->tiles;

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
            that instead of modifying tile_c, it loads and draws other
            collmaps over this one, according to the "parts" indicated
            at the top of the file.
            Also, '!' is like a combination of '+' and '?'.

            TODO: Clarify this whole comment... */

        for(int x = 0; x < line_len; x++){
            char c = line[x];
            if(elem_is_visible(c) || c == 'x'){
                char elem_type = c == 'x'?
                    get_map_elem_type(x-ox, y-oy):
                    get_elem_type(c);

                bool is_savepoint = c == 'S';
                bool is_door = c == 'D';
                bool is_water = c == 'w';
                bool is_hard_transparent = c == 'x';

                char tile_c =
                    is_savepoint? 'S':
                    is_door? 'D':
                    is_water? 'w':
                    is_hard_transparent? 'x':
                    represents_vert(c)? default_vert_c:
                    represents_edge(c)? default_edge_c:
                    represents_face(c)? default_face_c:
                    ' ';
                    /* NOTE: The way we've implemented this, 'S' and 'D'
                    can be overwritten by '%'. Maybe that's weird? Maybe if
                    is_savepoint or is_door then we should skip the check
                    for tilebucket_active entirely? */

                int draw_z = 0;

                if(tilebucket_active){
                    /* Get next non-' ' character in current tile bucket. */
                    char c2;
                    while(c2 = *tilebucket, c2 == ' ')tilebucket++;
                    if(c2 == ']'){
                        tilebucket_active = false;
                    }else if(!isprint(c2)){
                        _debug_print_line(line, x, y);
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
                    int mx, my; bool is_face1;
                    get_map_coords(x-ox, y-oy, elem_type,
                        &mx, &my, &is_face1);
                    mx -= map_l;
                    my -= map_t;
                    hexcollmap_tile_t *tile = &tiles[my * map_w + mx];

                    hexcollmap_elem_t *elem = NULL;
                    if(elem_type == '+'){
                        elem = &tile->vert[0];
                    }else if(elem_type == '-'){
                        elem = &tile->edge[0];
                    }else if(elem_type == '/'){
                        elem = &tile->edge[1];
                    }else if(elem_type == '\\'){
                        elem = &tile->edge[2];
                    }else if(elem_type == '*'){
                        elem = &tile->face[is_face1? 1: 0];
                    }
                    if(elem != NULL){
                        /* We don't expect elem to be NULL, but it never
                        hurts to check */
                        elem->tile_c = tile_c;
                        elem->z = draw_z;
                    }
                }
            }

            if(c == '%' || c == '?' || c == '!'){

                /* Find next tile bucket. */
                char c2;
                if(tilebucket == NULL){
                    tilebucket = &line[x+1];
                    while(c2 = *tilebucket, c2 != '\0' && c2 != '['){
                        tilebucket++;}
                    if(c2 != '['){
                        _debug_print_line(line, x, y);
                        fprintf(stderr, "Didn't find '[' in line\n");
                        return 2;}
                    tilebucket++;
                }else{
                    while(c2 = *tilebucket, c2 == ' ')tilebucket++;
                    if(c2 != ']'){
                        _debug_print_line(line, x, y);
                        fprintf(stderr, "Expected ']', got '%c'\n", c2);
                        return 2;}
                    tilebucket++;
                    while(c2 = *tilebucket, c2 == ' ')tilebucket++;
                    if(c2 != '['){
                        _debug_print_line(line, x, y);
                        fprintf(stderr, "Expected '[', got '%c'\n", c2);
                        return 2;}
                    tilebucket++;
                }

                if(c == '%'){
                    /* Activate the bucket we found. */
                    tilebucket_active = true;
                }else if(c == '?' || c == '!'){
                    /* Load collmaps from the bucket we found and draw them
                    onto current collmap. */

                    /* loop through all characters in tilebucket */
                    while(1){
                        /* Get next non-' ' character in current tile bucket. */
                        char c2;
                        while(c2 = *tilebucket, c2 == ' ')tilebucket++;

                        if(c2 == ']')break;

                        if(!isprint(c2)){
                            _debug_print_line(line, x, y);
                            if(c2 == '\0'){
                                fprintf(stderr, "Hit end of line\n");}
                            else{fprintf(stderr,
                                "Hit unprintable character: %x\n", c2);}
                            return 2;
                        }

                        tilebucket++;

                        if(parsing_part_references){
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

                            /* Find and draw parts with given part_c */
                            bool found = false;
                            for(int i = 0; i < parts_len; i++){
                                hexcollmap_part_t *part = parts[i];
                                if(part->part_c != c2)continue;
                                found = true;
                                err = hexcollmap_draw_part(collmap,
                                    part, &trf, draw_z);
                                if(err)return err;
                            }
                            if(!found){
                                _debug_print_line(line, x, y);
                                fprintf(stderr, "part not found: %c\n", c2);
                                return 2;
                            }

                        }
                    }
                }

            }else if(c == ';'){
                /* Explicit end of tile data */
                break;
            }else if(c == '['){
                /* We hit a tilebucket, so no more regular tile data on
                this line. Next plz! */
                break;
            }
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
    int ox, oy;
    err = _hexcollmap_parse_lines_origin(lines, lines_len, &ox, &oy);
    if(err)return err;

    /* Iteration 2: Find map bounds (set collmap->hexbox) */
    err = _hexcollmap_parse_lines_hexbox(lines, lines_len,
        ox, oy, &collmap->hexbox);
    if(err)return err;

    /* Intermission: initialize collmap with empty tile data */
    err = hexcollmap_init_tiles_from_hexbox(collmap);
    if(err)return err;

    /* Iterations 3 & 4: The meat of it all - parse tile data */
    for(int iter_i = 0; iter_i < 2; iter_i++){
        bool parsing_part_references = iter_i == 1;
        err = _hexcollmap_parse_lines_tiles(
            collmap, lines, lines_len, parts, parts_len,
            default_vert_c, default_edge_c, default_face_c,
            ox, oy, parsing_part_references);
        if(err)return err;
    }

    /* OKAY */
    return 0;
}

static int _hexcollmap_parse_part(hexcollmap_t *collmap,
    hexcollmap_part_t *part, fus_lexer_t *lexer
){
    /* Parses part data from lexer and initializes part if successful. */
    int err;

    char part_c;
    GET_CHR(part_c)

    int type = HEXCOLLMAP_PART_TYPE_HEXCOLLMAP;
    char *filename = NULL;
    char *palmapper_name = NULL;
    int frame_offset = 0;
    trf_t trf = {0};
    int draw_z = 0;

    valexpr_t visible_expr;
    valexpr_set_literal_bool(&visible_expr, true);
    bool visible_not = false;

    vars_t vars;
    vars_init(&vars);
    vars_t bodyvars;
    vars_init(&bodyvars);

    GET("(")
    {
        if(GOT("recording")){
            NEXT
            type = HEXCOLLMAP_PART_TYPE_RECORDING;
        }else if(GOT("actor")){
            NEXT
            type = HEXCOLLMAP_PART_TYPE_ACTOR;
        }else if(GOT("shape")){
            NEXT
            type = HEXCOLLMAP_PART_TYPE_RENDERGRAPH;
        }

        if(GOT("empty")){
            NEXT
        }else{
            GET_STR(filename)
            err = _parse_trf(lexer, collmap->space, &trf, &draw_z);
            if(err)return err;
        }

        if(!GOT(")") && !GOT("visible") && !GOT("vars") && !GOT("bodyvars")){
            if(GOT("empty")){
                NEXT
            }else{
                GET_STR(palmapper_name)
            }
            if(GOT_INT){
                GET_INT(frame_offset)
            }
        }

        if(GOT("visible")){
            NEXT
            GET("(")
            if(GOT("not")){
                NEXT
                visible_not = true;
            }
            err = valexpr_parse(&visible_expr, lexer);
            if(err)return err;
            GET(")")
        }

        if(GOT("vars")){
            NEXT
            GET("(")
            err = vars_parse(&vars, lexer);
            if(err)return err;
            GET(")")
        }

        if(GOT("bodyvars")){
            NEXT
            GET("(")
            err = vars_parse(&bodyvars, lexer);
            if(err)return err;
            GET(")")
        }
    }
    GET(")")

    err = hexcollmap_part_init(part, type, part_c,
        filename, palmapper_name, frame_offset,
        &visible_expr, visible_not, &vars, &bodyvars);
    if(err)return err;
    trf_cpy(collmap->space, &part->trf, &trf);
    part->draw_z = draw_z;
    return 0;
}

static int _hexcollmap_parse_with_parts(hexcollmap_t *collmap, fus_lexer_t *lexer,
    char default_vert_c, char default_edge_c, char default_face_c,
    hexcollmap_part_t **parts, int parts_len
){
    int err;

    /* set up dynamic array of lines */
    int collmap_lines_len = 0;
    int collmap_lines_size = 8;
    char **collmap_lines = calloc(collmap_lines_size,
        sizeof(*collmap_lines));
    if(collmap_lines == NULL)return 1;

    /* read in lines */
    while(GOT_STR){
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
        GET_STR(collmap_lines[collmap_lines_len - 1])
    }

    /* parse lines */
    err = hexcollmap_parse_lines(collmap,
        collmap_lines, collmap_lines_len, parts, parts_len,
        default_vert_c, default_edge_c, default_face_c);
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

int hexcollmap_parse_with_parts(hexcollmap_t *collmap, fus_lexer_t *lexer,
    bool just_coll,
    hexcollmap_part_t ***parts_ptr, int *parts_len_ptr
){
    int err;

    char default_vert_c = '0';
    char default_edge_c = '0';
    char default_face_c = '0';

    ARRAY_DECL(hexcollmap_part_t*, parts)
    ARRAY_INIT(parts)

    if(just_coll){
        err = _hexcollmap_parse_with_parts(collmap, lexer,
            default_vert_c, default_edge_c, default_face_c,
            parts, parts_len);
        if(err)return err;
    }else{
        if(GOT("spawn")){
            NEXT
            GET("(")
            err = hexgame_location_parse(&collmap->spawn, lexer);
            if(err)return err;
            GET(")")
        }

        while(GOT("text")){
            NEXT
            GET("(")
            ARRAY_PUSH_NEW(valexpr_t*, collmap->text_exprs, text_expr)
            err = valexpr_parse(text_expr, lexer);
            if(err)return err;
            GET(")")
        }

        if(GOT("parts")){
            NEXT
            GET("(")
            while(!GOT(")")){
                ARRAY_PUSH_NEW(hexcollmap_part_t*, parts, part)
                err = _hexcollmap_parse_part(collmap, part, lexer);
                if(err)return err;
            }
            NEXT
        }

        if(GOT("default_vert")){
            NEXT
            GET("(")
            GET_CHR(default_vert_c)
            GET(")")
        }

        if(GOT("default_edge")){
            NEXT
            GET("(")
            GET_CHR(default_edge_c)
            GET(")")
        }

        if(GOT("default_face")){
            NEXT
            GET("(")
            GET_CHR(default_face_c)
            GET(")")
        }

        if(GOT("import")){
            NEXT

            /* We use _fus_lexer_get_str to avoid calling fus_lexer_next until after
            the call to fus_lexer_init_with_vars is done, to make sure we don't modify
            lexer->vars first */
            char *filename;
            err = _fus_lexer_get_str(lexer, &filename);
            if(err)return err;

            char *text = load_file(filename);
            if(text == NULL)return 1;

            fus_lexer_t sublexer;
            err = fus_lexer_init_with_vars(&sublexer, text, filename,
                lexer->vars);
            if(err)return err;

            err = _hexcollmap_parse_with_parts(collmap, &sublexer,
                default_vert_c, default_edge_c, default_face_c,
                parts, parts_len);
            if(err)return err;

            /* We now call fus_lexer_next manually, see call to _fus_lexer_get_str
            above */
            err = fus_lexer_next(lexer);
            if(err)return err;

            fus_lexer_cleanup(&sublexer);
            free(filename);
            free(text);
        }else{
            GET("collmap")
            GET("(")
            err = _hexcollmap_parse_with_parts(collmap, lexer,
                default_vert_c, default_edge_c, default_face_c,
                parts, parts_len);
            if(err)return err;
            GET(")")
        }

        if(GOT("draw")){
            NEXT
            GET("(")
            while(!GOT(")")){
                GET("(")

                char part_c;
                GET_CHR(part_c)
                trf_t trf = {0};
                GET_TRF(collmap->space, trf)

                int draw_z = 0;
                if(GOT("|")){
                    NEXT
                    GET_INT(draw_z)
                }

                /* Find and draw parts with given part_c */
                bool found = false;
                for(int i = 0; i < parts_len; i++){
                    hexcollmap_part_t *part = parts[i];
                    if(part->part_c != part_c)continue;
                    found = true;
                    err = hexcollmap_draw_part(collmap,
                        part, &trf, draw_z);
                    if(err)return err;
                }
                if(!found){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr, "part not found: %c\n", part_c);
                    return 2;
                }

                GET(")")
            }
            NEXT
        }
    }

    *parts_ptr = parts;
    *parts_len_ptr = parts_len;
    return 0;
}


int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer,
    bool just_coll
){
    int err;

    ARRAY_DECL(hexcollmap_part_t*, parts)
    /* NOTE: we don't ARRAY_INIT parts, because it's completely overwritten by
    hexcollmap_parse_with_parts on successful return */
    err = hexcollmap_parse_with_parts(collmap, lexer, just_coll,
        &parts, &parts_len);
    if(err)return err;
    ARRAY_FREE_PTR(hexcollmap_part_t*, parts, hexcollmap_part_cleanup)

    return 0;
}

int hexcollmap_clone(hexcollmap_t *collmap,
    hexcollmap_t *from_collmap, rot_t rot
){
    int err;

    hexbox_rot(&collmap->hexbox, rot);

    err = hexcollmap_init_tiles_from_hexbox(collmap);
    if(err)return err;

    for(int from_y = 0; from_y < from_collmap->h; from_y++){
        // \ /
        //  . -

        int from_vy = from_collmap->oy - from_y;
        for(int from_x = 0; from_x < from_collmap->w; from_x++){
            int from_vx = from_x - from_collmap->ox;

            /*
            (from_x, from_y): coordinates into the from_collmap->tiles array
            (from_vx, from_vy): coordinates in (X, Y) space of from_collmap
            */

            hexcollmap_tile_t *from_tile =
                &from_collmap->tiles[from_y * from_collmap->w + from_x];

            vec_t v = {from_vx, from_vy};
            hexspace_rot(v, rot);

            int x = collmap->ox + v[0];
            int y = collmap->oy - v[1];

#           define _PARSE_PART(PART, MAX_I) \
            for(int i = 0; i < MAX_I; i++){ \
                int to_x = x, to_y = y, to_i = i; \
                hexcollmap_##PART##_rot(&to_x, &to_y, &to_i, rot); \
                hexcollmap_tile_t *tile = hexcollmap_get_tile_xy( \
                    collmap, to_x, to_y); \
                if(!tile)continue; \
                tile->PART[to_i] = from_tile->PART[i]; \
            }
            _PARSE_PART(vert, 1)
            _PARSE_PART(edge, 3)
            _PARSE_PART(face, 2)
#           undef _PARSE_PART
        }
    }

    return 0;
}
