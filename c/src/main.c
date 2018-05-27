
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "util.h"
#include "prismelrenderer.h"
#include "vec4.h"
#include "geom.h"


int main(int n_args, char *args[]){
    int err;

    SDL_Color pal[] = {
        {.r=255, .g= 60, .b= 60},
        {.r= 60, .g=255, .b= 60},
        {.r= 60, .g= 60, .b=255},
    };

    fus_lexer_t lexer;
    prismelrenderer_t prend;

    char *filename = "data/test.fus";
    if(n_args >= 2)filename = args[1];

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text);
    if(err)return err;

    err = prismelrenderer_init(&prend, &vec4);
    if(err)return err;

    err = prismelrenderer_parse(&prend, &lexer);
    if(err)return err;

    rendergraph_map_t *rgraph_map = prend.rendergraph_map;
    while(rgraph_map != NULL){
        rendergraph_t *rgraph = rgraph_map->rgraph;
        for(int rot = 0; rot < prend.space->rot_max; rot++){
            trf_t trf = {false, rot, {0, 0, 0, 0}};
            err = rendergraph_get_or_render_bitmap(
                rgraph, NULL, &trf, pal);
            if(err)return err;
        }
        rgraph_map = rgraph_map->next;
    }

    prismelrenderer_dump(&prend, stdout);

    if(0){
        prismel_t *prismel = prismelrenderer_get_prismel(&prend, "tri");
        if(prismel == NULL)return 2;

        printf("PRISMEL: %s\n", prismel->name);

        printf("  boundary boxes:\n");
        for(int rot = 0; rot < prend.space->rot_max; rot++){
            trf_t trf = {false, rot, {0, 0, 0, 0}};
            int bitmap_i = get_bitmap_i(prend.space, &trf);
            boundary_box_t bbox;
            prismel_get_boundary_box(prismel, &bbox, bitmap_i);
            printf("    ");
            trf_printf(prend.space->dims, &trf);
            printf(" (bitmap %2i): ", bitmap_i);
            boundary_box_printf(&bbox);
            printf("\n");
        }
    }

    if(0){
        const char *name = "dodeca";
        rendergraph_t *rgraph = rendergraph_map_get(
            prend.rendergraph_map, name);
        if(rgraph == NULL)return 2;

        printf("RGRAPH: %s\n", name);

        printf("  boundary boxes:\n");
        for(int rot = 0; rot < prend.space->rot_max; rot++){
            trf_t trf = {false, rot, {0, 0, 0, 0}};
            int bitmap_i = get_bitmap_i(prend.space, &trf);
            rendergraph_bitmap_t *bitmap = &rgraph->bitmaps[bitmap_i];
            position_box_t pbox = bitmap->bbox;
            boundary_box_t bbox;
            boundary_box_from_position_box(&bbox, &pbox);
            printf("    ");
            trf_printf(prend.space->dims, &trf);
            printf(" (bitmap %2i): ", bitmap_i);
            boundary_box_printf(&bbox);
            printf("\n");
        }
    }

    printf("OK!\n");
    return 0;
}
