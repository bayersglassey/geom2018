
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexpicture.h"


#define HEXPICTURE_CHAR_VERT '+'
#define HEXPICTURE_CHAR_ORIGIN '*'
#define HEXPICTURE_CHAR_EDGE_HORIZ '-'
#define HEXPICTURE_CHAR_EDGE_VERTI '|'

#define HEXPICTURE_VERT_NEIGHBOURS 6
#define HEXPICTURE_VERT_NEIGHBOUR_COORDS { \
    { 4,  0}, \
    { 3, -2}, \
    { 2, -3}, \
    { 0, -4}, \
    {-2, -3}, \
    {-3, -2}, \
    \
    {-4,  0}, \
    {-3,  2}, \
    {-2,  3}, \
    { 0,  4}, \
    { 2,  3}, \
    { 3,  2} \
}

const int _neigh_coords[HEXPICTURE_VERT_NEIGHBOURS * 2][2] =
    HEXPICTURE_VERT_NEIGHBOUR_COORDS;

#define HEXPICTURE_SQ_EDGES 4
#define HEXPICTURE_TRI_EDGES 3
#define HEXPICTURE_DIA_EDGES 4


#define _GET_PART(_X, _Y) (&parts[(_Y) * max_line_len + (_X)])


enum hexpicture_face_type {
    HEXPICTURE_FACE_TYPE_NONE,
    HEXPICTURE_FACE_TYPE_SQ,
    HEXPICTURE_FACE_TYPE_TRI,
    HEXPICTURE_FACE_TYPE_DIA,
    HEXPICTURE_FACE_TYPES
};

enum hexpicture_part_type {
    HEXPICTURE_PART_TYPE_NONE,
    HEXPICTURE_PART_TYPE_VERT,
    HEXPICTURE_PART_TYPE_EDGE,
    HEXPICTURE_PART_TYPE_FACE,
    HEXPICTURE_PART_TYPES
};

typedef struct hexpicture_face {
    int type; /* enum hexpicture_face_type */
    int color; /* 0=transparent, 1..16 are colours */
} hexpicture_face_t;

typedef struct hexpicture_edge {
    bool solid;
    struct hexpicture_vert *vert;
        /* vert: the vert "pointed to" by this edge, from the vert on
        which this edge lives */
        /* NOTE: POSSIBLY UNUSED?.. */
} hexpicture_edge_t;

typedef struct hexpicture_vert {
    int x, y;
    struct hexpicture_edge edges[HEXPICTURE_VERT_NEIGHBOURS];
    struct hexpicture_face faces[HEXPICTURE_VERT_NEIGHBOURS];
    struct hexpicture_vert *reverse_verts[HEXPICTURE_VERT_NEIGHBOURS];
        /* reverse_verts:
        Verts for which we are the corresponding neighbour (edge->vert).
        That is, the following assertions hold:

            for(int i = 0; i < HEXPICTURE_VERT_NEIGHBOURS; i++){
                if(this->reverse_verts[i] != NULL){
                    assert(this->reverse_verts[i]->edges[i].vert == this);
                }
            }

        */
        /* NOTE: POSSIBLY UNUSED?.. */
} hexpicture_vert_t;

typedef struct hexpicture_part {
    int type; /* enum hexpicture_part_type */
    union {
        hexpicture_vert_t *vert;
        hexpicture_edge_t *edge;
        hexpicture_face_t *face;
    } part;
} hexpicture_part_t;



static const char *_face_type_msg(int type){
    switch(type){
        case HEXPICTURE_FACE_TYPE_NONE: return "none";
        case HEXPICTURE_FACE_TYPE_SQ: return "sq";
        case HEXPICTURE_FACE_TYPE_TRI: return "tri";
        case HEXPICTURE_FACE_TYPE_DIA: return "dia";
        default: return "unknown";
    }
}

static hexpicture_part_t *_get_part(hexpicture_part_t *parts,
    size_t lines_len, size_t max_line_len,
    int x, int y
){
    if(x < 0 || y < 0 || x >= max_line_len || y >= lines_len)return NULL;
    return _GET_PART(x, y);
}

