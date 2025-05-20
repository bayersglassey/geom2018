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
#include "hexgame_audio.h"


#define MAX_FRAME_I 554400
    /* Largest highly composite number smaller than 2^16 */

#define DEBUG_RECORDINGS false

#define HEXGAME_PLAYER_0 0
    /* player 0 is special... e.g. they control the menu */

/* RESET_TO_SAFETY is to player->safe_location, RESET_SOFT is to
player->respawn_location. */
enum reset_level {
    RESET_TO_SAFETY,
    RESET_SOFT
};


struct body;
struct actor;
struct hexgame;


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

enum recording_action {
    RECORDING_ACTION_NONE,
    RECORDING_ACTION_PLAY,
    RECORDING_ACTION_RECORD,
    RECORDING_ACTIONS
};

typedef struct recording_node {
    int type; /* enum recording_node_type */
    union {
        int wait;
        int key_c;
    } u;
} recording_node_t;

typedef struct recording {
    int action; /* enum recording_action */
    bool reacts;
    bool loop;
    bool resets_position; /* default: true, if false, looping doesn't reset body's position */
    const char *stateset_name;
    const char *state_name;

    hexgame_location_t loc0;

    keyinfo_t keyinfo;
    vars_t vars;

    ARRAY_DECL(struct recording_node, nodes)

    int frame_i;
    int node_i;
    int wait;
    const char *filename;
    FILE *file;
    int offset;

    /* Weakrefs: */
    struct body *body;
} recording_t;

const char *recording_action_msg(int action);

void recording_cleanup(recording_t *rec);
void recording_init(recording_t *rec);
void recording_reset(recording_t *rec);
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
    valexpr_t visible_expr;
    valexpr_t target_expr;
    vars_t vars;

    ARRAY_DECL(label_mapping_t*, label_mappings)

    recording_t recording;
        /* This can be used by player (while they create/edit
        a recording), actor (plays back recordings), or neither
        (e.g. bodies loaded by hexmap which simply play a recording
        on repeat, or body created when you press F10). */

    int frame_i;
    int cooldown;
    bool no_key_reset; /* don't reset keyinfo when cooldown reaches zero */
    int dead; /* enum body_dead */
    bool remove;
        /* marks body for removal (for use e.g. while iterating over bodies) */
    bool just_spawned;
        /* marks body as having been created this frame (so we shouldn't do the
        usual things in "step") */

    bool confused; /* Reverses l/r keys */

    bool out_of_bounds;
    bool touching_mappoint;

    /* Weakrefs: */
    struct hexgame *game;
    hexmap_t *map;
    hexmap_submap_t *cur_submap;
    palettemapper_t *palmapper;

    stateset_t *stateset;
        /* Bodies can be in a strange state where their stateset is NULL.
        This happens when the stateset_filename passed to body_init is NULL.
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
            * body->stateset == NULL
            * body->state == NULL (?)

        ...I would rather not have to support bodies with NULL statesets.
        Can we fix it somehow?.. */

    state_t *state;
} body_t;

void body_cleanup(body_t *body);
int body_init(body_t *body, struct hexgame *game, hexmap_t *map,
    const char *stateset_filename, const char *state_name,
    palettemapper_t *palmapper);
void body_dump(body_t *body, int depth);
int body_is_visible(body_t *body, bool *visible_ptr);
int body_is_target(body_t *body, bool *target_ptr);
int body_get_index(body_t *body);
bool body_is_done_for(body_t *body);
int body_respawn(body_t *body, vec_t pos, rot_t rot, bool turn,
    hexmap_t *map);
int body_add_body(body_t *body, body_t **new_body_ptr,
    const char *stateset_filename, const char *state_name,
    palettemapper_t *palmapper,
    vec_t addpos, rot_t addrot, bool turn);

struct player;
int body_unset_label_mapping(body_t *body, const char *label_name);
int body_set_label_mapping(body_t *body, const char *label_name,
    rendergraph_t *rgraph);
struct player *body_get_player(body_t *body);
struct actor *body_get_actor(body_t *body);
void body_flash_cameras(body_t *body, Uint8 r, Uint8 g, Uint8 b,
    int percent);
void body_reset_cameras(body_t *body);
void body_remove(body_t *body);
int body_move_to_map(body_t *body, hexmap_t *map);
int body_refresh_vars(body_t *body);
int body_relocate(body_t *body, const char *map_filename,
    hexgame_location_t *loc, const char *stateset_filename,
    const char *state_name);

int body_execute_procs(body_t *body, int type /* enum stateset_proc_type */);
int body_set_stateset(body_t *body, const char *stateset_filename,
    const char *state_name);
int body_set_state(body_t *body, const char *state_name,
    bool reset_cooldown);

struct actor;
struct hexgame;
int body_update_cur_submap(body_t *body);
int body_handle_rules(body_t *body, body_t *your_body);
int body_step(body_t *body, struct hexgame *game);
bool body_sends_collmsg(body_t *body, const char *msg);
collmsg_handler_t *body_get_collmsg_handler(body_t *body, const char *msg);
int body_collide_against_body(body_t *body, body_t *body_other);
int body_render(body_t *body,
    SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom,
    vec_t camera_renderpos, prismelmapper_t *mapper);
int body_render_rgraph(body_t *body, rendergraph_t *rgraph,
    SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom,
    vec_t camera_renderpos, prismelmapper_t *mapper,
    bool render_labels);

