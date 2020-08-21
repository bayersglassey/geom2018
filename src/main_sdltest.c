
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "util.h"


void show_usage(){
    printf(
        "Usage:\n"
        " -V           Video test\n"
        " -A           Audio test\n"
        " -h --help    Show this help screen\n"
    );
}

void print_audio_spec(SDL_AudioSpec *spec){
    printf("  freq = %i\n", spec->freq);
    printf("  format = %i\n", spec->format);
    printf("  channels = %i\n", spec->channels);
    printf("  silence = %i\n", spec->silence);
    printf("  samples = %i\n", spec->samples);
}

int audio_test(int n_args, char *args[]){
    int err;

    if(SDL_Init(SDL_INIT_AUDIO)){
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 2;
    }

    int n_audio_devices = SDL_GetNumAudioDevices(false);
    printf("Audio devices:\n");
    for(int i = 0; i < n_audio_devices; i++){
        const char *device = SDL_GetAudioDeviceName(i, false);
        printf("  %s\n", device);
    }

    printf("Choose device (empty string for best choice): ");
    char device[200];
    err = getln(device, 200, stdin); if(err)return err;

    SDL_AudioSpec want, have;
    SDL_AudioDeviceID device_id;

    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.channels = 1;
    want.samples = 4096;
    device_id = SDL_OpenAudioDevice(!strcmp(device, "")? NULL: device,
        false, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if(device_id == 0){
        fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
        return 2;
    }

    printf("WANTED:\n");
    print_audio_spec(&want);
    printf("GOT:\n");
    print_audio_spec(&have);

    SDL_AudioQuit();
    return 0;
}

int video_test(int n_args, char *args[]){
    int err;

    if(SDL_Init(SDL_INIT_VIDEO)){
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 2;
    }

    int n_drivers = SDL_GetNumVideoDrivers();
    printf("Video drivers:\n");
    for(int i = 0; i < n_drivers; i++){
        const char *driver = SDL_GetVideoDriver(i);
        printf("  %s\n", driver);
    }

    printf("Choose driver: ");
    char driver[200];
    err = getln(driver, 200, stdin); if(err)return err;

    printf("Driver %s initializing...", driver);
    fflush(stdout);
    if(SDL_VideoInit(driver)){
        fprintf(stderr, "SDL_VideoInit failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("done\n");

    printf("Fullscreen (y/n)? ");
    char fullscreen[200];
    err = getln(fullscreen, 200, stdin); if(err)return err;

    printf("Creating window...");
    fflush(stdout);
    SDL_Window* window = SDL_CreateWindow(
        "LA LA LAAA!!!",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640, 400, fullscreen[0] == 'y'? SDL_WINDOW_FULLSCREEN: 0);
    if(window == NULL){
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("done\n");

    while(1){
        char action[200];
        printf("Do something (\"exit\" to quit): ");
        err = getln(action, 200, stdin); if(err)return err;
        printf("Doing %s\n", action);
        if(strcmp(action, "exit") == 0)break;
    }

    SDL_DestroyWindow(window);
    SDL_VideoQuit();
    return 0;
}

int main(int n_args, char *args[]){
    int err = 0;

    bool did_something = false;

    for(int i = 1; i < n_args; i++){
        char *arg = args[i];
        if(!strcmp(arg, "-A")){
            err = audio_test(n_args - i - 1, &args[i + 1]);
            did_something = true;
            break;
        }else if(!strcmp(arg, "-V")){
            err = video_test(n_args - i - 1, &args[i + 1]);
            did_something = true;
            break;
        }else if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            show_usage();
            did_something = true;
            break;
        }else{
            printf("Unrecognized option: %s\n", arg);
            err = 2;
            break;
        }
    }

    if(!did_something)show_usage();

    printf("Quitting!\n");
    return 0;
}
