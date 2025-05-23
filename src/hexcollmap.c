

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "str_utils.h"
#include "file_utils.h"
#include "hexcollmap.h"
#include "lexer.h"
#include "vars.h"
#include "geom.h"
#include "hexspace.h"
#include "hexgame_vars_props.h"


/************************
 * HELPERS & ALGORITHMS *
 ***********************/

void hexcollmap_normalize_vert(trf_t *index){
    /* Normalizes the given trf_t so that it can be used to get a vertex,
    according to the following logic:

        int x = index->add[0];
        int y = index->add[1];
        int i = index->rot;
        ...index->flip is unused, we "normalize" it to false...
        hexcollmap_tile_t *tile = &collmap->tiles[y * collmap->w + x];
        return tile->vert[i];

    PROBABLY TODO: get rid of these "normalization" functions and just
    add get_vert/edge/face functions which do this.
    */
    index->rot = 0;
    index->flip = false;
}

void hexcollmap_normalize_edge(trf_t *index){
    /* See hexcollmap_normalize_vert */
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
    /* See hexcollmap_normalize_vert */
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


/********************
 * HEXMAP RECORDING *
 ********************/

void hexmap_recording_cleanup(hexmap_recording_t *recording){
    valexpr_cleanup(&recording->visible_expr);
    valexpr_cleanup(&recording->target_expr);
    vars_cleanup(&recording->vars);
    vars_cleanup(&recording->bodyvars);
}

void hexmap_recording_init(hexmap_recording_t *recording, int type,
    const char *filename, const char *palmapper_name, int frame_offset
){
    recording->type = type;
    recording->filename = filename;
    recording->palmapper_name = palmapper_name;
    recording->frame_offset = frame_offset;
    trf_zero(&recording->trf);

    valexpr_set_literal_bool(&recording->visible_expr, true);
    valexpr_set_literal_bool(&recording->target_expr, false);

    vars_init_with_props(&recording->vars, hexgame_vars_prop_names);
    vars_init_with_props(&recording->bodyvars, hexgame_vars_prop_names);
}

int hexmap_recording_clone(hexmap_recording_t *recording1,
    hexmap_recording_t *recording2
){
    int err;

    hexmap_recording_init(recording1, recording2->type,
        recording2->filename, recording2->palmapper_name,
        recording2->frame_offset);

    recording1->trf = recording2->trf;

    err = valexpr_copy(&recording1->visible_expr, &recording2->visible_expr);
    if(err)return err;
    err = valexpr_copy(&recording1->target_expr, &recording2->target_expr);
    if(err)return err;

    err = vars_copy(&recording1->vars, &recording2->vars);
    if(err)return err;
    err = vars_copy(&recording1->bodyvars, &recording2->bodyvars);
    if(err)return err;
    return 0;
}


/**********************
 * HEXMAP RENDERGRAPH *
 **********************/

void hexmap_rendergraph_cleanup(hexmap_rendergraph_t *rendergraph){
    /* Nuthin */
}

void hexmap_rendergraph_init(hexmap_rendergraph_t *rendergraph,
    const char *name, const char *palmapper_name
){
    rendergraph->name = name;
    rendergraph->palmapper_name = palmapper_name;
    trf_zero(&rendergraph->trf);
}


/*******************
 * HEXMAP LOCATION *
 *******************/

void hexmap_location_cleanup(hexmap_location_t *location){
    /* Nuthin */
}

void hexmap_location_init(hexmap_location_t *location, const char *name){
    location->name = name;
    memset(&location->loc, 0, sizeof(location->loc));
}


/**************
 * HEXCOLLMAP *
 **************/

void hexcollmap_part_init(hexcollmap_part_t *part, int type,
    char part_c, const char *filename, const char *palmapper_name,
    int frame_offset, valexpr_t *visible_expr, valexpr_t *target_expr,
    vars_t *vars, vars_t *bodyvars
){
    part->type = type;
    part->part_c = part_c;
    part->filename = filename;
    part->palmapper_name = palmapper_name;
    part->frame_offset = frame_offset;

    vec_zero(part->trf.add);
    part->trf.rot = 0;
    part->trf.flip = false;

    /* NOTE: we take ownership of *visible_expr and *target_expr from caller */
    part->visible_expr = *visible_expr;
    part->target_expr = *target_expr;

    /* vars_t must guarantee that it can be freely copied.
    Which it does. (See the comment in definition of vars_t.) */
    part->vars = *vars;
    part->bodyvars = *bodyvars;
}

void hexcollmap_part_cleanup(hexcollmap_part_t *part){
    valexpr_cleanup(&part->visible_expr);
    valexpr_cleanup(&part->target_expr);
    vars_cleanup(&part->vars);
    vars_cleanup(&part->bodyvars);
}


void hexcollmap_cleanup(hexcollmap_t *collmap){
    free(collmap->tiles);
    ARRAY_FREE_PTR(hexmap_recording_t*, collmap->recordings,
        hexmap_recording_cleanup)
    ARRAY_FREE_PTR(hexmap_rendergraph_t*, collmap->rendergraphs,
        hexmap_rendergraph_cleanup)
    ARRAY_FREE_PTR(hexmap_location_t*, collmap->locations,
        hexmap_location_cleanup)
    ARRAY_FREE_PTR(valexpr_t*, collmap->text_exprs,
        valexpr_cleanup)
}

void hexcollmap_init_empty(hexcollmap_t *collmap){
    memset(collmap, 0, sizeof(*collmap));
}

void hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    const char *filename
){
    hexcollmap_init_empty(collmap);
    collmap->filename = filename;
    collmap->space = space;
    ARRAY_INIT(collmap->recordings);
    ARRAY_INIT(collmap->rendergraphs);
    ARRAY_INIT(collmap->text_exprs);
}

