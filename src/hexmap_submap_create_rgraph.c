
#include "hexgame.h"
#include "hexmap.h"
#include "util.h"
#include "geom.h"
#include "vec4.h"
#include "hexspace.h"
#include "prismelrenderer.h"



/* BIG OL' HACK: If any "tile" rgraphs are animated, we need the
map's rgraph to be animated also.
The most correct way to do this is I guess to compute the LCD of
the tile rgraphs' n_frames, and set the map's rgraph's n_frames
to that.
But for now we use a magic number which has "many" divisors.
That's a lot of bitmaps to cache for the map's rgraph, though...
if we're going to allow complicated map animations, maybe we
should disable bitmap caching for it (somehow). */
#define HEXMAP_SUBMAP_RGRAPH_N_FRAMES 24


static void hexcollmap_face_rot(
    int *x_ptr, int *y_ptr, int *i_ptr, rot_t addrot
){
    /* NOTE: this function was originally in hexcollmap_parse.c, used by
    hexcollmap_clone. However, that function now simply calls out to
    hexcollmap_draw, which sets up a trf_t index and ultimately does
    hexcollmap_normalize_##PART(&index).
    Somehow, doing that sidesteps the need for this function's lookup table.
    Is that better somehow?.. do we want to remove this function and document
    & use hexcollmap_draw's technique?.. */

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


static int _get_rgraph_i_when_faces_solid_vert(int n_faces_solid) {
    if(n_faces_solid == 0)return 0;
    if(n_faces_solid == 6)return 2;
    return 1;
}
static int _get_rgraph_i_when_faces_solid_edge(int n_faces_solid) {
    /* n_faces_solid:
        0 if neither face is solid
        1 if bottom face (face 0) is solid
        2 if top face (face 1) is solid
        3 if both faces are solid
    */
    return n_faces_solid;
}
static int _get_rgraph_i_when_faces_solid_face(int n_faces_solid) {
    /* Should never be called. hexmap_tileset_entry_t's type should
    never be HEXMAP_TILESET_ENTRY_TYPE_WHEN_FACES_SOLID for a face. */
    return 0;
}

typedef void hexmap_tileset_get_rgraph(hexmap_tileset_t *tileset,
    char tile_c, rot_t rot, int n_faces_solid,
    rendergraph_t **rgraph_ptr, bool *rot_ok_ptr,
    int *frame_offset_ptr);
typedef hexcollmap_elem_t *hexcollmap_tile_get_part(
    hexcollmap_tile_t *tile, int i);
#define HEXMAP_TILESET_DEFINE_HELPERS(TYPE) \
    static void hexmap_tileset_get_rgraph_##TYPE(hexmap_tileset_t *tileset, \
        char tile_c, rot_t rot, int n_faces_solid, \
        rendergraph_t **rgraph_ptr, bool *rot_ok_ptr, \
        int *frame_offset_ptr \
    ){ \
        /* rgraph: non-NULL if we found an entry with matching tile_c */ \
        /* rot_ok: whether the entry we found had an rgraph for the */ \
        /* requested rot; if false, caller should rotate the rgraph */ \
        /* manually */ \
        bool rot_ok = false; \
        int frame_offset = 0; \
        rendergraph_t *rgraph = NULL; \
        for(int i = 0; i < tileset->TYPE##_entries_len; i++){ \
            /* This loop searches for the entry with given tile_c. */ \
            hexmap_tileset_entry_t *entry = tileset->TYPE##_entries[i]; \
            if(entry->tile_c != tile_c)continue; \
            rgraph = entry->rgraphs[0]; \
            if(entry->type == HEXMAP_TILESET_ENTRY_TYPE_ROTS){ \
                if(rot < entry->n_rgraphs){ \
                    rot_ok = true; \
                    rgraph = entry->rgraphs[rot]; \
                } \
            }else if( \
                entry->type == HEXMAP_TILESET_ENTRY_TYPE_WHEN_FACES_SOLID \
            ){ \
                int rgraph_i = _get_rgraph_i_when_faces_solid_##TYPE( \
                    n_faces_solid); \
                rgraph = entry->rgraphs[rgraph_i]; \
            } \
            frame_offset = entry->frame_offset; \
            break; \
        } \
        *rgraph_ptr = rgraph; \
        *rot_ok_ptr = rot_ok; \
        *frame_offset_ptr = frame_offset; \
    } \
    static hexcollmap_elem_t *hexcollmap_tile_get_##TYPE( \
        hexcollmap_tile_t *tile, int i \
    ){ \
        return &tile->TYPE[i]; \
    }
HEXMAP_TILESET_DEFINE_HELPERS(vert)
HEXMAP_TILESET_DEFINE_HELPERS(edge)
HEXMAP_TILESET_DEFINE_HELPERS(face)
#undef HEXMAP_TILESET_DEFINE_HELPERS




