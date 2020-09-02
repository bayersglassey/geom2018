
#include <stdio.h>
#include <stdbool.h>

#include "hexspace.h"
#include "hexbox.h"


int hexbox_rot_matrix[HEXBOX_VALUES] = HEXBOX_ROT_MATRIX;



void hexbox_set(hexbox_t *hexbox,
    int min_x, int max_x,
    int min_y, int max_y,
    int min_z, int max_z
){
    hexbox->values[0] = min_x;
    hexbox->values[1] = max_x;
    hexbox->values[2] = min_y;
    hexbox->values[3] = max_y;
    hexbox->values[4] = min_z;
    hexbox->values[5] = max_z;
}

bool hexbox_eq(hexbox_t *hexbox1, hexbox_t *hexbox2){
    for(int i = 0; i < HEXBOX_VALUES; i++){
        if(hexbox1->values[i] != hexbox2->values[i])return false;
    }
    return true;
}

void hexbox_fprintf(FILE *f, hexbox_t *hexbox){
    fputc('{', f);
    for(int i = 0; i < HEXBOX_VALUES; i++){
        fprintf(f, "% 4i", hexbox->values[i]);
    }
    fputc('}', f);
}

void hexbox_printf(hexbox_t *hexbox){
    hexbox_fprintf(stdout, hexbox);
}

void hexbox_rot(hexbox_t *hexbox, rot_t rot){
    rot = rot_contain(HEXSPACE_ROT_MAX, rot);
    for(int i = 0; i < rot; i++){
        hexbox_t original = *hexbox;
        for(int j = 0; j < HEXBOX_VALUES; j++){
            int j_rot = hexbox_rot_matrix[j];
            hexbox->values[j] = original.values[j_rot];
        }
    }
}
