

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


/********************
 * HEXMAP RECORDING *
 ********************/

void hexmap_recording_cleanup(hexmap_recording_t *recording){
    free(recording->filename);
    free(recording->palmapper_name);
    valexpr_cleanup(&recording->visible_expr);
    vars_cleanup(&recording->vars);
    vars_cleanup(&recording->bodyvars);
}

int hexmap_recording_init(hexmap_recording_t *recording, int type,
    char *filename, char *palmapper_name, int frame_offset
){
    recording->type = type;
    recording->filename = filename;
    recording->palmapper_name = palmapper_name;
    recording->frame_offset = frame_offset;
    trf_zero(&recording->trf);

    valexpr_set_literal_bool(&recording->visible_expr, true);
    recording->visible_not = false;

    vars_init(&recording->vars);
    vars_init(&recording->bodyvars);
    return 0;
}


/**********************
 * HEXMAP RENDERGRAPH *
 **********************/

void hexmap_rendergraph_cleanup(hexmap_rendergraph_t *rendergraph){
    free(rendergraph->name);
    free(rendergraph->palmapper_name);
}

int hexmap_rendergraph_init(hexmap_rendergraph_t *rendergraph,
    char *name, char *palmapper_name
){
    rendergraph->name = name;
    rendergraph->palmapper_name = palmapper_name;
    trf_zero(&rendergraph->trf);
    return 0;
}


/**************
 * HEXCOLLMAP *
 **************/

int hexcollmap_part_init(hexcollmap_part_t *part, int type,
    char part_c, char *filename, char *palmapper_name, int frame_offset,
    valexpr_t *visible_expr, bool visible_not,
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

    /* NOTE: we take ownership of *visible_expr from caller */
    part->visible_expr = *visible_expr;
    part->visible_not = visible_not;

    /* vars_t must guarantee that it can be freely copied.
    Which it does. (See the comment in definition of vars_t.) */
    part->vars = *vars;
    part->bodyvars = *bodyvars;

    return 0;
}

void hexcollmap_part_cleanup(hexcollmap_part_t *part){
    free(part->filename);
    free(part->palmapper_name);
    valexpr_cleanup(&part->visible_expr);
    vars_cleanup(&part->vars);
    vars_cleanup(&part->bodyvars);
}


void hexcollmap_cleanup(hexcollmap_t *collmap){
    free(collmap->name);
    free(collmap->tiles);
    ARRAY_FREE_PTR(hexmap_recording_t*, collmap->recordings,
        hexmap_recording_cleanup)
    ARRAY_FREE_PTR(hexmap_rendergraph_t*, collmap->rendergraphs,
        hexmap_rendergraph_cleanup)
}

void hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    char *name
){
    memset(collmap, 0, sizeof(*collmap));
    collmap->name = name;
    collmap->space = space;
    ARRAY_INIT(collmap->recordings);
    ARRAY_INIT(collmap->rendergraphs);
}

void hexcollmap_init_clone(hexcollmap_t *collmap,
    hexcollmap_t *from_collmap, char *name
){
    hexcollmap_init(collmap, from_collmap->space, name);
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

int hexcollmap_load(hexcollmap_t *collmap, const char *filename,
    vars_t *vars
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = hexcollmap_parse(collmap, &lexer, false);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

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