static int rendergraph_add_rgraph(rendergraph_t *rgraph, rendergraph_t *rgraph2,
    vec_t add, rot_t rot, int frame_i
){
    int err;
    rendergraph_trf_t *rendergraph_trf;
    err = rendergraph_push_rendergraph_trf(rgraph, &rendergraph_trf);
    if(err)return err;
    rendergraph_trf->rendergraph = rgraph2;
    rendergraph_trf->trf.rot = rot;
    rendergraph_trf->frame_i = frame_i;
    vec_cpy(rgraph->prend->space->dims, rendergraph_trf->trf.add, add);
    return 0;
}

static void hexcollmap_get_vec4(hexcollmap_t *collmap,
    int x, int y, vec_t v
){
    /* Converts x, y coords of collmap into a vec_t of hexspace (a, b, c, d) */

    int px = x - collmap->ox;
    int py = y - collmap->oy;
    vec_t w;
    hexspace_set(w, px, -py);
    vec4_vec_from_hexspace(v, w);
}

static int rendergraph_add_rgraph_from_tile(
    rendergraph_t *rgraph, hexcollmap_tile_t *tile,
    int rot, const char *part_name,
    hexmap_tileset_t *tileset, int x,
    vecspace_t *space, vec_t v,
    hexmap_tileset_get_rgraph *get_rgraph,
    hexcollmap_tile_get_part *tile_get_part,
    int *n_faces_solid
        /* array of length rot, or NULL if this call is for a face */
){
    int err;
    for(int i = 0; i < rot; i++){
        hexcollmap_elem_t *elem = tile_get_part(tile, i);
        if(!hexcollmap_elem_is_visible(elem))continue;
        rendergraph_t *rgraph_tile;
        bool rot_ok;
        int frame_offset;
        get_rgraph(tileset,
                elem->tile_c, i,
                n_faces_solid? n_faces_solid[i]: 0,
                &rgraph_tile, &rot_ok, &frame_offset);
        int frame_i = frame_offset? x: 0;
        if(rgraph_tile == NULL){
            fprintf(stderr, "In tileset: %s\n", tileset->name);
            fprintf(stderr,
                "Couldn't find %s tile for character: %c\n",
                part_name, elem->tile_c);
            return 2;}
        err = rendergraph_add_rgraph(rgraph, rgraph_tile,
            v, rot_ok? 0: vec4_rot_from_hexspace(i),
            frame_i);
        if(err)return err;
    }
    return 0;
}

static void _init_n_faces_solid_vert(int *n_faces_solid,
    hexcollmap_t *collmap
){
    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            int n_faces = 0;
            for(int rot = 0; rot < 6; rot++){
                int face_x = x;
                int face_y = y;
                int face_i = 0;
                hexcollmap_face_rot(&face_x, &face_y, &face_i, rot);

                hexcollmap_tile_t *face_tile = hexcollmap_get_tile_xy(
                    collmap, face_x, face_y);
                if(face_tile){
                    hexcollmap_elem_t *elem = &face_tile->face[face_i];
                    if(hexcollmap_elem_is_visible(elem))n_faces++;
                }
            }

            int tile_i = y * collmap->w + x;
            n_faces_solid[tile_i] = n_faces;
        }
    }
}

static void _init_n_faces_solid_edge(int *n_faces_solid,
    hexcollmap_t *collmap
){
    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            for(int i = 0; i < 3; i++){

                /* n_faces: a bit array, where:
                    if(n_faces & 1) bottom face solid
                    if(n_faces & 2) top face solid

                    ...where "bottom" and "top" faces are relative to current
                    edge, as indicated by B and T in the following diagram:
                          T
                       (.)- .
                          B
                */
                int n_faces = 0;

                {
                    /*
                       .   .
                           0
                        (+)  .
                    */
                    int face_x = x;
                    int face_y = y;
                    int face_i = 0;
                    hexcollmap_face_rot(&face_x, &face_y, &face_i, i);

                    hexcollmap_tile_t *face_tile = hexcollmap_get_tile_xy(
                        collmap, face_x, face_y);
                    if(face_tile){
                        // top face is solid
                        hexcollmap_elem_t *elem = &face_tile->face[face_i];
                        if(hexcollmap_elem_is_visible(elem))n_faces |= 2;
                    }
                }
                {
                    /*
                        (+)  .
                           0
                           .   .
                    */
                    int face_x = x;
                    int face_y = y;
                    int face_i = 0;
                    hexcollmap_face_rot(&face_x, &face_y, &face_i, i + 5);

                    hexcollmap_tile_t *face_tile = hexcollmap_get_tile_xy(
                        collmap, face_x, face_y);
                    if(face_tile){
                        // bottom face is solid
                        hexcollmap_elem_t *elem = &face_tile->face[face_i];
                        if(hexcollmap_elem_is_visible(elem))n_faces |= 1;
                    }
                }

                int tile_i = y * collmap->w + x;
                n_faces_solid[tile_i * 3 + i] = n_faces;
            }
        }
    }
}

