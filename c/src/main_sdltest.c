
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

char driver_buf[200];

int getln(char buf[], int buf_len){
    if(!fgets(buf, buf_len, stdin)){
        perror("fgets failed");
        return 1;
    }
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

int main(int n_args, char *args[]){
    int err;
    char driver[200];

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)){
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 2;
    }

    int n_audio_devices = SDL_GetNumAudioDevices(false);
    printf("Audio devices:\n");
    for(int i = 0; i < n_audio_devices; i++){
        const char *device = SDL_GetAudioDeviceName(i, false);
        printf("  %s\n", device);
    }

    int n_drivers = SDL_GetNumVideoDrivers();
    printf("Video drivers:\n");
    for(int i = 0; i < n_drivers; i++){
        const char *driver = SDL_GetVideoDriver(i);
        printf("  %s\n", driver);
    }

    printf("Choose driver: ");
    err = getln(driver, 200); if(err)return err;
    printf("Driver %s initializing...", driver);
    fflush(stdout);
    if(SDL_VideoInit(driver)){
        fprintf(stderr, "SDL_VideoInit failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("done\n");

    printf("Fullscreen (y/n)? ");
    char fullscreen[200];
    err = getln(fullscreen, 200); if(err)return err;
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
        printf("Do something: ");
        err = getln(action, 200); if(err)return err;
        printf("Doing %s\n", action);
        if(strcmp(action, "exit") == 0)break;
    }

    SDL_DestroyWindow(window);
    SDL_VideoQuit();
    printf("Quitting!\n");
    return 0;
}
