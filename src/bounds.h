#ifndef _BOUNDS_H_
#define _BOUNDS_H_

#include <stdio.h>
#include <stdbool.h>

typedef struct {
    int l;
    int r;
    int t;
    int b;
} boundary_box_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} position_box_t;


void boundary_box_clear(boundary_box_t *b);
void position_box_clear(position_box_t *b);
void boundary_box_set(boundary_box_t *box, int l, int r, int t, int b);
void position_box_set(position_box_t *box, int x, int y, int w, int h);
void boundary_box_fprintf(FILE *f, boundary_box_t *b);
void position_box_fprintf(FILE *f, position_box_t *b);
void boundary_box_printf(boundary_box_t *b);
void position_box_printf(position_box_t *b);
bool boundary_box_eq(boundary_box_t *b1, boundary_box_t *b2);
bool position_box_eq(position_box_t *b1, position_box_t *b2);
void boundary_box_from_position_box(boundary_box_t *bbox, position_box_t *pbox);
void position_box_from_boundary_box(position_box_t *pbox, boundary_box_t *bbox);
void boundary_box_union(boundary_box_t *b1, boundary_box_t *b2);
void boundary_box_shift(boundary_box_t *b, int x, int y);
void position_box_shift(position_box_t *b, int x, int y);

#endif