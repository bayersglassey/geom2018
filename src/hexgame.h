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



/************
 * KEY INFO *
 ************/

#define KEYINFO_KEY_ACTION  0
#define KEYINFO_KEY_U       1
#define KEYINFO_KEY_D       2
#define KEYINFO_KEY_L       3
#define KEYINFO_KEY_R       4
#define KEYINFO_KEYS        5

typedef struct keyinfo {
    bool isdown[KEYINFO_KEYS];
    bool wasdown[KEYINFO_KEYS];
    bool wentdown[KEYINFO_KEYS];
} keyinfo_t;

void keyinfo_reset(keyinfo_t *info);
void keyinfo_copy(keyinfo_t *info1, keyinfo_t *info2);
int fus_lexer_get_keyinfo(fus_lexer_t *lexer,
    keyinfo_t *info);



/*************
 * RECORDING *
 *************/

typedef struct recording {
    int action;
        /* 0: none, 1: play, 2: record */
    bool loop;
    char *data;
    char *stateset_name;
    char *state_name;

    struct body *body;

    vec_t pos0;
    rot_t rot0;
    bool turn0;

    keyinfo_t keyinfo;

    int i;
    int size;
    int wait;
    char *name;
    FILE *file;
    int offset;
} recording_t;

void recording_cleanup(recording_t *rec);
void recording_reset(recording_t *rec);
void recording_init(recording_t *rec, struct body *body,
    bool loop);
int recording_load(recording_t *rec, const char *filename,
    struct body *body, bool loop);
const char *get_last_recording_filename();
const char *get_next_recording_filename();
int recording_step(recording_t *rec);


/********
 * BODY *
 ********/

typedef struct body {
    struct hexgame *game;

    vec_t pos;
    rot_t rot;
    bool turn;

    keyinfo_t keyinfo;

    recording_t recording;
        /* This can be used by player (while they create/edit
        a recording), actor (plays back recordings), or neither
        (e.g. bodies loaded by hexmap which simply play a recording
        on repeat, or body created when you press F10). */

    palettemapper_t *palmapper;
    stateset_t stateset;
    state_t *state;
    int frame_i;
    int cooldown;
    bool dead;

    bool out_of_bounds;
    hexmap_t *map;
    hexmap_submap_t *cur_submap;
} body_t;

void body_cleanup(body_t *body);
int body_init(body_t *body, struct hexgame *game, hexmap_t *map,
    const char *stateset_filename, const char *state_name,
    palettemapper_t *palmapper);

rot_t body_get_rot(body_t *body);
void body_init_trf(body_t *body, trf_t *trf);

int body_init_stateset(body_t *body, const char *stateset_filename,
    const char *state_name, hexmap_t *map);
int body_set_state(body_t *body, const char *state_name);

struct actor;
struct hexgame;
int state_handle_rules(state_t *state, body_t *body,
    struct actor *actor, struct hexgame *game);
int body_step(body_t *body, struct hexgame *game);
int body_render(body_t *body,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom,
    hexmap_t *map, vec_t camera_renderpos, prismelmapper_t *mapper);

void body_keydown(body_t *body, int key_i);
void body_keyup(body_t *body, int key_i);
int body_get_key_i(body_t *body, char c, bool absolute);
char body_get_key_c(body_t *body, int key_i, bool absolute);

int body_load_recording(body_t *body, const char *filename, bool loop);
int body_play_recording(body_t *body);
int body_restart_recording(body_t *body, bool hard);
int body_start_recording(body_t *body, char *name);
int body_stop_recording(body_t *body);
int body_record(body_t *body, const char *data);
int body_maybe_record_wait(body_t *body);


/**********
 * PLAYER *
 **********/

typedef struct player {
    body_t *body;

    vec_t respawn_pos;
    rot_t respawn_rot;
    bool respawn_turn;
    char *respawn_filename;

    int keymap;
    SDL_Keycode key_code[KEYINFO_KEYS];
} player_t;

void player_cleanup(player_t *player);
int player_init(player_t *player, body_t *body, int keymap,
    vec_t respawn_pos, rot_t respawn_rot, bool respawn_turn,
    char *respawn_filename);

int player_process_event(player_t *player, SDL_Event *event);
int player_step(player_t *player, struct hexgame *game);



/*********
 * ACTOR *
 *********/

typedef struct actor {
    body_t *body;
    stateset_t stateset;
    state_t *state;
} actor_t;

void actor_cleanup(actor_t *actor);
int actor_init(actor_t *actor, hexmap_t *map, body_t *body,
    const char *stateset_filename, const char *state_name);

int actor_init_stateset(actor_t *actor, const char *stateset_filename,
    const char *state_name, hexmap_t *map);
int actor_set_state(actor_t *actor, const char *state_name);
int actor_step(actor_t *actor, struct hexgame *game);



/**********
 * CAMERA *
 **********/

typedef struct camera {
    struct hexgame *game;
    hexmap_t *map;
    hexmap_submap_t *cur_submap;

    /* player_t *player;
    ...it makes some sense for a camera to not have a player; I can imagine
    a camera which just scrolls around the map, controlled directly by arrow
    keys.
    However, when camera is following a player, it should have a pointer to
    it, instead of currently hardcoded "look for body 0" nonsense. */

    bool zoomout;
    bool follow;
    bool smooth_scroll;

    SDL_Color colors[256];
    int colors_fade;
    #define HEXGAME_MAX_COLORS_FADE 10

    vec_t pos;
        /* target pos within hexmap, to which we are scrolling */
    vec_t scrollpos;
        /* pos to which we have scrolled so far */
    rot_t rot;
    bool should_reset;
        /* If true, on next step scrollpos should jump to pos
        and should_reset should be set to false. */
} camera_t;

void camera_cleanup(camera_t *camera);
int camera_init(camera_t *camera, struct hexgame *game);
void camera_set(camera_t *camera, vec_t pos, rot_t rot);
body_t *camera_get_body(camera_t *camera);
void camera_colors_flash(camera_t *camera, Uint8 r, Uint8 g, Uint8 b,
    int percent);
void camera_colors_flash_white(camera_t *camera, int percent);
int camera_step(camera_t *camera);
int camera_render(camera_t *camera,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom);


/***********
 * HEXGAME *
 ***********/

typedef struct hexgame {
    int frame_i;

    hexmap_t *map;
        /* currently not owned by game, but really should be
        (and should be an array of maps!) */
    /* ARRAY_DECL(hexmap_t*, maps) */

    camera_t camera;
        /* should be an array so we can have better hotseat multiplayer */
    /* ARRAY_DECL(camera_t*, cameras) */

    ARRAY_DECL(player_t*, players)
    ARRAY_DECL(actor_t*, actors)
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, hexmap_t *map);
int hexgame_load_actors(hexgame_t *game);
int hexgame_reset_player(hexgame_t *game, player_t *player, bool hard);
int hexgame_reset_players_by_keymap(hexgame_t *game, int keymap, bool hard);
int hexgame_load_recording(hexgame_t *game, const char *filename,
    int keymap, palettemapper_t *palmapper, bool loop);
int hexgame_process_event(hexgame_t *game, SDL_Event *event);
int hexgame_step(hexgame_t *game);


#endif