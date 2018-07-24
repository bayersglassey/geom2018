
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "util.h"
#include "audio.h"


int main(int n_args, char *args[]){
    int err;

    if(SDL_Init(SDL_INIT_AUDIO)){
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 2;
    }

    SDL_AudioSpec want;
    SDL_AudioDeviceID dev;

    SDL_zero(want);
    want.freq = 4800;
    want.format = AUDIO_S16;
    want.channels = 1;
    want.samples = 4096;
    dev = SDL_OpenAudioDevice(NULL, false, &want, NULL,
        SDL_AUDIO_ALLOW_ANY_CHANGE);
    if(dev == 0){
        fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
        return 2;
    }

    const char *filename = "data/music/test.fus";
    printf("Loading %s...\n", filename);
    audio_buffer_t buf;
    err = audio_buffer_load(&buf, filename);
    if(err)return err;

    printf("Queueing %i samples...\n", buf.data_len);
    RET_IF_SDL_NZ(SDL_QueueAudio(dev, buf.data,
        buf.data_len * sizeof(audio_sample_t)));

    SDL_PauseAudioDevice(dev, 0);

    printf("Playing!.. press Enter to quit.\n");
    char c = getchar();

    SDL_CloseAudioDevice(dev);

    SDL_AudioQuit();
    printf("Quitting!\n");
    return 0;
}

