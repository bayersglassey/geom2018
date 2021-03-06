#ifndef _HEXGAME_H_
#define _HEXGAME_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "vars.h"
#include "hexmap.h"
#include "prismelrenderer.h"
#include "geom.h"
#include "vec4.h"
#include "array.h"
#include "hexgame_location.h"


#define MAX_FRAME_I 554400
    /* Largest highly composite number smaller than 2^16 */

#define DEBUG_RULES false
#define DEBUG_RECORDINGS false

/* RESET_TO_SAFETY is to player->safe_location, RESET_SOFT is to
player->respawn_location, RESET_HARD is to start of game. */
enum reset_level {
    RESET_TO_SAFETY,
    RESET_SOFT,
    RESET_HARD
};


/************
 * KEY INFO *
 ************/

#define KEYINFO_KEY_ACTION1 0
#define KEYINFO_KEY_ACTION2 1
#define KEYINFO_KEY_U       2
#define KEYINFO_KEY_D       3
#define KEYINFO_KEY_L       4
#define KEYINFO_KEY_R       5
#define KEYINFO_KEYS        6

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

enum recording_node_type {
    RECORDING_NODE_TYPE_WAIT,
    RECORDING_NODE_TYPE_KEYDOWN,
    RECORDING_NODE_TYPE_KEYUP,
    RECORDING_NODE_TYPES
};

typedef struct recording_node {
    int type; /* enum recording_node_type */
    union {
        int wait;
        int key_c;
    } u;
} recording_node_t;

typedef struct recording {
    int action;
        /* 0: none, 1: play, 2: record */
    bool reacts;
    bool loop;
    bool resets_position; /* default: true, if false, looping doesn't reset body's position */
    char *stateset_name;
    char *state_name;

    hexgame_location_t loc0;

    keyinfo_t keyinfo;

    ARRAY_DECL(struct recording_node, nodes)

    int frame_i;
    int node_i;
    int wait;
    char *name;
    FILE *file;
    int offset;

    /* Weakrefs: */
    struct body *body;
} recording_t;

const char *recording_action_msg(int action);

void recording_cleanup(recording_t *rec);
void recording_reset(recording_t *rec);
void recording_init(recording_t *rec, struct body *body,
    bool loop);
int recording_load(recording_t *rec, const char *filename,
    vars_t *vars, struct body *body, bool loop);
int recording_step(recording_t *rec);


/********
 * BODY *
 ********/

#define FOREACH_BODY_CAMERA(BODY, CAMERA_VAR, BLOCK) { \
    body_t *_foreach_body = (BODY); \
    hexgame_t *_foreach_game = _foreach_body->game; \
    for(int _foreach_i = 0; _foreach_i < _foreach_game->cameras_len; _foreach_i++){ \
        camera_t *CAMERA_VAR = _foreach_game->cameras[_foreach_i]; \
        if(CAMERA_VAR->body != _foreach_body)continue; \
        BLOCK \
    } \
}

typedef struct body {
    hexgame_location_t loc;
    keyinfo_t keyinfo;
    vars_t vars;

    recording_t recording;
        /* This can be used by player (while they create/edit
        a recording), actor (plays back recordings), or neither
        (e.g. bodies loaded by hexmap which simply play a recording
        on repeat, or body created when you press F10). */

    stateset_t stateset;
        /* Bodies can be in a strange state where their stateset is
        not initialized yet.
        This happens when the stateset_filename passed to body_init is
        NULL.
        This happens in the following cases (as of this writing):

            * An actor is loaded by a hexmap (hexmap_load_hexmap_recording)
            ...in this case, a body is created for the actor with
            uninitialized stateset, because we expect the actor to execute
            the "play" effect and initialize body's stateset. (?)

            * A recording is loaded by a hexmap (hexmap_load_recording)
            ...in this case, we need a body because the recording object
            lives on it.
            Once recording is loaded, we call body_play_recording which
            initializes body's stateset.

        I believe we can detect this situation with either of the following:
            * body->stateset->filename = NULL
            * body->state == NULL (?)

        ...I would rather not have to support bodies with uninitialized
        statesets. Can we fix it somehow?.. */

    int frame_i;
    int cooldown;
    int dead; /* enum body_dead */
    bool safe; /* lets player know it should update its safe_location */

    bool confused; /* Reverses l/r keys */

    bool out_of_bounds;

    /* Weakrefs: */
    struct hexgame *game;
    hexmap_t *map;
    hexmap_submap_t *cur_submap;
    palettemapper_t *palmapper;
    state_t *state;
} body_t;

