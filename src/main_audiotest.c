
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "util.h"
#include "audio.h"


int audiotest(SDL_AudioDeviceID dev, const char *filename){
    int err;

    audio_buffer_t buf;
    audio_parser_t parser;
    err = audio_parser_init(&parser, &buf);
    if(err)return err;

    printf("Loading %s...\n", filename);
    char *text = load_file(filename);
    if(text == NULL)return 1;
    {
        fus_lexer_t lexer;
        err = fus_lexer_init(&lexer, text, filename);
        if(err)return err;
        err = audio_parser_parse(&parser, &lexer);
        if(err)return err;
        fus_lexer_cleanup(&lexer);
    }

    SDL_PauseAudioDevice(dev, 0);

    printf("WARNING: don't define new variables with \"@def\", because "
        "the input buffer is overwritten every time you enter new text...\n");

    int pos = 0;
    while(1){
        int end = int_min(buf.data_len, parser.pos);
        int len = end - pos;
        printf("Queueing %i samples starting at %i...\n", len, pos);
        RET_IF_SDL_NZ(SDL_QueueAudio(dev, buf.data + pos,
            len * sizeof(audio_sample_t)));
        pos += len;

        printf("Parser is at sample %i/%i\n", parser.pos, buf.data_len);
        printf("Enter more music, or \"@exit\" to quit\n");
        printf("> ");
        char buffer[200];
        err = getln(buffer, 200);
        if(err)return err;
        if(!strcmp(buffer, "@exit"))break;

        fus_lexer_t lexer;
        err = fus_lexer_init(&lexer, buffer, "<user input>");
        if(err)return err;
        err = audio_parser_parse(&parser, &lexer);
        if(err)return err;
        fus_lexer_cleanup(&lexer);
    }

    audio_parser_cleanup(&parser);
    free(text);
    return 0;
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

    const char *filename = "data/music/test.fus";
    err = audiotest(dev, filename);
    if(err)return err;

    printf("Closing audio device...\n");
    SDL_CloseAudioDevice(dev);

    SDL_AudioQuit();
    printf("Quitting!\n");
    return 0;
}

