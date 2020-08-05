
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



typedef void hexmap_tileset_get_rgraph(hexmap_tileset_t *tileset,
    char tile_c, rot_t rot,
    rendergraph_t **rgraph_ptr, bool *rot_ok_ptr,
    int *frame_offset_ptr);
typedef hexcollmap_elem_t *hexcollmap_tile_get_part(
    hexcollmap_tile_t *tile, int i);
#define HEXMAP_TILESET_DEFINE_HELPERS(TYPE) \
    static void hexmap_tileset_get_rgraph_##TYPE(hexmap_tileset_t *tileset, \
        char tile_c, rot_t rot, \
        rendergraph_t **rgraph_ptr, bool *rot_ok_ptr, \
        int *frame_offset_ptr \
    ){ \
        /* rgraph: non-NULL if we found an entry with matching tile_c */ \
        /* rot_ok: whether the entry we found had an rgraph for the */ \
        /* requested rot; if false, rgraph is the entry's rgraph for */ \
        /* rot=0 and caller should rotate the rgraph manually */ \
        bool rot_ok = false; \
        int frame_offset = 0; \
        rendergraph_t *rgraph = NULL; \
        for(int i = 0; i < tileset->TYPE##_entries_len; i++){ \
            hexmap_tileset_entry_t *entry = tileset->TYPE##_entries[i]; \
            if(entry->tile_c != tile_c)continue; \
            if(entry->n_rgraphs > rot){ \
                rot_ok = true; \
                rgraph = entry->rgraphs[rot]; \
            }else rgraph = entry->rgraphs[0]; \
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
    hexcollmap_tile_get_part *tile_get_part
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
                &rgraph_tile, &rot_ok, &frame_offset);
        int frame_i = frame_offset? x: 0;
        if(rgraph_tile == NULL){
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

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            vec_t v;
            hexcollmap_get_vec4(collmap, x, y, v);
            vec_mul(space, v, unit);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            #define ADD_RGRAPH_FROM_TILE(PART, ROT) { \
                err = rendergraph_add_rgraph_from_tile( \
                    rgraph, tile, ROT, #PART, \
                    tileset, x, space, v, \
                    hexmap_tileset_get_rgraph_##PART, \
                    hexcollmap_tile_get_##PART); \
                if(err)return err; \
            }
            ADD_RGRAPH_FROM_TILE(vert, 1)
            ADD_RGRAPH_FROM_TILE(edge, 3)
            ADD_RGRAPH_FROM_TILE(face, 2)
            #undef HEXMAP_ADD_TILE
        }
    }

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
        rgraph, &submap->collmap, &submap->tileset,
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
    prismelrenderer_t *prend = map->prend;

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