void body_cleanup(body_t *body);
int body_init(body_t *body, struct hexgame *game, hexmap_t *map,
    const char *stateset_filename, const char *state_name,
    palettemapper_t *palmapper);
void hexgame_body_dump(body_t *body, int depth);
int body_get_index(body_t *body);
bool body_is_done_for(body_t *body);
int body_respawn(body_t *body, vec_t pos, rot_t rot, bool turn,
    hexmap_t *map);
int body_add_body(body_t *body, body_t **new_body_ptr,
    const char *stateset_filename, const char *state_name,
    palettemapper_t *palmapper,
    vec_t addpos, rot_t addrot, bool turn);

struct player;
struct player *body_get_player(body_t *body);
void body_flash_cameras(body_t *body, Uint8 r, Uint8 g, Uint8 b,
    int percent);
void body_reset_cameras(body_t *body);
int body_remove(body_t *body);
int body_move_to_map(body_t *body, hexmap_t *map);
int body_refresh_vars(body_t *body);

int body_set_stateset(body_t *body, const char *stateset_filename,
    const char *state_name);
int body_set_state(body_t *body, const char *state_name,
    bool reset_cooldown);

struct actor;
struct hexgame;
void body_update_cur_submap(body_t *body);
int body_handle_rules(body_t *body);
int body_step(body_t *body, struct hexgame *game);
bool body_sends_collmsg(body_t *body, const char *msg);
collmsg_handler_t *body_get_collmsg_handler(body_t *body, const char *msg);
int body_collide_against_body(body_t *body, body_t *body_other);
int body_render(body_t *body,
    SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom,
    hexmap_t *map, vec_t camera_renderpos, prismelmapper_t *mapper);

void body_keydown(body_t *body, int key_i);
void body_keyup(body_t *body, int key_i);
int body_get_key_i(body_t *body, char c);
char body_get_key_c(body_t *body, int key_i, bool absolute);

int body_load_recording(body_t *body, const char *filename, bool loop);
int body_play_recording(body_t *body);
int body_restart_recording(body_t *body, bool ignore_offset, bool reset_position);
int body_start_recording(body_t *body, char *name);
int body_stop_recording(body_t *body);
int body_record_keydown(body_t *body, int key_i);
int body_record_keyup(body_t *body, int key_i);


/**********
 * PLAYER *
 **********/

typedef struct player {
    hexgame_savelocation_t respawn_location;
    hexgame_savelocation_t safe_location;
    char *respawn_filename;

    int keymap;
    SDL_Keycode key_code[KEYINFO_KEYS];

    /* Weakrefs: */
    struct hexgame *game;
    body_t *body;
} player_t;

void player_cleanup(player_t *player);
int player_init(player_t *player, struct hexgame *game, int keymap,
    vec_t respawn_pos, rot_t respawn_rot, bool respawn_turn,
    char *respawn_map_filename, char *respawn_filename);

void player_set_body(player_t *player, body_t *body);
int player_get_index(player_t *player);
void hexgame_player_dump(player_t *player, int depth);
int player_reload(player_t *player);
int player_reload_from_location(player_t *player,
    hexgame_savelocation_t *location);
int player_process_event(player_t *player, SDL_Event *event);
int player_step(player_t *player, struct hexgame *game);
int player_use_savepoint(player_t *player);



/*********
 * ACTOR *
 *********/

typedef struct actor {
    trf_t trf;
    stateset_t stateset;
    int wait;

    vars_t vars;

    /* Weakrefs: */
    struct hexgame *game;
    body_t *body;
    state_t *state;
} actor_t;

void actor_cleanup(actor_t *actor);
int actor_init(actor_t *actor, hexmap_t *map, body_t *body,
    const char *stateset_filename, const char *state_name);

int actor_init_stateset(actor_t *actor, const char *stateset_filename,
    const char *state_name, hexmap_t *map);
