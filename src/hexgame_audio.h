#ifndef _HEXGAME_AUDIO_H_
#define _HEXGAME_AUDIO_H_

#include <SDL2/SDL.h>

#include "vars.h"


typedef struct hexgame_audio_data hexgame_audio_data_t;

typedef void hexgame_audio_callback_t(struct hexgame_audio_data *data, Uint8 *buf, int len);

struct hexgame_audio_data {
    vars_t vars;

    /* Private fields */
    int _t;
    hexgame_audio_callback_t *_callback;
};


void hexgame_audio_data_cleanup(hexgame_audio_data_t *data);
void hexgame_audio_data_init(hexgame_audio_data_t *data);
void hexgame_audio_data_set_callback(hexgame_audio_data_t *data, hexgame_audio_callback_t *callback);

int hexgame_audio_sdl_open_device(
    hexgame_audio_data_t *data,
    SDL_AudioSpec *want_spec,
    SDL_AudioSpec *spec,
    SDL_AudioDeviceID *audio_id_ptr);


typedef struct hexgame_song_entry {
    const char *name;
    hexgame_audio_callback_t *callback;
} hexgame_song_entry_t;

extern hexgame_song_entry_t hexgame_songs[];
hexgame_audio_callback_t *hexgame_songs_get(const char *name);

#endif
