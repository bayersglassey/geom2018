
#include <stdio.h>
#include <stdbool.h>

#include "bounds.h"


void boundary_box_clear(boundary_box_t *b){
    b->l = b->r = b->t = b->b = 0;
}

void position_box_clear(position_box_t *b){
    b->x = b->y = b->w = b->h = 0;
}

void boundary_box_set(boundary_box_t *box, int l, int r, int t, int b){
    box->l = l;
    box->r = r;
    box->t = t;
    box->b = b;
}

void position_box_set(position_box_t *box, int x, int y, int w, int h){
    box->x = x;
    box->y = y;
    box->w = w;
    box->h = h;
}

void boundary_box_fprintf(FILE *f, boundary_box_t *b){
    fprintf(f, "{.l=% 2i, .r=% 2i, .t=% 2i, .b=% 2i}",
        b->l, b->r, b->t, b->b);
}

void position_box_fprintf(FILE *f, position_box_t *b){
    fprintf(f, "{.x=% 2i, .y=% 2i, .w=% 2i, .h=% 2i}",
        b->x, b->y, b->w, b->h);
}

void boundary_box_printf(boundary_box_t *b){
    boundary_box_fprintf(stdout, b);
}

void position_box_printf(position_box_t *b){
    position_box_fprintf(stdout, b);
}

bool boundary_box_eq(boundary_box_t *b1, boundary_box_t *b2){
    return
        b1->l == b2->l &&
        b1->r == b2->r &&
        b1->t == b2->t &&
        b1->b == b2->b
    ;
}

bool position_box_eq(position_box_t *b1, position_box_t *b2){
    return
        b1->x == b2->x &&
        b1->y == b2->y &&
        b1->w == b2->w &&
        b1->h == b2->h
    ;
}

void boundary_box_from_position_box(boundary_box_t *bbox, position_box_t *pbox){
    bbox->l = -pbox->x;
    bbox->t = -pbox->y;
    bbox->r =  pbox->w - pbox->x;
    bbox->b =  pbox->h - pbox->y;
}

void position_box_from_boundary_box(position_box_t *pbox, boundary_box_t *bbox){
    pbox->x = -bbox->l;
    pbox->y = -bbox->t;
    pbox->w =  bbox->r - bbox->l;
    pbox->h =  bbox->b - bbox->t;
}

void boundary_box_union(boundary_box_t *b1, boundary_box_t *b2){
    if(b2->l < b1->l)b1->l = b2->l;
    if(b2->r > b1->r)b1->r = b2->r;
    if(b2->t < b1->t)b1->t = b2->t;
    if(b2->b > b1->b)b1->b = b2->b;
}

void position_box_shift(position_box_t *b, int x, int y){
    b->x += x;
    b->y += y;
}

void boundary_box_shift(boundary_box_t *b, int x, int y){
    b->l += x;
    b->r += x;
    b->t += y;
    b->b += y;
}

