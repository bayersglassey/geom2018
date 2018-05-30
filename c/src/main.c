
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "prismelrenderer.h"
#include "vec4.h"


int main(int n_args, char *args[]){
    int err;

    SDL_Color pal[] = {
        {.r=255, .g= 60, .b= 60},
        {.r= 60, .g=255, .b= 60},
        {.r= 60, .g= 60, .b=255},
        {.r=255, .g=255, .b=255},
    };

    prismelrenderer_t prend;

    char *filename = "data/test.fus";
    if(n_args >= 2)filename = args[1];

    err = prismelrenderer_load(&prend, filename, &vec4);
    if(err)return err;

    err = prismelrenderer_render_all_bitmaps(&prend, pal);
    if(err)return err;

    prismelrenderer_dump(&prend, stdout);

    printf("OK!\n");
    return 0;
}
