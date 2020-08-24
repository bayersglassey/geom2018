

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "str_utils.h"
#include "file_utils.h"
#include "hexcollmap.h"
#include "lexer.h"
#include "write.h"
#include "geom.h"
#include "hexspace.h"


/********************
 * HEXMAP RECORDING *
 ********************/

void hexmap_recording_cleanup(hexmap_recording_t *recording){
    free(recording->filename);
    free(recording->palmapper_name);
}

int hexmap_recording_init(hexmap_recording_t *recording, int type,
    char *filename, char *palmapper_name, int frame_offset
){
    recording->type = type;
    recording->filename = filename;
    recording->palmapper_name = palmapper_name;
    recording->frame_offset = frame_offset;
    trf_zero(&recording->trf);
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
    char part_c, char *filename, char *palmapper_name, int frame_offset
){
    part->type = type;
    part->part_c = part_c;
    part->filename = filename;
    part->palmapper_name = palmapper_name;
    part->frame_offset = frame_offset;

    vec_zero(part->trf.add);
    part->trf.rot = 0;
    part->trf.flip = false;
    return 0;
}

void hexcollmap_part_cleanup(hexcollmap_part_t *part){
    free(part->filename);
    free(part->palmapper_name);
}


void hexcollmap_cleanup(hexcollmap_t *collmap){
    free(collmap->name);
    free(collmap->tiles);
    ARRAY_FREE_PTR(hexmap_recording_t*, collmap->recordings,
        hexmap_recording_cleanup)
    ARRAY_FREE_PTR(hexmap_rendergraph_t*, collmap->rendergraphs,
        hexmap_rendergraph_cleanup)
}

int hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    char *name
){
    memset(collmap, 0, sizeof(*collmap));
    collmap->name = name;
    collmap->space = space;
    ARRAY_INIT(collmap->recordings);
    ARRAY_INIT(collmap->rendergraphs);
    return 0;
}

void hexcollmap_dump(hexcollmap_t *collmap, FILE *f){
    /* The rawest of dumps */
    fprintf(f, "hexcollmap: %p\n", collmap);
    if(collmap == NULL)return;
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

static char _write_vert(char tile_c){
    return tile_c_is_visible(tile_c)? '+': '.';
}

static char _write_edge(char tile_c, char _default){
    return tile_c_is_visible(tile_c)? _default: ' ';
}

static char _write_face(char tile_c){
    return tile_c_is_visible(tile_c)?
        (tile_c_is_special(tile_c)? tile_c: '*'): ' ';
}

void hexcollmap_write_with_parts(hexcollmap_t *collmap, FILE *f,
    bool just_coll, bool extra,
    hexcollmap_part_t **parts, int parts_len
){
    /* Writes it so you can hopefully more or less read it back again */

    const char *tabs = "";
    if(!just_coll){
        if(parts){
            fprintf(f, "parts:\n");
            for(int i = 0; i < parts_len; i++){
                hexcollmap_part_t *part = parts[i];
                char part_c_str[2] = {part->part_c, '\0'};
                fputs("    ", f);
                fus_write_str(f, part_c_str);
                fputs(": ", f);

                if(part->type == HEXCOLLMAP_PART_TYPE_RECORDING){
                    fputs("recording ", f);
                }else if(part->type == HEXCOLLMAP_PART_TYPE_RENDERGRAPH){
                    fputs("shape ", f);
                }

                if(part->filename){
                    fus_write_str(f, part->filename);
                }else{
                    fputs("empty", f);
                }

                {
                    trf_t *trf = &part->trf;
                    if(trf->rot)fprintf(f, " ^%i", trf->rot);
                    if(trf->flip)fputs(" ~", f);
                    if(part->draw_z)fprintf(f, " |%i", part->draw_z);
                }

                if(part->palmapper_name){
                    fputc(' ', f);
                    fus_write_str(f, part->palmapper_name);
                }else if(part->frame_offset){
                    fputs(" empty", f);
                }

                if(part->frame_offset){
                    fprintf(f, " %i", part->frame_offset);
                }

                fputc('\n', f);
            }
        }

        //fprintf(f, "default_vert: ...");
        //fprintf(f, "default_edge: ...");
        //fprintf(f, "default_face: ...");

        if(extra)
        for(int i = 0; i < collmap->recordings_len; i++){
            hexmap_recording_t *recording = collmap->recordings[i];
            fprintf(f, "# %s:\n",
                hexmap_recording_type_msg(recording->type));
            fprintf(f, "#     filename: %s\n", recording->filename);
            if(recording->palmapper_name){
                fprintf(f, "#     palmapper: %s\n",
                    recording->palmapper_name);
            }
            fprintf(f, "#     trf: (%i %i) %i %c\n",
                recording->trf.add[0],
                recording->trf.add[1],
                recording->trf.rot,
                recording->trf.flip? 'y': 'n');
            fprintf(f, "#     frame_offset: %i\n", recording->frame_offset);
        }

        if(extra)
        for(int i = 0; i < collmap->rendergraphs_len; i++){
            hexmap_rendergraph_t *rgraph = collmap->rendergraphs[i];
            fprintf(f, "# rendergraph:\n");
            fprintf(f, "#     name: %s\n", rgraph->name);
            if(rgraph->palmapper_name){
                fprintf(f, "#     palmapper: %s\n",
                    rgraph->palmapper_name);
            }
            fprintf(f, "#     trf: (%i %i) %i %c\n",
                rgraph->trf.add[0],
                rgraph->trf.add[1],
                rgraph->trf.rot,
                rgraph->trf.flip? 'y': 'n');
        }

        fprintf(f, "collmap:\n");
        tabs = "    ";
    }

    for(int y = 0; y < collmap->h; y++){
        // \ /
        //  . -

        // \ /
        fputs(tabs, f);
        fputs(";; ", f);
        for(int x = 0; x < y; x++){
            fputs("  ", f);
        }
        for(int x = 0; x < collmap->w; x++){
            hexcollmap_tile_t *tile = &collmap->tiles[y * collmap->w + x];
            fprintf(f, "%c%c%c%c",
                _write_edge(tile->edge[2].tile_c, '\\'),
                _write_face(tile->face[1].tile_c),
                _write_edge(tile->edge[1].tile_c, '/'),
                _write_face(tile->face[0].tile_c));
        }
        fputc('\n', f);

        //  . -
        fputs(tabs, f);
        fputs(";; ", f);
        for(int x = 0; x < y; x++){
            fputs("  ", f);
        }
        for(int x = 0; x < collmap->w; x++){
            bool is_origin = x == collmap->ox && y == collmap->oy;
            hexcollmap_tile_t *tile = &collmap->tiles[y * collmap->w + x];
            fprintf(f, "%c%c%c%c",
                is_origin? '(': ' ',
                _write_vert(tile->vert[0].tile_c),
                is_origin? ')': ' ',
                _write_edge(tile->edge[0].tile_c, '-'));
        }
        fputc('\n', f);
    }
}

void hexcollmap_write(hexcollmap_t *collmap, FILE *f,
    bool just_coll, bool extra
){
    hexcollmap_write_with_parts(collmap, f, just_coll, extra, NULL, 0);
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

