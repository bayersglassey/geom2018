#include <stdio.h>
#include <stdbool.h>


#include "tooling.h"
#include "defaults.h"
#include "hexgame.h"
#include "prismelrenderer.h"


static hexgame_save_callback_t _save_callback;
static int _save_callback(hexgame_t *game){
    /* Should presumably never happen when running tests! */
    fprintf(stderr, "WARNING: tried to save!..");
    return 0;
}


void hexgame_cleanup_for_tooling(hexgame_t *game){
    prismelrenderer_cleanup(game->prend);
    free(game->prend);

    prismelrenderer_cleanup(game->minimap_prend);
    free(game->minimap_prend);

    hexgame_cleanup(game);
}

int hexgame_init_for_tooling(hexgame_t *game, const char *prend_filename){
    int err;

    fprintf(stderr, "Loading prismelrenderer from file: %s\n",
        prend_filename);
    prismelrenderer_t *prend = malloc(sizeof *prend);
    if(!prend)return 1;
    err = prismelrenderer_init(prend, &vec4);
    if(err)return err;
    err = prismelrenderer_load(prend, prend_filename, NULL, NULL);
    if(err)return err;

    prismelrenderer_t *minimap_prend = malloc(sizeof *minimap_prend);
    if(!minimap_prend)return 1;
    err = prismelrenderer_init(minimap_prend, &vec4);
    if(err)return err;
    err = prismelrenderer_load(minimap_prend,
        DEFAULT_MINIMAP_PREND_FILENAME,
        NULL, NULL);
    if(err)return err;

    err = hexgame_init(game, prend, minimap_prend,
        HEXGAME_DEFAULT_MINIMAP_TILESET,
        &_save_callback, NULL, /* no callback data */
        false /* no audio */);
    if(err)return err;

    return 0;
}
