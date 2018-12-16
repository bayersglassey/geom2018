
#include <stdio.h>

#include "vec4.h"
#include "bounds.h"


char ok_char(bool ok){
    return ok? '.': 'X';
}

int vec4_test(vec_t v, int a, int b, int c, int d){
    vec_t w;
    vec4_set(w, a, b, c, d);
    bool ok = vec_eq(4, v, w);
    printf("%c ", ok_char(ok)); vec_printf(4, v);
    printf(" == "); vec_printf(4, w); printf("\n");
    return ok? 0: 1;
}

void renderpair_printf(int x, int y){
    printf("(% 2i, % 2i)", x, y);
}

int renderpair_test(int x1, int y1, int x2, int y2){
    bool ok = x1 == x2 && y1 == y2;
    printf("%c ", ok_char(ok)); renderpair_printf(x1, y1);
    printf(" == "); renderpair_printf(x2, y2); printf("\n");
    return ok? 0: 1;
}

int trf4_test(trf_t *t, bool flip, int rot, int a, int b, int c, int d){
    trf_t s = {flip, rot, {a, b, c, d}};
    bool ok = trf_eq(&vec4, t, &s);
    printf("%c (", ok_char(ok)); trf_printf(4, t);
    printf(") == ("); trf_printf(4, &s); printf(")\n");
    return ok? 0: 1;
}

int boundary_box_test(boundary_box_t *b1, int l, int r, int t, int b){
    boundary_box_t b2 = {.l=l, .r=r, .t=t, .b=b};
    bool ok = boundary_box_eq(b1, &b2);
    printf("%c ", ok_char(ok)); boundary_box_printf(b1);
    printf(" == "); boundary_box_printf(&b2); printf("\n");
    return ok? 0: 1;
}

int position_box_test(position_box_t *b1, int x, int y, int w, int h){
    position_box_t b2 = {.x=x, .y=y, .w=w, .h=h};
    bool ok = position_box_eq(b1, &b2);
    printf("%c ", ok_char(ok)); position_box_printf(b1);
    printf(" == "); position_box_printf(&b2); printf("\n");
    return ok? 0: 1;
}

void print_title(const char *title){
    printf("\n  << %s >>\n", title);
}


int vec4_tests(){
    int fails = 0;
    vec_t v, w;
    trf_t t, s;
    boundary_box_t bbox;
    position_box_t pbox;


    /****** VEC_T TESTS ******/

        print_title("R A = B");
        vec4_set(v, 1, 0, 0, 0);
        vec4_rot(v, 1);
        fails += vec4_test(v, 0, 1, 0, 0);

        print_title("R^-1 B = A");
        vec4_set(v, 0, 1, 0, 0);
        vec4_rot(v, vec4.rot_max-1);
        fails += vec4_test(v, 1, 0, 0, 0);

        print_title("R^-1 (R D) = D");
        vec4_set(v, -1, 0, 1, 0);
        vec4_rot(v, vec4.rot_max-1);
        fails += vec4_test(v, 0, 0, 0, 1);

        print_title("Stress test of rot");
        vec4_set(v, 1, 2, 3, 4);
        vec4_rot(v, 2);
        fails += vec4_test(v, -3, -4, 4, 6);

        print_title("Stress test of flip");
        vec4_set(v, 1, 2, 3, 4);
        vec4_flip(v, true);
        fails += vec4_test(v, 4, 2, -3, -6);

        print_title("2C 3B = 6D");
        vec4_set(v, 0, 3, 0, 0);
        vec4_set(w, 0, 0, 2, 0);
        vec_mul(&vec4, v, w);
        fails += vec4_test(v, 0, 0, 0, 6);

        print_title("Stress test of mul");
        vec4_set(v, 1, 2, 3, 4);
        vec4_set(w, 5, 6, 7, 8);
        vec_mul(&vec4, v, w);
        fails += vec4_test(v, -88, -36, 95, 112);

        print_title("Test of render");
        vec4_set(v, 1, 2, 3, 4);
        {
            int x, y;
            vec4_render(v, &x, &y);
            fails += renderpair_test(x, y, 9, -16);
        }

    /****** TRF_T TESTS ******/

        print_title("Test identity trf");
        vec4_set(v, 2, 3, 4, 5);
        t.flip = false;
        t.rot = 7;
        vec4_set(t.add, 2, 3, 4, 5);
        s.flip = false;
        s.rot = 0;
        vec4_set(s.add, 0, 0, 0, 0);
        vec_apply(&vec4, v, &s);
        trf_apply(&vec4, &t, &s);
        fails += vec4_test(v, 2, 3, 4, 5);
        fails += trf4_test(&t, false, 7, 2, 3, 4, 5);
        vec_apply_inv(&vec4, v, &s);
        trf_apply_inv(&vec4, &t, &s);
        fails += vec4_test(v, 2, 3, 4, 5);
        fails += trf4_test(&t, false, 7, 2, 3, 4, 5);

        print_title("Stress test {vec,trf}_apply & {vec,trf}_apply_inv");
        vec4_set(v, 2, 3, 4, 5);
        t.flip = false;
        t.rot = 7;
        vec4_set(t.add, 2, 3, 4, 5);
        s.flip = true;
        s.rot = 3;
        vec4_set(s.add, 6, 7, 8, 9);
        vec_apply(&vec4, v, &s);
        trf_apply(&vec4, &t, &s);
        /* TODO: calculate by hand & verify results here */
        vec_apply_inv(&vec4, v, &s);
        trf_apply_inv(&vec4, &t, &s);
        fails += vec4_test(v, 2, 3, 4, 5);
        fails += trf4_test(&t, false, 7, 2, 3, 4, 5);

    /****** BOUNDS TESTS ******/

        /*
            #####
            ##X##
            #####
            #####
        */
        print_title("Conversion back & forth between position and "
            "boundary boxes");
        position_box_set(&pbox, 2, 1, 5, 4);
        boundary_box_from_position_box(&bbox, &pbox);
        fails += boundary_box_test(&bbox, -2, 3, -1, 3);
        position_box_from_boundary_box(&pbox, &bbox);
        fails += position_box_test(&pbox, 2, 1, 5, 4);

    /****** RESULTS ******/

    if(fails > 0){
        printf("### FAILED %i TEST%s ###\n", fails, fails > 1? "S": "");
        return 1;
    }else{
        printf("OK\n");
        return 0;
    }
}


int main(int n_args, char *args[]){
    return vec4_tests();
}

