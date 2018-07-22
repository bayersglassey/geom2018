
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "util.h"

#define MAX_DATA_LEN 65535
//#define MAX_DATA_LEN 16777215

typedef Sint16 sample_t;



void gen_square(sample_t *data, int data_pos, int data_len, int freq, int vol){
    for(int i = 0; i < data_len; i++){
        data[data_pos + i] += i % freq == 0? vol: 0;
    }
}

void gen_triangle(sample_t *data, int data_pos, int data_len, int add, int vol){
    int y = 0;
    bool up = true;
    for(int i = 0; i < data_len; i++){
        data[data_pos + i] += y;
        y += up? add: -add;
        if(y >= vol)up = false;
        if(y <= -vol)up = true;
    }
}

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

    static const int data_len = 20000;
    sample_t data[MAX_DATA_LEN] = {0};

    gen_square(data, 2000, 10000 - 2000, 50, 5000);
    gen_square(data, 2500, 10000 - 2500, 100, 5000);
    gen_square(data, 3000, 10000 - 3000, 25, 5000);

    gen_triangle(data, 1000, 5000, 200, 5000);
    gen_triangle(data, 1500, 5000, 300, 5000);
    gen_triangle(data, 2000, 5000, 500, 5000);

    RET_IF_SDL_NZ(SDL_QueueAudio(dev, data, data_len));

    /*
        freq -- samples per second
        1/freq -- length of sample in seconds
        1000/freq -- length of sample in milliseconds
    */

    int delay = data_len*1000/want.freq;
    printf("Delaying for %ims\n", delay);
    SDL_PauseAudioDevice(dev, 0);
    SDL_Delay(delay + 1000);
    SDL_CloseAudioDevice(dev);

    SDL_AudioQuit();
    printf("Quitting!\n");
    return 0;
}

