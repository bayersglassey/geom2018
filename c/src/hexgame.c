

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "hexspace.h"
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

int player_step(player_t *player){

    /* start of new frame, no keys have gone down yet. */
    for(int i = 0; i < PLAYER_KEYS; i++){
        player->key_wentdown[i] = false;}

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

    ARRAY_PUSH_NEW(player_t, *game, players, player0)
    player_init(player0, map->rgraph_player, 0);
    player0->pos[0] = 4;
    player0->pos[1] = 1;
    player0->rot = 0;

    ARRAY_PUSH_NEW(player_t, *game, players, player1)
    player_init(player1, map->rgraph_player, 1);
    player1->pos[0] = 0;
    player1->pos[1] = -2;
    player1->rot = 3;

    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    if(event->type == SDL_KEYDOWN && !event->key.repeat){
        for(int i = 0; i < game->players_len; i++){
            player_t *player = game->players[i];
            for(int i = 0; i < PLAYER_KEYS; i++){
                if(event->key.keysym.sym == player->key_code[i]){
                    player->key_wentdown[i] = true;
                    player->key_isdown[i] = true;}}}
    }
    return 0;
}

int hexgame_step(hexgame_t *game){
    int err;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player);
        if(err)return err;
    }
    return 0;
}

int hexgame_render(hexgame_t *game, test_app_t *app){
    int err;

    RET_IF_SDL_NZ(SDL_SetRenderDrawColor(app->renderer,
        30, 50, 80, 255));
    RET_IF_SDL_NZ(SDL_RenderClear(app->renderer));

    rendergraph_t *rgraph = game->map->rgraph_map;
    err = test_app_blit_rgraph(app, game->map->rgraph_map,
        (vec_t){0}, 0, false, app->frame_i);
    if(err)return err;

    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];

        /* TODO: render player properly... */

        rendergraph_t *rgraph = player->rgraph;
        vec_t pos;
        vec4_vec_from_hexspace(pos, player->pos);
        vec_mul(rgraph->space, pos, game->map->unit);
        rot_t rot = vec4_rot_from_hexspace(player->rot);
        flip_t flip = false;
        int frame_i = 0;

        /* TODO: player->turn affects rot + flip.
        Add player_get_rot, player_get_flip?.. */

        err = test_app_blit_rgraph(app, rgraph, pos, rot, flip, frame_i);
        if(err)return err;
    }

    SDL_RenderPresent(app->renderer);
    return 0;
}