static hexpicture_vert_t *_get_vert(hexpicture_part_t *parts,
    size_t lines_len, size_t max_line_len,
    int x, int y
){
    hexpicture_part_t *part = _get_part(parts,
        lines_len, max_line_len, x, y);
    if(!part || part->type != HEXPICTURE_PART_TYPE_VERT)return NULL;
    return part->part.vert;
}

static bool _hexpicture_vert_check_face(hexpicture_vert_t *vert,
    const int *edges, int rot,
    hexpicture_part_t *parts,
    size_t lines_len, size_t max_line_len
){
    int x = vert->x;
    int y = vert->y;
    for(int edge; (edge = *edges) != -1; edges++){
        edge += rot;
        if(edge >= 12)edge -= 12;

        x += _neigh_coords[edge][0];
        y += _neigh_coords[edge][1];

        hexpicture_vert_t *found_vert = _get_vert(parts,
            lines_len, max_line_len, x, y);
        if(!found_vert)return false;
    }
    return true;
}


/* HEXPICTURE METHODS */

void hexpicture_cleanup(hexpicture_t *pic){
}

void hexpicture_init(hexpicture_t *pic){
}

int hexpicture_parse(hexpicture_t *pic,
    const char **lines, size_t lines_len,
    bool verbose
){
    int err = 0;

    if(verbose){
        fprintf(stderr, "hexpicture_parse:\n");
        fprintf(stderr, "lines_len: %zi\n", lines_len);
    }

    /* Determine max_line_len and verts_len */
    size_t max_line_len = 0;
    size_t verts_len = 0;
    for(int y = 0; y < lines_len; y++){
        const char *line = lines[y];
        size_t line_len = strlen(line);
        if(line_len > max_line_len)max_line_len = line_len;
        char c;
        for(const char *s = line; (c = *s); s++){
            if(c == HEXPICTURE_CHAR_VERT || c == HEXPICTURE_CHAR_ORIGIN){
                verts_len++;
            }
        }
    }

    if(verbose){
        fprintf(stderr, "max_line_len: %zi\n", max_line_len);
        fprintf(stderr, "verts_len: %zi\n", verts_len);
    }

    /* Allocate verts */
    hexpicture_vert_t *verts = calloc(verts_len, sizeof(*verts));
    if(!verts){
        perror("calloc verts");
        err = 1;
        goto end;
    }

    /* Allocate parts */
    hexpicture_part_t *parts = calloc(lines_len * max_line_len, sizeof(*parts));
    if(!parts){
        perror("calloc parts");
        err = 1;
        goto free_verts;
    }

    /* Determine origin, populate verts and their parts */
    hexpicture_vert_t *origin = NULL;
    for(int vert_i = 0, y = 0; y < lines_len; y++){
        const char *line = lines[y];
        int x = 0;
        char c;
        for(const char *s = line; (c = *s); s++){
            if(c == HEXPICTURE_CHAR_VERT || c == HEXPICTURE_CHAR_ORIGIN){
                hexpicture_vert_t *vert = &verts[vert_i];
                if(c == HEXPICTURE_CHAR_ORIGIN)origin = vert;
                vert->x = x;
                vert->y = y;

                hexpicture_part_t *part = _GET_PART(x, y);
                part->type = HEXPICTURE_PART_TYPE_VERT;
                part->part.vert = vert;

                vert_i++;
            }
            x++;
        }
    }

    if(verbose){
        fprintf(stderr, "origin: vert %ti\n", origin - verts);
    }

    /* Populate verts' edges and reverse_verts */
    for(int vert_i = 0; vert_i < verts_len; vert_i++){
        hexpicture_vert_t *vert = &verts[vert_i];
        for(int neigh_i = 0; neigh_i < HEXPICTURE_VERT_NEIGHBOURS; neigh_i++){
            int neigh_x = vert->x + _neigh_coords[neigh_i][0];
            int neigh_y = vert->y + _neigh_coords[neigh_i][1];
            hexpicture_part_t *part = _get_part(parts,
                lines_len, max_line_len,
                neigh_x, neigh_y);
            if(!part || part->type == HEXPICTURE_PART_TYPE_NONE)continue;
            if(part->type != HEXPICTURE_PART_TYPE_VERT){
                fprintf(stderr,
                    "Bad part type at (%i, %i): %i, expected %i\n",
                    neigh_x, neigh_y, part->type, HEXPICTURE_PART_TYPE_VERT);
                err = 2;
                goto free_parts;
            }
            hexpicture_edge_t *edge = &vert->edges[neigh_i];
            edge->vert = part->part.vert;
            edge->vert->reverse_verts[neigh_i] = vert;
        }
    }

    /* Populate verts' faces */
    for(int vert_i = 0; vert_i < verts_len; vert_i++){
        hexpicture_vert_t *vert = &verts[vert_i];

        static const int sq_edges[] = {0, 3, 6, 9, -1};
        static const int tri_edges[] = {0, 4, 8, -1};
        static const int dia_edges[] = {0, 1, 6, 7, -1};
        /* TODO!!!!!!!
        static const int sq_faces[][2] = {
            {1, -1},
            {0, -1},
        };
        static const int tri_edges[] = {0, 4, 8, -1};
        static const int dia_edges[] = {0, 1, 6, 7, -1};
        */
        for(int rot = 0; rot < HEXPICTURE_VERT_NEIGHBOURS; rot++){
            int type = HEXPICTURE_FACE_TYPE_NONE;

            if(rot < 3 &&
                _hexpicture_vert_check_face(vert, sq_edges, rot,
                    parts, lines_len, max_line_len)
            ){
                type = HEXPICTURE_FACE_TYPE_SQ;
            }else if(rot < 4 &&
                _hexpicture_vert_check_face(vert, tri_edges, rot,
                    parts, lines_len, max_line_len)
            ){
                type = HEXPICTURE_FACE_TYPE_TRI;
            }else if(rot < 6 &&
                _hexpicture_vert_check_face(vert, dia_edges, rot,
                    parts, lines_len, max_line_len)
            ){
                type = HEXPICTURE_FACE_TYPE_DIA;
            }

            if(type != HEXPICTURE_FACE_TYPE_NONE){
                hexpicture_face_t *face = &vert->faces[rot];
                face->type = type;
                /* TODO: figure out face->color... */
            }
        }
    }

    if(verbose){
        fprintf(stderr, "verts, edges, faces:\n");
        for(int vert_i = 0; vert_i < verts_len; vert_i++){
            hexpicture_vert_t *vert = &verts[vert_i];
            fprintf(stderr, "  vert %ti (%i, %i):\n",
                vert - verts, vert->x, vert->y);
            for(int neigh_i = 0; neigh_i < HEXPICTURE_VERT_NEIGHBOURS; neigh_i++){
                hexpicture_edge_t *edge = &vert->edges[neigh_i];
                if(!edge->vert)continue;
                fprintf(stderr, "    edge %i: vert %ti (%i, %i)\n",
                    neigh_i, edge->vert - verts,
                    edge->vert->x, edge->vert->y);
            }
            if(false) for(int neigh_i = 0; neigh_i < HEXPICTURE_VERT_NEIGHBOURS; neigh_i++){
                hexpicture_vert_t *reverse_vert = vert->reverse_verts[neigh_i];
                if(!reverse_vert)continue;
                fprintf(stderr, "    reverse vert %i: vert %ti (%i, %i)\n",
                    neigh_i, reverse_vert - verts,
                    reverse_vert->x, reverse_vert->y);
            }
            for(int neigh_i = 0; neigh_i < HEXPICTURE_VERT_NEIGHBOURS; neigh_i++){
                hexpicture_face_t *face = &vert->faces[neigh_i];
                if(face->type == HEXPICTURE_FACE_TYPE_NONE)continue;
                fprintf(stderr, "    face %i: %s\n", neigh_i,
                    _face_type_msg(face->type));
            }
        }
    }

    free_parts: free(parts);
    free_verts: free(verts);
    end: return err;
}

