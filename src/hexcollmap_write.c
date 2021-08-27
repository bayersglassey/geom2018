

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "str_utils.h"
#include "hexcollmap.h"
#include "write.h"
#include "geom.h"
#include "hexspace.h"



static char _write_vert(char tile_c, char empty){
    return tile_c_is_visible(tile_c)? '+': empty;
}

static char _write_edge(char tile_c, char visible, char empty){
    return tile_c_is_visible(tile_c)? visible: empty;
}

static char _write_face(char tile_c, char empty){
    return tile_c_is_visible(tile_c)?
        (tile_c_is_special(tile_c)? tile_c: '*'): empty;
}

static bool out_of_bounds_z(hexbox_t *hexbox, int x, int y){
    int z = x - y;
    return
        z < hexbox->values[HEXBOX_INDEX(HEXBOX_Z, HEXBOX_MIN)] ||
        z > hexbox->values[HEXBOX_INDEX(HEXBOX_Z, HEXBOX_MAX)];
}

static void _hexcollmap_write(hexcollmap_t *collmap, FILE *f,
    const char *tabs, bool nodots, bool show_tiles, bool eol_semicolons
){
    for(int y = 0; y < collmap->h; y++){
        // \ /
        //  . -

        int _y = collmap->oy - y;

        // \ /
        fputs(tabs, f);
        fputs(";; ", f);
        for(int x = 0; x < y; x++){
            fputs("  ", f);
        }
        for(int x = 0; x < collmap->w; x++){
            int _x = x - collmap->ox;
            if(out_of_bounds_z(&collmap->hexbox, _x, _y)){
                fputs(show_tiles? "[XX]": "    ", f);
                continue;
            }
            hexcollmap_tile_t *tile = &collmap->tiles[y * collmap->w + x];
            fprintf(f, "%c%c%c%c",
                _write_edge(tile->edge[2].tile_c, '\\', show_tiles? '[': ' '),
                _write_face(tile->face[1].tile_c, show_tiles? ' ': ' '),
                _write_edge(tile->edge[1].tile_c, '/', show_tiles? ' ': ' '),
                _write_face(tile->face[0].tile_c, show_tiles? ']': ' '));
        }
        if(eol_semicolons)fputc(';', f);
        fputc('\n', f);

        //  . -
        fputs(tabs, f);
        fputs(";; ", f);
        for(int x = 0; x < y; x++){
            fputs("  ", f);
        }
        for(int x = 0; x < collmap->w; x++){
            int _x = x - collmap->ox;
            if(out_of_bounds_z(&collmap->hexbox, _x, _y)){
                fputs(show_tiles? "[XX]": "    ", f);
                continue;
            }
            bool is_origin = x == collmap->ox && y == collmap->oy;
            hexcollmap_tile_t *tile = &collmap->tiles[y * collmap->w + x];
            fprintf(f, "%c%c%c%c",
                is_origin? '(': show_tiles? '[': ' ',
                _write_vert(tile->vert[0].tile_c, nodots? (show_tiles? ' ': ' '): '.'),
                is_origin? ')': show_tiles? ' ': ' ',
                _write_edge(tile->edge[0].tile_c, '-', show_tiles? ']': ' '));
        }
        if(eol_semicolons)fputc(';', f);
        fputc('\n', f);
    }
}

void hexcollmap_write_with_parts(hexcollmap_t *collmap, FILE *f,
    bool just_coll, bool extra, bool nodots, bool show_tiles,
    bool eol_semicolons, hexcollmap_part_t **parts, int parts_len
){
    /* Writes it so you can hopefully more or less read it back again */

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
        /* TODO: recording->vars */
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

    const char *tabs = "";
    if(!just_coll){
        tabs = "    ";

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
                }else if(part->type == HEXCOLLMAP_PART_TYPE_ACTOR){
                    fputs("actor ", f);
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

        fprintf(f, "collmap:\n");
    }

    fprintf(f, "    # hexbox: (%i %i) (%i %i) (%i %i)\n",
        collmap->hexbox.values[0], collmap->hexbox.values[1],
        collmap->hexbox.values[2], collmap->hexbox.values[3],
        collmap->hexbox.values[4], collmap->hexbox.values[5]);

    _hexcollmap_write(collmap, f, tabs, nodots, show_tiles, eol_semicolons);
}

void hexcollmap_write(hexcollmap_t *collmap, FILE *f,
    bool just_coll, bool extra, bool nodots, bool show_tiles,
    bool eol_semicolons
){
    hexcollmap_write_with_parts(collmap, f,
        just_coll, extra, nodots, show_tiles, eol_semicolons, NULL, 0);
}

