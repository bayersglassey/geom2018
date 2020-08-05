
#include "hexmap.h"
#include "util.h"
#include "geom.h"
#include "vec4.h"
#include "hexspace.h"
#include "prismelrenderer.h"


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




static int add_tile_rgraph(rendergraph_t *rgraph, rendergraph_t *rgraph2,
    vecspace_t *space, vec_t add, rot_t rot, int frame_i
){
    int err;
    rendergraph_trf_t *rendergraph_trf;
    err = rendergraph_push_rendergraph_trf(rgraph, &rendergraph_trf);
    if(err)return err;
    rendergraph_trf->rendergraph = rgraph2;
    rendergraph_trf->trf.rot = rot;
    rendergraph_trf->frame_i = frame_i;
    vec_cpy(space->dims, rendergraph_trf->trf.add, add);
    return 0;
}

static void get_rgraph_v_from_collmap(hexmap_t *map, hexmap_submap_t *submap,
    int x, int y, vec_t v
){
    /* Converts x, y coords of submap->collmap into a vec_t of
    prend->space */
    prismelrenderer_t *prend = map->prend;
    vecspace_t *space = prend->space;
    hexcollmap_t *collmap = &submap->collmap;

    int px = x - collmap->ox;
    int py = y - collmap->oy;
    vec_t w;
    hexspace_set(w, px, -py);
    vec4_vec_from_hexspace(v, w);
    vec_mul(space, v, map->unit);
}

static int hexmap_submap_add_tile_rgraph(hexcollmap_tile_t *tile, int rot,
    const char *part_name,
    hexmap_tileset_t *tileset, int x, rendergraph_t *rgraph,
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
            fprintf(stderr, "Couldn't find %s tile "
                "for character: %c\n", part_name, elem->tile_c);
            return 2;}
        err = add_tile_rgraph(rgraph, rgraph_tile,
            space, v,
            rot_ok? 0: vec4_rot_from_hexspace(i),
            frame_i);
        if(err)return err;
    }
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

    ARRAY_PUSH_NEW(rendergraph_t*, prend->rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(submap->filename), prend, NULL,
        rendergraph_animation_type_default,
        n_frames);
    if(err)return err;

    hexmap_tileset_t *tileset = &submap->tileset;

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            vec_t v;
            get_rgraph_v_from_collmap(map, submap, x, y, v);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            #define HEXMAP_ADD_TILE_RGRAPH(PART, ROT) { \
                err = hexmap_submap_add_tile_rgraph(tile, ROT, #PART, \
                    tileset, x, rgraph, space, v, \
                    hexmap_tileset_get_rgraph_##PART, \
                    hexcollmap_tile_get_##PART); \
                if(err)return err; \
            }
            HEXMAP_ADD_TILE_RGRAPH(vert, 1)
            HEXMAP_ADD_TILE_RGRAPH(edge, 3)
            HEXMAP_ADD_TILE_RGRAPH(face, 2)
            #undef HEXMAP_ADD_TILE
        }
    }

    for(int i = 0; i < collmap->rendergraphs_len; i++){
        hexmap_rendergraph_t *hexmap_rgraph = collmap->rendergraphs[i];
        rendergraph_t *rgraph2 = prismelrenderer_get_rendergraph(
            prend, hexmap_rgraph->name);
        if(rgraph2 == NULL){
            fprintf(stderr, "Couldn't find hexmap rgraph: %s\n",
                hexmap_rgraph->name);
            return 2;}

        trf_t *trf = &hexmap_rgraph->trf;

        /* Convert trf from hexspace to vec4 and multiply by map->unit */
        vec_t v;
        vec4_vec_from_hexspace(v, trf->add);
        vec_mul(space, v, map->unit);
        rot_t r = vec4_rot_from_hexspace(trf->rot);

        int frame_i = 0;

        err = add_tile_rgraph(rgraph, rgraph2,
            space, v, r, frame_i);
        if(err)return err;
    }

    submap->rgraph_map = rgraph;
    return 0;
}