int actor_set_state(actor_t *actor, const char *state_name);
int actor_handle_rules(actor_t *actor);
int actor_step(actor_t *actor, struct hexgame *game);
int actor_refresh_vars(actor_t *actor);



/**********
 * CAMERA *
 **********/

enum camera_type {
    CAMERA_TYPE_STATIC,
    CAMERA_TYPE_FOLLOW,
    CAMERA_TYPES,
};

enum {
    /* Distance after which camera of type CAMERA_TYPE_FOLLOW will
    move to catch up to its target. */
    CAMERA_FOLLOW_MAX_DIST=10
};

typedef struct camera {
    bool follow;

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

    /* Weakrefs: */
    struct hexgame *game;
    hexmap_t *map;
    hexmap_submap_t *cur_submap;
    body_t *body; /* optional: camera follows a body */
    prismelmapper_t *mapper;
        /* mapper: set to NULL at start of each step, up to player/app/etc
        to set it each step.
        If set, overrides cur_submap->mapper for that step. */
} camera_t;

void camera_cleanup(camera_t *camera);
int camera_init(camera_t *camera, struct hexgame *game, struct hexmap *map,
    body_t *body);
void camera_set(camera_t *camera, vec_t pos, rot_t rot);
void camera_set_body(camera_t *camera, body_t *body);
void camera_colors_flash(camera_t *camera, Uint8 r, Uint8 g, Uint8 b,
    int percent);
void camera_colors_flash_white(camera_t *camera, int percent);
int camera_step(camera_t *camera);
int camera_render(camera_t *camera,
    SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom);


/***********
 * HEXGAME *
 ***********/

struct hexgame;
typedef int new_game_callback_t(struct hexgame *game, player_t *player,
    const char *map_filename);
typedef int continue_callback_t(struct hexgame *game, player_t *player);
typedef int set_players_callback_t(struct hexgame *game, player_t *player,
    int n_players);
typedef int exit_callback_t(struct hexgame *game, player_t *player);

typedef struct hexgame {
    int frame_i;
    bool show_minimap;
    hexmap_tileset_t minimap_tileset;

    ARRAY_DECL(char*, worldmaps)
    ARRAY_DECL(hexmap_t*, maps)
    ARRAY_DECL(camera_t*, cameras)
    ARRAY_DECL(player_t*, players)
    ARRAY_DECL(actor_t*, actors)

    /* Weakrefs: */
    prismelrenderer_t *prend;
    prismelrenderer_t *minimap_prend;
        /* May use a different space than prend, e.g. vec4_alt instead of vec4 */
    vecspace_t *space;
        /* should always be hexspace!
        NOT the same as prend->space! */
    void *app;
        /* app: the application which is running this game.
        The game isn't allowed to know anything about it, but it is
        used in some callbacks. */
    new_game_callback_t *new_game_callback;
    continue_callback_t *continue_callback;
    set_players_callback_t *set_players_callback;
    exit_callback_t *exit_callback;
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, prismelrenderer_t *prend,
    const char *worldmaps_filename,
    prismelrenderer_t *minimap_prend,
    const char *minimap_tileset_filename,
    const char *map_filename,
    void *app,
    new_game_callback_t *new_game_callback,
    continue_callback_t *continue_callback,
    set_players_callback_t *set_players_callback,
    exit_callback_t *exit_callback);
int hexgame_load_map(hexgame_t *game, const char *map_filename,
    hexmap_t **map_ptr);
int hexgame_get_or_load_map(hexgame_t *game, const char *map_filename,
    hexmap_t **map_ptr);
int hexgame_get_map_index(hexgame_t *game, hexmap_t *map);
int hexgame_reset_player(hexgame_t *game, player_t *player,
    int reset_level, hexmap_t *reset_map);
int hexgame_reset_player_by_keymap(hexgame_t *game, int keymap,
    int reset_level, hexmap_t *reset_map);
int hexgame_reset_players(hexgame_t *game, int reset_level,
    hexmap_t *reset_map);
int hexgame_process_event(hexgame_t *game, SDL_Event *event);
int hexgame_step(hexgame_t *game);
int hexgame_step_cameras(hexgame_t *game);


#endif