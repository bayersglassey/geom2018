
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

const int _neigh_coords[HEXPICTURE_VERT_NEIGHBOURS * 2][2] = {
    { 4,  0},
    { 3, -2},
    { 2, -3},
    { 0, -4},
    {-2, -3},
    {-3, -2},

    {-4,  0},
    {-3,  2},
    {-2,  3},
    { 0,  4},
    { 2,  3},
    { 3,  2}
};


const int _neigh_hexcoords[HEXPICTURE_VERT_NEIGHBOURS * 2][4] = {
    { 1,  0,  0,  0},
    { 0,  1,  0,  0},
    { 0,  0,  1,  0},
    { 0,  0,  0,  1},
    {-1,  0,  1,  0},
    { 0, -1,  0,  1},

    {-1,  0,  0,  0},
    { 0, -1,  0,  0},
    { 0,  0, -1,  0},
    { 0,  0,  0, -1},
    { 1,  0, -1,  0},
    { 0,  1,  0, -1}
};

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
    int color; /* 0..255 are colours, -1 is error */
} hexpicture_face_t;

typedef struct hexpicture_edge {
    bool solid;
    struct hexpicture_vert *vert;
        /* vert: the vert "pointed to" by this edge, from the vert on
        which this edge lives */
        /* NOTE: POSSIBLY UNUSED?.. */
} hexpicture_edge_t;

typedef struct hexpicture_vert {
    int x, y; /* Coords within lines */
    int a, b, c, d; /* Coords in hexspace */
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



/* STATIC HELPER FUNCTIONS */

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
    if(c >= '0' && c <= '9')return c - '0';
    if(c >= 'A' && c <= 'Z')return c - 'A' + 10;
    if(c >= 'a' && c <= 'z')return c - 'a' + 10;
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


/* HEXPICTURE PARSE */

static void _hexpicture_parse_max_line_len_and_verts_len(
    size_t *max_line_len_ptr, size_t *verts_len_ptr,
    const char **lines, size_t lines_len
){
    /* Find max_line_len, verts_len */
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
    *max_line_len_ptr = max_line_len;
    *verts_len_ptr = verts_len;
}

static int _hexpicture_parse_origin_and_verts_and_parts(
    hexpicture_vert_t **origin_ptr,
    hexpicture_vert_t *verts,
    hexpicture_part_t *parts,
    const char **lines, size_t lines_len, size_t max_line_len
){
    /* Find origin.
    Set vert->x, vert->y for all vert in verts.
    Set part->type, part->part.vert for all part in parts which corresponds
    to a vert.
    */
    hexpicture_vert_t *origin = NULL;
    for(int vert_i = 0, y = 0; y < lines_len; y++){
        const char *line = lines[y];
        int x = 0;
        char c;
        for(const char *s = line; (c = *s); s++){
            if(c == HEXPICTURE_CHAR_VERT || c == HEXPICTURE_CHAR_ORIGIN){
                hexpicture_vert_t *vert = &verts[vert_i];
                vert->x = x;
                vert->y = y;

                if(c == HEXPICTURE_CHAR_ORIGIN){
                    if(origin){
                        fprintf(stderr,
                            "Multiple origins specified! "
                            "vert %ti (%i, %i) and vert %ti (%i, %i)\n",
                            origin - verts, origin->x, origin->y,
                            vert - verts, vert->x, vert->y);
                        return 2;
                    }
                    origin = vert;
                }

                hexpicture_part_t *part = _GET_PART(x, y);
                part->type = HEXPICTURE_PART_TYPE_VERT;
                part->part.vert = vert;

                vert_i++;
            }
            x++;
        }
    }
    *origin_ptr = origin;
    return 0;
}

static int _hexpicture_parse_edges(
    hexpicture_vert_t *verts, size_t verts_len,
    hexpicture_part_t *parts,
    size_t lines_len, size_t max_line_len
){
    /* Populate vert->edges and vert->reverse_verts for all vert in verts.
    */
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
                return 2;
            }
            hexpicture_edge_t *edge = &vert->edges[neigh_i];
            edge->vert = part->part.vert;
            edge->vert->reverse_verts[neigh_i] = vert;
        }
    }
    return 0;
}