void hexcollmap_init_clone(hexcollmap_t *collmap,
    hexcollmap_t *from_collmap, const char *filename
){
    hexcollmap_init(collmap, from_collmap->space, filename);
    collmap->spawn = from_collmap->spawn;
    collmap->hexbox = from_collmap->hexbox;
}

int hexcollmap_init_tiles_from_hexbox(hexcollmap_t *collmap){
    int map_l = collmap->hexbox.values[HEXBOX_INDEX(HEXBOX_X, HEXBOX_MIN)];
    int map_r = collmap->hexbox.values[HEXBOX_INDEX(HEXBOX_X, HEXBOX_MAX)];
    int map_t = -collmap->hexbox.values[HEXBOX_INDEX(HEXBOX_Y, HEXBOX_MAX)];
    int map_b = -collmap->hexbox.values[HEXBOX_INDEX(HEXBOX_Y, HEXBOX_MIN)];

    /* The "+1" here are because e.g. map_r - map_l gives the map's width
    in *edges*, but we want to know the number of *vertices* we need.
    For instance, a map consisting of a single vertex only has a "width"
    of 0, yet we still need to allocate a vertex for it. */
    int map_w = map_r - map_l + 1;
    int map_h = map_b - map_t + 1;

    int map_size = map_w * map_h;

    /* ...Allocate map data */
    hexcollmap_tile_t *tiles = calloc(map_size, sizeof(*tiles));
    if(tiles == NULL)return 1;

    /* ...Initialize tile elements */
    hexcollmap_elem_t initial_elem = {
        .tile_c = ' ',
        .z = -99,
            /* So you can have tiles whose z is between this initial value and 0 */
    };
    for(int i = 0; i < map_size; i++){
        for(int j = 0; j < 1; j++)tiles[i].vert[j] = initial_elem;
        for(int j = 0; j < 3; j++)tiles[i].edge[j] = initial_elem;
        for(int j = 0; j < 2; j++)tiles[i].face[j] = initial_elem;
    }

    /* ...Assign attributes */
    collmap->ox = -map_l;
    collmap->oy = -map_t;
    collmap->w = map_w;
    collmap->h = map_h;
    collmap->tiles = tiles;

    return 0;
}

