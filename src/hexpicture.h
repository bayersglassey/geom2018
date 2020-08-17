#ifndef _HEXPICTURE_H_
#define _HEXPICTURE_H_

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>


enum hexpicture_return_face_type {
    HEXPICTURE_RETURN_FACE_TYPE_SQ,
    HEXPICTURE_RETURN_FACE_TYPE_TRI,
    HEXPICTURE_RETURN_FACE_TYPE_DIA,
    HEXPICTURE_RETURN_FACE_TYPES
};


typedef struct hexpicture_return_face {
    int type; /* enum hexpicture_return_face_type */
    int rot; /* 0..11 */
    int color;
    int a, b, c, d; /* Coords in hexspace */
} hexpicture_return_face_t;


int hexpicture_parse(
    hexpicture_return_face_t **return_faces_ptr,
    size_t *return_faces_len_ptr,
    const char **lines, size_t lines_len,
    bool verbose);

void hexpicture_return_face_dump(hexpicture_return_face_t *face,
    FILE *file);


#endif