static int _hexpicture_parse_hexspace_coords(
    hexpicture_vert_t *origin,
    hexpicture_vert_t *verts, size_t verts_len
){
    if(verts_len <= 0)return 0;

    /* Allocate vert_queue.
    Queue of vert pointers, telling us in what order we should calculate
    their hexspace coords (since each vert's hexspace coords are
    calculated from a previous one's). */
    hexpicture_vert_t **vert_queue =
        calloc(verts_len, sizeof(*vert_queue));
    if(!vert_queue){
        perror("calloc vert_queue");
        return 1;
    }

    /* Index of front & end of vert_queue.
    We "pop" from the front of the queue by increasing queue_start_i.
    We "push" to the end of the queue by increasing queue_end_i. */
    int queue_start_i = 0;
    int queue_end_i = 0;

    /* Start with the origin: push it onto the queue. */
    vert_queue[0] = origin;
    queue_end_i++;

    while(queue_start_i < queue_end_i){

        /* Pop next vert from front of queue */
        hexpicture_vert_t *vert = vert_queue[queue_start_i];
        queue_start_i++;

        for(int rot = 0; rot < HEXPICTURE_VERT_NEIGHBOURS * 2; rot++){
            hexpicture_vert_t *other_vert =
                (rot < HEXPICTURE_VERT_NEIGHBOURS)?
                vert->edges[rot].vert:
                vert->reverse_verts[rot - HEXPICTURE_VERT_NEIGHBOURS];
            if(!other_vert)continue;

            /* If other_vert is (or ever was) in the queue already,
            continue */
            for(int j = 0; j < queue_end_i; j++){
                if(vert_queue[j] == other_vert)goto _continue;
            }

            /* Add to the queue */
            vert_queue[queue_end_i] = other_vert;
            queue_end_i++;

            /* Calculate hexspace coords */
            other_vert->a = vert->a + _neigh_hexcoords[rot][0];
            other_vert->b = vert->b + _neigh_hexcoords[rot][1];
            other_vert->c = vert->c + _neigh_hexcoords[rot][2];
            other_vert->d = vert->d + _neigh_hexcoords[rot][3];

            /* Lets us continue from within a deeper for-loop */
            _continue: ;
        }
    }

    free(vert_queue);
    return 0;
}

static bool _hexpicture_vert_check_face(hexpicture_vert_t *vert,
    const int *edge_rots, int rot,
    hexpicture_part_t *parts,
    size_t lines_len, size_t max_line_len
){
    /* Checks whether the indicated face exists at vert's position.
    The face is defined by edge_rots, an array of unit vectors represented
    by rotation values in 0..11 (array is terminated with -1).
    Rotation values are integers representing multiples of 30 degrees.
    Each rotation value has rot added to it.
    So for instance, edge_rots={0, 3, 6, 9, -1}, rot=0 represents a square.
    With rot=1, that square is rotated by 30 degrees.
    */
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

static int _hexpicture_parse_faces(
    size_t *faces_len_ptr,
    hexpicture_vert_t *verts, size_t verts_len,
    hexpicture_part_t *parts,
    const char **lines, size_t lines_len, size_t max_line_len
){
    /* Find faces_len, populate vert->faces for all vert in verts.
    */
    size_t faces_len = 0;
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
                        "While parsing face at vert %ti (%i, %i): "
                        "%s, rot=%i\n",
                        vert - verts, vert->x, vert->y,
                        _face_type_msg(type), rot);
                    fprintf(stderr,
                        "Couldn't find face colour, unrecognized char [%c] "
                        "at position (%i, %i)\n", *c, x, y);
                    return 2;
                }
                face->color = color;

                faces_len++;
            }
        }
    }
    *faces_len_ptr = faces_len;
    return 0;
}

static void _hexpicture_dump_verts(
    FILE *file,
    hexpicture_vert_t *origin,
    hexpicture_vert_t *verts, size_t verts_len
){
    /* Dump a text representation of verts to file
    */
    fprintf(file, "verts, edges, faces:\n");
    for(int vert_i = 0; vert_i < verts_len; vert_i++){
        hexpicture_vert_t *vert = &verts[vert_i];
        fprintf(file, "  vert %ti (%i, %i) [%i %i %i %i]:\n",
            vert - verts, vert->x, vert->y,
            vert->a, vert->b, vert->c, vert->d);
        for(int neigh_i = 0; neigh_i < HEXPICTURE_VERT_NEIGHBOURS; neigh_i++){
            hexpicture_edge_t *edge = &vert->edges[neigh_i];
            if(!edge->vert)continue;
            fprintf(file, "    edge %i: vert %ti (%i, %i)\n",
                neigh_i, edge->vert - verts,
                edge->vert->x, edge->vert->y);
        }
        if(false) for(int neigh_i = 0; neigh_i < HEXPICTURE_VERT_NEIGHBOURS; neigh_i++){
            hexpicture_vert_t *reverse_vert = vert->reverse_verts[neigh_i];
            if(!reverse_vert)continue;
            fprintf(file, "    reverse vert %i: vert %ti (%i, %i)\n",
                neigh_i, reverse_vert - verts,
                reverse_vert->x, reverse_vert->y);
        }
        for(int neigh_i = 0; neigh_i < HEXPICTURE_VERT_NEIGHBOURS; neigh_i++){
            hexpicture_face_t *face = &vert->faces[neigh_i];
            if(face->type == HEXPICTURE_FACE_TYPE_NONE)continue;
            fprintf(file, "    face %i: %s %i\n", neigh_i,
                _face_type_msg(face->type), face->color);
        }
    }
}