int hexcollmap_union_hexbox(hexcollmap_t *collmap, hexbox_t *hexbox){
    /* Expands collmap's hexbox by unioning it with given hexbox.
    Tile data is reallocated (and old data preserved). */
    int err;

    hexbox_t old_hexbox = collmap->hexbox;
    hexbox_union(&collmap->hexbox, hexbox);
    if(hexbox_eq(&old_hexbox, &collmap->hexbox)){
        /* Hexbox unchanged: nothing to change, so early exit! */
        return 0;
    }

    hexcollmap_tile_t *old_tiles = collmap->tiles;
    int old_ox = collmap->ox;
    int old_oy = collmap->oy;
    int old_w = collmap->w;
    int old_h = collmap->h;

    /* (re)initialize tiles etc */
    err = hexcollmap_init_tiles_from_hexbox(collmap);
    if(err)return err;

    /* Copy old tile data onto new tile array */
    for(int y = 0; y < old_h; y++){
        for(int x = 0; x < old_w; x++){
            int new_x = x - old_ox + collmap->ox;
            int new_y = y - old_oy + collmap->oy;
            hexcollmap_tile_t *old_tile =
                &old_tiles[old_w * y + x];
            hexcollmap_tile_t *new_tile =
                &collmap->tiles[new_y * collmap->w + new_x];
            *new_tile = *old_tile;
        }
    }

    free(old_tiles);
    return 0;
}

void hexcollmap_dump(hexcollmap_t *collmap, FILE *f){
    /* The rawest of dumps */
    fprintf(f, "hexcollmap: %p\n", collmap);
    if(collmap == NULL)return;
    fprintf(f, "  hexbox: (%i %i) (%i %i) (%i %i)\n",
        collmap->hexbox.values[0], collmap->hexbox.values[1],
        collmap->hexbox.values[2], collmap->hexbox.values[3],
        collmap->hexbox.values[4], collmap->hexbox.values[5]);
    fprintf(f, "  origin: %i %i\n", collmap->ox, collmap->oy);
    fprintf(f, "  tiles:\n");
    for(int y = 0; y < collmap->h; y++){
        fprintf(f, "    ");
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

int hexcollmap_load(hexcollmap_t *collmap, vecspace_t *space,
    const char *filename, vars_t *vars,
    stringstore_t *name_store, stringstore_t *filename_store
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = hexcollmap_parse(collmap, &lexer, space, filename, false,
        name_store, filename_store);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}

hexcollmap_tile_t *hexcollmap_get_tile_xy(hexcollmap_t *collmap,
    int x, int y
){
    if(x < 0 || x >= collmap->w || y < 0 || y >= collmap->h)return NULL;
    return &collmap->tiles[y * collmap->w + x];
}

hexcollmap_tile_t *hexcollmap_get_tile(hexcollmap_t *collmap, trf_t *index){
    int x = collmap->ox + index->add[0];
    int y = collmap->oy + index->add[1];
    return hexcollmap_get_tile_xy(collmap, x, y);
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

static int hexcollmap_collide_elem(hexcollmap_t *collmap1, bool all,
    vecspace_t *space, int x, int y, trf_t *trf,
    hexcollmap_elem_t *elems2, int rot,
    void (*normalize_elem)(trf_t *index),
    hexcollmap_elem_t *(get_elem)(
        hexcollmap_t *collmap, trf_t *index)
){
    /* Returns true (1) or false (0), or 2 if caller should continue
    checking for a collision. */

    /* TODO: can we do a fancy boundary check to see if we can early exit? */

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

bool hexcollmap_has_mappoint(hexcollmap_t *collmap){
    int tiles_len = collmap->w * collmap->h;
    for(int i = 0; i < tiles_len; i++){
        hexcollmap_tile_t *tile = &collmap->tiles[i];
        for(int j = 0; j < 2; j++){
            if(tile->face[j].tile_c == 'M')return true;
        }
    }
    return false;
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

    /* NOTE: for tile coords (ox, oy, x, y, w, h),
    Y is reversed (down is positive, up is negative) */

    /* TODO: can we do a fancy boundary check to see if we can early exit? */

    trf_t trf;
    trf_cpy(space, &trf, trf2);
    trf_apply_inv(space, &trf, trf1);

    for(int y2 = 0; y2 < h2; y2++){
        for(int x2 = 0; x2 < w2; x2++){
            hexcollmap_tile_t *tile2 = &collmap2->tiles[y2 * w2 + x2];

            int collide;
            int x = x2 - ox2;
            int y = y2 - oy2;

            /* TODO: REMOVE THE FOLLOWING FUNCTION CALLS, WHICH TAKE FUNCTION
            POINTERS, WITH A MACRO WHICH DOES E.G. hexcollmap_normalize_##PART
            INSTEAD OF CALLING FUNCTION POINTERS, SEE hexcollmap_draw */
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

