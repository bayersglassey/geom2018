
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

#define HEXPICTURE_FACE_SQ_MAX_ROT 3
#define HEXPICTURE_FACE_TRI_MAX_ROT 4
#define HEXPICTURE_FACE_DIA_MAX_ROT 6


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
    int color;
        /* 0=transparent, 1..16 are colours, -1 is error
        return value for e.g. _get_color */
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

static int _get_color(char c){
    if(c == ' ')return 0;
    if(c >= '0' && c <= '9')return c - '0' + 1;
    if(c >= 'A' && c <= 'Z')return c - 'A' + 10 + 1;
    if(c >= 'a' && c <= 'z')return c - 'a' + 10 + 1;
    return -1;
}

static const char *_get_char(const char **lines,
    size_t lines_len,
    int x, int y
){
    if(x < 0 || y < 0 || y >= lines_len)return NULL;
    const char *line = lines[y];
    size_t line_len = strlen(line);
    if(x >= line_len)return NULL;
    return &line[x];
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
    const int *edge_rots, int rot,
    hexpicture_part_t *parts,
    size_t lines_len, size_t max_line_len
){
    int x = vert->x;
    int y = vert->y;
    for(int edge_rot; (edge_rot = *edge_rots) != -1; edge_rots++){
        edge_rot += rot;
        if(edge_rot >= 12)edge_rot -= 12;

        x += _neigh_coords[edge_rot][0];
        y += _neigh_coords[edge_rot][1];

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

        /* Arrays of unit vectors (represented as rot values) describing
        each prismel. Terminated with -1 */
        static const int sq_edge_rots[] = {0, 3, 6, 9, -1};
        static const int tri_edge_rots[] = {0, 4, 8, -1};
        static const int dia_edge_rots[] = {0, 1, 6, 7, -1};

        /* Location (x, y) of cell -- that is, lines[y][x] -- whose value
        determines face->color, e.g. '0' -> 0, 'A' -> 10, etc. */
        static const int sq_face_coords[HEXPICTURE_FACE_SQ_MAX_ROT][2] = {
            { 1, -1},
            { 0, -1},
            { 0, -1}
        };
        static const int tri_face_coords[HEXPICTURE_FACE_TRI_MAX_ROT][2] = {
            { 2, -1},
            { 1, -2},
            { 0, -1},
            {-1, -2}
        };
        static const int dia_face_coords[HEXPICTURE_FACE_DIA_MAX_ROT][2] = {
            { 3, -1},
            { 2, -2},
            { 1, -3},
            {-1, -3},
            {-2, -2},
            {-3, -1}
        };

        /* Array to be indexed by type - 1 (sq=0, tri=1, dia=2) */
        /* NOTE: C's type notation at its gankiest... */
        static const int (*(face_coords[]))[2] = {
            sq_face_coords,
            tri_face_coords,
            dia_face_coords
        };

        for(int rot = 0; rot < HEXPICTURE_VERT_NEIGHBOURS; rot++){
            int type = HEXPICTURE_FACE_TYPE_NONE;

            if(rot < HEXPICTURE_FACE_SQ_MAX_ROT &&
                _hexpicture_vert_check_face(vert,
                    sq_edge_rots, rot,
                    parts, lines_len, max_line_len)
            ){
                type = HEXPICTURE_FACE_TYPE_SQ;
            }else if(rot < HEXPICTURE_FACE_TRI_MAX_ROT &&
                _hexpicture_vert_check_face(vert,
                    tri_edge_rots, rot,
                    parts, lines_len, max_line_len)
            ){
                type = HEXPICTURE_FACE_TYPE_TRI;
            }else if(rot < HEXPICTURE_FACE_DIA_MAX_ROT &&
                _hexpicture_vert_check_face(vert,
                    dia_edge_rots, rot,
                    parts, lines_len, max_line_len)
            ){
                type = HEXPICTURE_FACE_TYPE_DIA;
            }

            if(type != HEXPICTURE_FACE_TYPE_NONE){
                hexpicture_face_t *face = &vert->faces[rot];
                face->type = type;
                const int (*coords)[2] = face_coords[type - 1];
                    /* type - 1: sq=0, tri=1, dia=2 */
                int x = vert->x + coords[rot][0];
                int y = vert->y + coords[rot][1];
                const char *c = _get_char(lines, lines_len, x, y);
                int color = !c? 0: _get_color(*c);
                if(color == -1){
                    fprintf(stderr,
                        "Couldn't find face colour, unrecognized char [%c] "
                        "at position (%i, %i)\n", *c, x, y);
                    return 2;
                }
                face->color = color;
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
                fprintf(stderr, "    face %i: %s %i\n", neigh_i,
                    _face_type_msg(face->type), face->color);
            }
        }
    }

    free_parts: free(parts);
    free_verts: free(verts);
    end: return err;
}
