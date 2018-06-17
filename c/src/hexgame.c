

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "prismelrenderer.h"
#include "array.h"
#include "util.h"
#include "test_app.h"



/**********
 * PLAYER *
 **********/

void player_cleanup(player_t *player){
}

int player_init(player_t *player, rendergraph_t *rgraph, int keymap){
    player->rgraph = rgraph;
    for(int i = 0; i < PLAYER_KEYS; i++)player->key_code[i] = 0;
    if(keymap == 0){
        player->key_code[PLAYER_KEY_U] = SDLK_UP;
        player->key_code[PLAYER_KEY_D] = SDLK_DOWN;
        player->key_code[PLAYER_KEY_L] = SDLK_LEFT;
        player->key_code[PLAYER_KEY_R] = SDLK_RIGHT;
    }else if(keymap == 1){
        player->key_code[PLAYER_KEY_U] = SDLK_w;
        player->key_code[PLAYER_KEY_D] = SDLK_s;
        player->key_code[PLAYER_KEY_L] = SDLK_a;
        player->key_code[PLAYER_KEY_R] = SDLK_d;
    }
    return 0;
}


/***********
 * HEXGAME *
 ***********/


void hexgame_cleanup(hexgame_t *game){
    ARRAY_FREE(player_t, *game, players, player_cleanup)
}

int hexgame_init(hexgame_t *game, stateset_t *stateset, hexmap_t *map){
    game->stateset = stateset;
    game->map = map;
    ARRAY_INIT(*game, players)
    return 0;
}

bool hexgame_ready(hexgame_t *game){
    return game->stateset && game->map;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    switch(event->type){
        case SDL_KEYDOWN: {
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                for(int i = 0; i < PLAYER_KEYS; i++){
                    if(event->key.keysym.sym == player->key_code[i]){
                        player->key_wentdown[i] = true;
                        player->key_isdown[i] = true;}}}
        } break;
        default: break;
    }
    return 0;
}

int hexgame_step(hexgame_t *game){
    return 0;
}

int hexgame_render(hexgame_t *game, test_app_t *app){
    int err;

    RET_IF_SDL_NZ(SDL_SetRenderDrawColor(app->renderer,
        30, 50, 80, 255));
    RET_IF_SDL_NZ(SDL_RenderClear(app->renderer));

    rendergraph_t *rgraph = game->map->rgraph_map;
    int animated_frame_i = get_animated_frame_i(
        rgraph->animation_type, rgraph->n_frames, app->frame_i);

    rendergraph_bitmap_t *bitmap;
    err = rendergraph_get_or_render_bitmap(rgraph, &bitmap,
        app->rot, false, animated_frame_i, app->pal, app->renderer);
    if(err)return err;

    SDL_Rect dst_rect = {
        app->scw / 2 + app->x0 - bitmap->pbox.x * app->zoom,
        app->sch / 2 + app->y0 - bitmap->pbox.y * app->zoom,
        bitmap->pbox.w * app->zoom,
        bitmap->pbox.h * app->zoom
    };
    SDL_Texture *bitmap_texture;
    err = rendergraph_bitmap_get_texture(bitmap, app->renderer,
        &bitmap_texture);
    if(err)return err;
    RET_IF_SDL_NZ(SDL_RenderCopy(app->renderer, bitmap_texture,
        NULL, &dst_rect));

    SDL_RenderPresent(app->renderer);
    return 0;
}