static int _hexpicture_parse_return_faces(
    hexpicture_return_face_t **return_faces_ptr,
    size_t faces_len,
    hexpicture_vert_t *verts, size_t verts_len
){
    hexpicture_return_face_t *return_faces =
        calloc(faces_len, sizeof(*return_faces));
    if(!return_faces){
        perror("calloc return_faces");
        return 1;
    }

    int return_face_i = 0;
    for(int vert_i = 0; vert_i < verts_len; vert_i++){
        hexpicture_vert_t *vert = &verts[vert_i];
        for(int neigh_i = 0; neigh_i < HEXPICTURE_VERT_NEIGHBOURS; neigh_i++){
            hexpicture_face_t *face = &vert->faces[neigh_i];
            if(face->type == HEXPICTURE_FACE_TYPE_NONE)continue;

            hexpicture_return_face_t *return_face =
                &return_faces[return_face_i];
            return_face->type = face->type - 1;
            return_face->rot = neigh_i;
            return_face->color = face->color;
            return_face->a = vert->a;
            return_face->b = vert->b;
            return_face->c = vert->c;
            return_face->d = vert->d;

            return_face_i++;
        }
    }

    *return_faces_ptr = return_faces;
    return 0;
}

int hexpicture_parse(
    hexpicture_return_face_t **return_faces_ptr,
    size_t *return_faces_len_ptr,
    const char **lines, size_t lines_len,
    bool verbose
){
    int err = 0;

    hexpicture_vert_t *verts = NULL;
    hexpicture_part_t *parts = NULL;

    if(verbose){
        fprintf(stderr, "hexpicture_parse:\n");
        fprintf(stderr, "lines_len: %zi\n", lines_len);
    }

    /* Determine max_line_len and verts_len.
    (These are the width + height of our 2d coordinate system, used by
    e.g. the "parts" array below) */
    size_t max_line_len;
    size_t verts_len;
    _hexpicture_parse_max_line_len_and_verts_len(
        &max_line_len, &verts_len,
        lines, lines_len);

    if(verbose){
        fprintf(stderr, "max_line_len: %zi\n", max_line_len);
        fprintf(stderr, "verts_len: %zi\n", verts_len);
    }

    /* Allocate verts */
    if(!(verts = calloc(verts_len, sizeof(*verts)))){
        perror("calloc verts");
        err = 1;
        goto end;
    }

    /* Allocate parts */
    if(!(parts = calloc(lines_len * max_line_len, sizeof(*parts)))){
        perror("calloc parts");
        err = 1;
        goto end;
    }

    /* Determine origin, populate verts and their parts */
    hexpicture_vert_t *origin;
    err = _hexpicture_parse_origin_and_verts_and_parts(
        &origin, verts, parts,
        lines, lines_len, max_line_len);
    if(err)goto end;

    if(!origin){
        fprintf(stderr, "No origin specified\n");
        err = 2;
        goto end;
    }

    if(verbose){
        fprintf(stderr, "origin: vert %ti\n", origin - verts);
    }

    /* Populate verts' edges and reverse_verts */
    err = _hexpicture_parse_edges(
        verts, verts_len, parts,
        lines_len, max_line_len);
    if(err)goto end;

    /* Populate vert's hexspace coords (a, b, c, d) */
    err = _hexpicture_parse_hexspace_coords(origin, verts, verts_len);
    if(err)goto end;

    /* Populate verts' faces */
    size_t faces_len;
    err = _hexpicture_parse_faces(
        &faces_len,
        verts, verts_len, parts,
        lines, lines_len, max_line_len);
    if(err)goto end;

    if(verbose){
        _hexpicture_dump_verts(stderr, origin, verts, verts_len);
    }

    /* Generate faces to be returned */
    hexpicture_return_face_t *return_faces;
    err = _hexpicture_parse_return_faces(
        &return_faces, faces_len, verts, verts_len);
    if(err)goto end;

    *return_faces_ptr = return_faces;
    *return_faces_len_ptr = faces_len;

    end:
    free(parts);
    free(verts);
    return err;
}


/* RETURN FACE */

static const char *hexpicture_return_face_type_msg(int type){
    switch(type){
        case HEXPICTURE_RETURN_FACE_TYPE_SQ: return "sq";
        case HEXPICTURE_RETURN_FACE_TYPE_TRI: return "tri";
        case HEXPICTURE_RETURN_FACE_TYPE_DIA: return "dia";
        default: return "unknown";
    }
}

void hexpicture_return_face_dump(hexpicture_return_face_t *face,
    FILE *file
){
    fprintf(file, "%s (%i %i %i %i) rot=%i color=%i\n",
        hexpicture_return_face_type_msg(face->type),
        face->a, face->b, face->c, face->d,
        face->rot, face->color);
}