void body_keydown(body_t *body, int key_i);
void body_keyup(body_t *body, int key_i);
int body_get_key_i(body_t *body, char c);
char body_get_key_c(body_t *body, int key_i, bool absolute);

int body_load_recording(body_t *body, const char *filename, bool loop);
int body_play_recording(body_t *body);
int body_restart_recording(body_t *body, bool ignore_offset, bool reset_position);
int body_start_recording(body_t *body, const char *filename);
int body_stop_recording(body_t *body);
int body_record_keydown(body_t *body, int key_i);
int body_record_keyup(body_t *body, int key_i);


/**********
 * PLAYER *
 **********/

/* Ticks after touching a savepoint until you can use it again */
#define PLAYER_SAVEPOINT_COOLDOWN 10

typedef struct player {
    hexgame_savelocation_t respawn_location;
    hexgame_savelocation_t safe_location;

    int savepoint_cooldown;

    int keymap;
    SDL_Keycode key_code[KEYINFO_KEYS];

    /* Weakrefs: */
    struct hexgame *game;
    body_t *body;
} player_t;

void player_cleanup(player_t *player);
int player_init(player_t *player, struct hexgame *game, int keymap,
    hexgame_savelocation_t *respawn_location);

int player_get_index(player_t *player);
void player_dump(player_t *player, int depth);
int player_spawn_body(player_t *player);
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
    int wait;

    vars_t vars;

    /* Weakrefs: */
    struct hexgame *game;
    body_t *body;
    state_t *state;
    stateset_t *stateset;
} actor_t;

void actor_cleanup(actor_t *actor);
int actor_init(actor_t *actor, hexmap_t *map, body_t *body,
    const char *stateset_filename, const char *state_name,
    trf_t *trf);

int actor_get_index(actor_t *actor);
int actor_init_stateset(actor_t *actor, const char *stateset_filename,
    const char *state_name, hexmap_t *map);
int actor_set_state(actor_t *actor, const char *state_name);
int actor_handle_rules(actor_t *actor, body_t *your_body);
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
    int max_colors_fade;

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
void camera_start_fade(camera_t *camera, int max_colors_fade);
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
typedef int hexgame_save_callback_t(struct hexgame *game);

typedef struct minimap_state_mappoint {
    hexgame_location_t location;
} minimap_state_mappoint_t;

typedef struct minimap_state {
    int zoom;
        /* HACK: zoom is used as both a bool and an int.
        It's a bool in that it controls whether minimap is shown; but
        it's also an int in that when nonzeo, it represents the zoom
        of the minimap. */
    ARRAY_DECL(minimap_state_mappoint_t, mappoints)
    int cur_mappoint;
} minimap_state_t;

typedef struct hexgame {
    int frame_i;
    int unpauseable_frame_i;
        /* For animations which continue when game is paused */

    tileset_t minimap_tileset;

    minimap_state_t minimap_state;

    vars_t vars;

    ARRAY_DECL(char*, worldmaps)
    ARRAY_DECL(hexmap_t*, maps)
    ARRAY_DECL(stateset_t*, statesets)
    ARRAY_DECL(camera_t*, cameras)
    ARRAY_DECL(player_t*, players)
    ARRAY_DECL(actor_t*, actors)

    hexgame_save_callback_t *save_callback;
    void *save_callback_data;

    bool animate_palettes;

    bool anim_debug;
    body_t *anim_debug_body;
    actor_t *anim_debug_actor;

    bool have_audio;
    hexgame_audio_data_t audio_data;

    /* Weakrefs: */
    prismelrenderer_t *prend;
    prismelrenderer_t *minimap_prend;
        /* May use a different space than prend, e.g. vec4_alt instead of vec4 */
    vecspace_t *space;
        /* should always be hexspace!
        NOT the same as prend->space! */
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, prismelrenderer_t *prend,
    prismelrenderer_t *minimap_prend,
    const char *minimap_tileset_filename,
    hexgame_save_callback_t *save_callback, void *save_callback_data,
    bool have_audio);
int hexgame_load_worldmaps(hexgame_t *game, const char *worldmaps_filename);
int hexgame_load_map(hexgame_t *game, const char *map_filename,
    hexmap_t **map_ptr);
int hexgame_get_or_load_map(hexgame_t *game, const char *map_filename,
    hexmap_t **map_ptr);
int hexgame_get_or_load_stateset(hexgame_t *game, const char *filename,
    stateset_t **stateset_ptr);
int hexgame_get_map_index(hexgame_t *game, hexmap_t *map);
int hexgame_reset_player(hexgame_t *game, player_t *player,
    int reset_level, hexmap_t *reset_map);
player_t *hexgame_get_player_by_keymap(hexgame_t *game, int keymap);
int hexgame_reset_player_by_keymap(hexgame_t *game, int keymap,
    int reset_level, hexmap_t *reset_map);
int hexgame_reset_players(hexgame_t *game, int reset_level,
    hexmap_t *reset_map);
int hexgame_process_event(hexgame_t *game, SDL_Event *event);
int hexgame_unpauseable_step(hexgame_t *game);
int hexgame_step(hexgame_t *game);
int hexgame_step_cameras(hexgame_t *game);
int hexgame_update_audio_data(hexgame_t *game, camera_t *camera);
void hexgame_set_minimap_zoom(hexgame_t *game, int zoom);
int hexgame_use_mappoint(hexgame_t *game, hexmap_t *map,
    hexmap_submap_t *cur_submap);


#endif
