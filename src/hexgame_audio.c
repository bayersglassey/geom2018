
#include <string.h>

#include "hexgame_audio.h"


void hexgame_audio_data_cleanup(hexgame_audio_data_t *data){
    vars_cleanup(&data->vars);
}


void hexgame_audio_data_init(hexgame_audio_data_t *data){
    vars_init(&data->vars);

    /* Private fields */
    data->_callback = NULL;
    data->_t = 0;
}


void hexgame_audio_data_set_callback(hexgame_audio_data_t *data, hexgame_audio_callback_t *callback){
    if(data->_callback != callback)data->_t = 0;
    data->_callback = callback;
}


static void _audio_callback(void *userdata, Uint8 *stream, int len){
    /* For use as the callback of a SDL_AudioSpec */
    hexgame_audio_data_t *data = userdata;
    if(data->_callback){
        data->_callback(data, stream, len);
    }else{
        /* Silence! */
        memset(stream, 0, len);
    }
}


int hexgame_audio_sdl_open_device(
    hexgame_audio_data_t *data,
    SDL_AudioSpec *want_spec,
    SDL_AudioSpec *spec,
    SDL_AudioDeviceID *audio_id_ptr
){
    SDL_zero(*want_spec);
    want_spec->freq = 8000;
    want_spec->format = AUDIO_U8;
    want_spec->channels = 2;
    want_spec->samples = 1024;
    want_spec->callback = &_audio_callback;
    want_spec->userdata = data;
    SDL_AudioDeviceID audio_id = SDL_OpenAudioDevice(
        NULL,
        0,
        want_spec,
        spec,
        SDL_AUDIO_ALLOW_SAMPLES_CHANGE
    );
    if(!audio_id){
        fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
        return 1;
    }
    *audio_id_ptr = audio_id;
    return 0;
}


#define SONG(_BODY) { \
    int _t = data->_t; \
    for(int _i = 0; _i < len; _i+=2){ \
        /* Generate data for each channel. */ \
        /* The variable "c" represents the current channel (0 or 1). */ \
        for(int _c = 0; _c < 2; _c++){ \
            int t = _t; \
            int c = _c; \
            Uint8 value = (_BODY); \
            buf[_i + c] = value; \
        } \
        _t++; \
    } \
    data->_t = _t; \
}


void hexgame_song_symphony(struct hexgame_audio_data *data, Uint8 *buf, int len){
    /* Based on "remix of miiro's 1-line symphony ":
    https://dollchan.net/bytebeat/#v3b64q1ZKzk9JVbJSstQqUSuxszOpMYUwzGuMIQxDA6VaAA
    */
    int x = vars_get_int(&data->vars, "x");
    SONG(
        (9-c*2)*t&t>>4*(x+1)|(5+c)*t&t>>7|(2+c)*t&t>>10*(x+1)
    )
}


void hexgame_song_house(struct hexgame_audio_data *data, Uint8 *buf, int len){
    /* Based on "electrohouse":
    https://dollchan.net/bytebeat/#v3b64q1ZKzk9JVbJSKrGzK1HVKFE1si9RMzYyN7OwNzG0KlEzNDO2MLE3NbQyM7QyNtFUAyozUaoFAA
    */
    SONG(
        t>>t%(t%2?t&32768?41:t&16384?51:61:34)&t>>4
    )
}


void hexgame_song_title(struct hexgame_audio_data *data, Uint8 *buf, int len){
    /* Based on "simple repeating bass":
    https://dollchan.net/bytebeat/#v3b64q1ZKzk9JVbJS0jDSKlEzMjXV1NLQLbGzM1czNDLXtLOzUKoFAA
    */
    SONG((
        t/(2<<c)&(2*t&255)*(-t>>7+c&255)>>8+c*2
    ))
}


hexgame_song_entry_t hexgame_songs[] = {
    {"symphony", &hexgame_song_symphony},
    {"house", &hexgame_song_house},
    {"title", &hexgame_song_title},
    {NULL}
};

hexgame_audio_callback_t *hexgame_songs_get(const char *name){
    hexgame_song_entry_t *entry = hexgame_songs;
    while(entry->name){
        if(!strcmp(entry->name, name))return entry->callback;
        entry++;
    }
    return NULL;
}