static int rendergraph_add_rgraphs_from_collmap(
    rendergraph_t *rgraph, hexcollmap_t *collmap,
    hexmap_tileset_t *tileset, vec_t unit,
    bool add_collmap_rendergraphs
){
    /* Add to rgraph->rendergraph_trfs, according to collmap->tiles,
    using tiles from tileset.
    unit should be the tileset's unit vector; tileset_t doesn't currently
    have an explicit unit, it's generally supplied by hexmap_t. */
    int err;

    prismelrenderer_t *prend = rgraph->prend;
    vecspace_t *space = prend->space;

    int collmap_size = collmap->w * collmap->h;

    /* Allocate the array backing n_faces_solid_{vert,edge} */
    int *_n_faces_solid = calloc(
        collmap_size * 4,
            /* one set of collmap_size for vert, another 3 for edges */
        sizeof(*_n_faces_solid));
    if(!_n_faces_solid)return 1;

    /* Arrays storing the number of solid faces neighbouring a given vert
    or edge */
    int *n_faces_solid_vert = _n_faces_solid;
    int *n_faces_solid_edge = _n_faces_solid + collmap_size;
    int *n_faces_solid_face = NULL;

    /* Initialize the arrays by calculating number of solid neighbours */
    _init_n_faces_solid_vert(n_faces_solid_vert, collmap);
    _init_n_faces_solid_edge(n_faces_solid_edge, collmap);

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            vec_t v;
            hexcollmap_get_vec4(collmap, x, y, v);
            vec_mul(space, v, unit);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            #define ADD_RGRAPH_FROM_TILE(PART, ROT) { \
                int *n_faces_solid = NULL; \
                if(n_faces_solid_##PART){ \
                    n_faces_solid = &n_faces_solid_##PART[ \
                        (y * collmap->w + x) * ROT]; \
                } \
                err = rendergraph_add_rgraph_from_tile( \
                    rgraph, tile, ROT, #PART, \
                    tileset, x, space, v, \
                    hexmap_tileset_get_rgraph_##PART, \
                    hexcollmap_tile_get_##PART, \
                    n_faces_solid); \
                if(err)return err; \
            }
            ADD_RGRAPH_FROM_TILE(vert, 1)
            ADD_RGRAPH_FROM_TILE(edge, 3)
            ADD_RGRAPH_FROM_TILE(face, 2)
            #undef HEXMAP_ADD_TILE
        }
    }

    free(_n_faces_solid);

    /* A better name for collmap->rendergraphs might be collmap->decals...
    They are rendergraphs attached to collmap for purely visual reasons,
    e.g. text */
    if(add_collmap_rendergraphs){
        for(int i = 0; i < collmap->rendergraphs_len; i++){
            hexmap_rendergraph_t *hexmap_rgraph = collmap->rendergraphs[i];
            rendergraph_t *rgraph2 = prismelrenderer_get_rendergraph(
                prend, hexmap_rgraph->name);
            if(rgraph2 == NULL){
                fprintf(stderr, "Couldn't find hexmap rgraph: %s\n",
                    hexmap_rgraph->name);
                return 2;}

            trf_t *trf = &hexmap_rgraph->trf;

            /* Convert trf from hexspace to vec4 and multiply by unit */
            vec_t v;
            vec4_vec_from_hexspace(v, trf->add);
            vec_mul(space, v, unit);
            rot_t r = vec4_rot_from_hexspace(trf->rot);

            int frame_i = 0;

            err = rendergraph_add_rgraph(rgraph, rgraph2,
                v, r, frame_i);
            if(err)return err;
        }
    }

    return 0;
}


int hexmap_submap_create_rgraph_map(hexmap_submap_t *submap){
    /* Build submap->rgraph_map based on submap->collmap.
    This rendergraph represents submap, treating hexcollmap's tiles
    as rendergraphs of hexmap->tileset. */
    int err;

    hexmap_t *map = submap->map;
    prismelrenderer_t *prend = map->prend;

    int n_frames = HEXMAP_SUBMAP_RGRAPH_N_FRAMES;

    ARRAY_PUSH_NEW(rendergraph_t*, prend->rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(submap->filename), prend, NULL,
        rendergraph_animation_type_default,
        n_frames);
    if(err)return err;

    err = rendergraph_add_rgraphs_from_collmap(
        rgraph, &submap->collmap, submap->tileset,
        map->unit, true);
    if(err)return err;

    submap->rgraph_map = rgraph;
    return 0;
}


int hexmap_submap_create_rgraph_minimap(hexmap_submap_t *submap){
    /* Build submap->rgraph_minimap based on submap->collmap.
    This rendergraph represents submap as a "minimap" treating
    hexcollmap's tiles as prismels (so, ignoring hexgame->unit and
    hexmap->tileset) */
    int err;

    hexmap_t *map = submap->map;
    prismelrenderer_t *prend = map->game->minimap_prend;

    int n_frames = HEXMAP_SUBMAP_RGRAPH_N_FRAMES;

    ARRAY_PUSH_NEW(rendergraph_t*, prend->rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdupcat("minimap:", submap->filename),
        prend, NULL,
        rendergraph_animation_type_default,
        n_frames);
    if(err)return err;

    vec_t unit = {1, 0, 0, 0};
    err = rendergraph_add_rgraphs_from_collmap(
        rgraph, &submap->collmap, &map->game->minimap_tileset,
        unit, false);
    if(err)return err;

    submap->rgraph_minimap = rgraph;
    return 0;
}
