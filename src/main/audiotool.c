
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "../hexgame_audio.h"


const char *get_default_song_name(){
    return hexgame_songs[0].name;
}


static void print_help(){
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h | --help   This message\n");
    fprintf(stderr, "  -s | --song   The name of the song (default: %s)\n", get_default_song_name());
    fprintf(stderr, "  -l | --list   Lists all songs\n");
}


int main(int n_args, char **args){
    int err;

    const char *song_name = get_default_song_name();
    bool list_songs = false;

    for(int i = 1; i < n_args; i++){
        const char *arg = args[i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            return 0;
        }else if(!strcmp(arg, "-s") || !strcmp(arg, "--song")){
            song_name = args[++i];
        }else if(!strcmp(arg, "-l") || !strcmp(arg, "--list")){
            list_songs = true;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            print_help();
            return 2;
        }
    }

    if(list_songs){
        printf("Songs:\n");
        hexgame_song_entry_t *entry = hexgame_songs;
        while(entry->name){
            printf(" * %s\n", entry->name);
            entry++;
        }
        return 0;
    }

    hexgame_audio_callback_t *song = hexgame_songs_get(song_name);
    if(!song){
        fprintf(stderr, "Song not found: %s\n", song_name);
        return 1;
    }

    if(SDL_Init(SDL_INIT_AUDIO)){
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 2;
    }

    hexgame_audio_data_t data;
    hexgame_audio_data_init(&data);
    hexgame_audio_data_set_callback(&data, song);

    /* Open an audio device */
    SDL_AudioSpec want_spec, spec;
    SDL_AudioDeviceID audio_id;
    if(hexgame_audio_sdl_open_device(&data, &want_spec, &spec, &audio_id))return 1;

    /* Start playing sound */
    SDL_PauseAudioDevice(audio_id, 0);

    /* Infinite loop!.. */
    while(1)SDL_Delay(3);

    /* Cleanup... */
    hexgame_audio_data_cleanup(&data);
    SDL_Quit();
    return 0;
}
