
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "util.h"
#include "prismelrenderer.h"
#include "vec4.h"
#include "geom.h"


int main(int n_args, char *args[]){
    int err;

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

    rendergraph_t *rgraph = rendergraph_map_get(
        prend.rendergraph_map, "dodeca");
    if(rgraph == NULL)return 2;
    trf_t trf = {false, 0, {0, 0, 0, 0}};
    SDL_Color pal[] = {};
    err = rendergraph_get_or_render_bitmap(rgraph, NULL, &trf, pal);
    if(err)return err;


    prismelrenderer_dump(&prend, stdout);

    return 0;
}
