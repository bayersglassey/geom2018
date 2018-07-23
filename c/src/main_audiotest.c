
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "util.h"

#define MAX_DATA_LEN 65535
//#define MAX_DATA_LEN 16777215

typedef Sint16 sample_t;


int min(int x, int y){return x < y? x: y;}
int max(int x, int y){return x > y? x: y;}


void limit(sample_t *data, int data_pos, int data_len, int maxvol){
    for(int i = 0; i < data_len; i++){
        data[data_pos + i] = min(data[data_pos + i], maxvol);
    }
}

void gen_square(sample_t *data, int data_pos, int data_len, int freq, int offset, int vol, int minval, int volinc){
    for(int i = 0; i < data_len; i++){
        int val = (i + offset) % freq == 0? vol: 0;
        data[data_pos + i] += max(max(val, minval), 0);
        vol += volinc;
    }
}

void gen_triangle(sample_t *data, int data_pos, int data_len, int add, int offset, int vol, int minval, int addinc1, int addinc2){
    int y = offset;
    bool up = true;
    for(int i = 0; i < data_len; i++){
        data[data_pos + i] += max(y, minval);
        y += up? add: -add;
        add += up? addinc1: addinc2;
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

    int delay = 0;

    for(int i = 0; i < 4; i++){
        static const int data_len = 20000;
        sample_t data[MAX_DATA_LEN] = {0};

        gen_square(data,    0, 10000 - 2000, 50 , 0, 5000, 0, -1);
        gen_square(data,  500, 10000 - 2500, 100, 0, 5000, 0, -2);
        gen_square(data, 1500, 10000 - 3000, 25 , 0, 5000, 3000, -1);
        gen_square(data, 1500, 10000 - 3000, 50 , 10, 5000, 2000, -2);
        gen_square(data, 1500, 10000 - 3000, 15 , 0, 2000, 1000, -3);

        int addinc1 = i == 0? 0: i+1;
        int addinc2 = i == 0? 0: -1;
        gen_triangle(data, 1000, 5000, 200, 0, 5000, -3000, addinc1, addinc2);
        gen_triangle(data, 1000, 5000, 400, 600, 5000, -3000, addinc1, addinc2);
        gen_triangle(data, 1500, 5000, 1000, 0, i*1000, -500, 0, 0);
        gen_triangle(data, 2000, 5000, 500, 0, 5000, 0, addinc1, addinc2);

        //limit(data, 1000, 1000, 5000);
        //limit(data, 3000, 1000, 7000);

        /*
        limit(data, 1000, 500, 4000);
        limit(data, 1500, 500, 3000);
        limit(data, 2000, 500, 2000);
        limit(data, 2500, 500, 1000);
        limit(data, 3000, 500, 4000);
        limit(data, 3500, 500, 3000);
        limit(data, 4000, 500, 2000);
        limit(data, 4500, 500, 1000);
        limit(data, 5000, 500, 4000);
        limit(data, 5500, 500, 3000);
        limit(data, 6000, 500, 2000);
        limit(data, 6500, 500, 1000);
        */

        RET_IF_SDL_NZ(SDL_QueueAudio(dev, data, data_len));

        /*
            freq -- samples per second
            1/freq -- length of sample in seconds
            1000/freq -- length of sample in milliseconds
        */

        delay += data_len*1000/want.freq;
    }

    SDL_PauseAudioDevice(dev, 0);
    printf("Delaying for %ims\n", delay);
    SDL_Delay(delay + 1000);
    SDL_CloseAudioDevice(dev);

    SDL_AudioQuit();
    printf("Quitting!\n");
    return 0;
}

