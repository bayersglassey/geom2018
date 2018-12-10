#ifndef _HEXGAME_H_
#define _HEXGAME_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "hexmap.h"
#include "prismelrenderer.h"
#include "geom.h"
#include "vec4.h"
#include "array.h"


#define MAX_FRAME_I 554400
    /* Largest highly composite number smaller than 2^16 */

#define DEBUG_RULES false
#define DEBUG_RECORDINGS false



/*******************
 * PLAYER KEY INFO *
 *******************/

#define PLAYER_KEY_ACTION  0
#define PLAYER_KEY_U       1
#define PLAYER_KEY_D       2
#define PLAYER_KEY_L       3
#define PLAYER_KEY_R       4
#define PLAYER_KEYS        5

typedef struct player_keyinfo {
    bool isdown[PLAYER_KEYS];
    bool wasdown[PLAYER_KEYS];
    bool wentdown[PLAYER_KEYS];
} player_keyinfo_t;

void player_keyinfo_reset(player_keyinfo_t *info);
void player_keyinfo_copy(player_keyinfo_t *info1, player_keyinfo_t *info2);
int fus_lexer_get_player_keyinfo(fus_lexer_t *lexer,
    player_keyinfo_t *info);



/********************
 * PLAYER RECORDING *
 ********************/

typedef struct player_recording {
    int action;
        /* 0: none, 1: play, 2: record */
    char *data;
    char *stateset_name;
    char *state_name;

    struct hexgame *game;

    vec_t pos0;
    rot_t rot0;
    bool turn0;

    player_keyinfo_t keyinfo;

    int i;
    int size;
    int wait;
    char *name;
    FILE *file;
    int offset;
} player_recording_t;

void player_recording_cleanup(player_recording_t *rec);
void player_recording_reset(player_recording_t *rec);
void player_recording_init(player_recording_t *rec, struct hexgame *game);
int player_recording_load(player_recording_t *rec, const char *filename,
    struct hexgame *game);
const char *get_last_recording_filename();
const char *get_next_recording_filename();


/**********
 * PLAYER *
 **********/

typedef struct player {
    vec_t respawn_pos;
    vec_t pos;
    rot_t rot;
    bool turn;
    SDL_Keycode key_code[PLAYER_KEYS];

    int keymap;
    player_keyinfo_t keyinfo;
    palettemapper_t *palmapper;

    stateset_t stateset;
    state_t *state;
    int frame_i;
    int cooldown;
    bool dead;

    player_recording_t recording;

    bool out_of_bounds;
    hexmap_submap_t *cur_submap;
    char *respawn_filename;
} player_t;

void player_cleanup(player_t *player);
int player_init(player_t *player, hexmap_t *map,
    const char *stateset_filename, const char *state_name, int keymap,
    vec_t respawn_pos, char *respawn_filename);

rot_t player_get_rot(player_t *player, const vecspace_t *space);
void player_init_trf(player_t *player, trf_t *trf, vecspace_t *space);

int player_init_stateset(player_t *player, const char *stateset_filename,
    const char *state_name, hexmap_t *map);
int player_set_state(player_t *player, const char *state_name);

void player_keydown(player_t *player, int key_i);
void player_keyup(player_t *player, int key_i);
int player_get_key_i(player_t *player, char c, bool absolute);
char player_get_key_c(player_t *player, int key_i, bool absolute);
int player_process_event(player_t *player, SDL_Event *event);

int player_step(player_t *player, struct hexgame *game);
int player_render(player_t *player,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom,
    hexmap_t *map, vec_t camera_renderpos, prismelmapper_t *mapper);

int player_play_recording(player_t *player);
int player_restart_recording(player_t *player, bool hard);
int player_start_recording(player_t *player, char *name);
int player_stop_recording(player_t *player);
int player_record(player_t *player, const char *data);
int player_maybe_record_wait(player_t *player);
int player_recording_step(player_t *player);



/*********
 * ACTOR *
 *********/

typedef struct actor {
    player_t *player;
} actor_t;

void actor_cleanup(actor_t *actor);
int actor_init(actor_t *actor);

int actor_step(actor_t *actor, struct hexgame *game);



/***********
 * HEXGAME *
 ***********/

typedef struct hexgame {
    int frame_i;
    bool zoomout;
    bool follow;
    hexmap_t *map;
    hexmap_submap_t *cur_submap;
    vec_t camera_pos;
    rot_t camera_rot;
    ARRAY_DECL(player_t*, players)
    ARRAY_DECL(actor_t*, actors)
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, hexmap_t *map);
int hexgame_reset_player(hexgame_t *game, player_t *player, bool hard);
int hexgame_reset_player_by_keymap(hexgame_t *game, int keymap, bool hard);
int hexgame_load_player_recording(hexgame_t *game, const char *filename,
    int keymap);
int hexgame_process_event(hexgame_t *game, SDL_Event *event);
int hexgame_step(hexgame_t *game);
int hexgame_render(hexgame_t *game,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom);


#endif