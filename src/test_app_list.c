
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"


/****************
* TEST_APP_LIST *
****************/

void test_app_list_init(test_app_list_t *list,
    const char *title, test_app_list_t *prev,
    int index_x, int index_y,
    void *data,
    test_app_list_callback_t *step,
    test_app_list_callback_t *render,
    test_app_list_callback_t *select_item,
    test_app_list_callback_t *cleanup
){
    memset(list, 0, sizeof(*list));
    list->title = title;
    list->prev = prev;
    list->index_x = index_x;
    list->index_y = index_y;
    list->data = data;
    list->step = step;
    list->render = render;
    list->select_item = select_item;
    list->cleanup = cleanup;
}

void test_app_list_cleanup(test_app_list_t *list){
    if(list->prev){
        test_app_list_cleanup(list->prev);
        free(list->prev);
    }
    if(list->cleanup){
        int err = list->cleanup(list);
        /* But do nothing with err, because we return void */
    }
}


/*********************
* TEST_APP_LIST_DATA *
*********************/

test_app_list_data_t *test_app_list_data_create(test_app_t *app){
    test_app_list_data_t *data = calloc(1, sizeof(*data));
    if(data == NULL)return NULL;
    data->app = app;
    return data;
}

void test_app_list_data_cleanup(test_app_list_data_t *data){
    /* Nuthin */
}


/*******************************
* TEST_APP_LIST_DATA CALLBACKS *
*******************************/

int test_app_list_cleanup_data(test_app_list_t *list){
    /* (Generic list->cleanup callback) */
    test_app_list_data_t *data = list->data;
    test_app_list_data_cleanup(data);
    free(data);
    return 0;
}


/***********
* TEST_APP *
***********/

int test_app_step_list(test_app_t *app){
    int err;

    err = app->list->step(app->list);
    if(err)return err;

    return 0;
}

static void _render_list_title(console_t *console, test_app_list_t *list){
    if(list->prev != NULL){
        _render_list_title(console, list->prev);
        console_write_msg(console, " -> ");
    }
    console_write_msg(console, list->title);
}

int test_app_render_list(test_app_t *app){
    int err;

    console_t *console = &app->console;
    console_clear(console);

    console_write_char(console, '(');
    _render_list_title(console, app->list);
    console_write_char(console, ')');
    console_newline(console);

    err = app->list->render(app->list);
    if(err)return err;

    return 0;
}

int test_app_process_event_list(test_app_t *app, SDL_Event *event){
    int err;

    /* Guaranteed to be non-NULL if we make it into this function: */
    test_app_list_t *list = app->list;

    switch(event->type){
        case SDL_KEYDOWN: {
            switch(event->key.keysym.sym){

                /* For now, we're lazy: we don't check indexes against length, so the
                callbacks need to handle wraparound themselves */
                case SDLK_UP: {
                    list->index_y--;
                } break;
                case SDLK_DOWN: {
                    list->index_y++;
                } break;
                case SDLK_LEFT: {
                    list->index_x--;
                } break;
                case SDLK_RIGHT: {
                    list->index_x++;
                } break;

                case SDLK_RETURN: {
                    if(list->select_item){
                        err = list->select_item(list);
                        if(err)return err;
                    }
                } break;

                case SDLK_BACKSPACE: {
                    if(list->back){
                        err = list->back(list);
                        if(err)return err;
                    }else{
                        /* Default "back" action if no callback */
                        err = test_app_close_list(app);
                        if(err)return err;
                    }
                } break;

                default: break;
            }
        } break;
        default: break;
    }
    return 0;
}

int test_app_open_list(test_app_t *app, const char *title,
    int index_x, int index_y,
    void *data,
    test_app_list_callback_t *step,
    test_app_list_callback_t *render,
    test_app_list_callback_t *select_item,
    test_app_list_callback_t *cleanup
){
    test_app_list_t *new_list = malloc(sizeof(*new_list));
    if(new_list == NULL)return 1;
    test_app_list_init(new_list, title, app->list,
        index_x, index_y,
        data, step, render, select_item, cleanup);
    app->list = new_list;
    return 0;
}

int test_app_close_list(test_app_t *app){
    int err;

    test_app_list_t *prev = app->list->prev;
    app->list->prev = NULL;
        /* Set list->prev to NULL so it's not cleaned up, since we're
        going to use it */
    test_app_list_cleanup(app->list);
    app->list = prev;

    if(!app->list){
        /* App is back in "console mode", so re-render console's input, which
        list's render probably erased */
        console_clear(&app->console);
        console_write_msg(&app->console, CONSOLE_START_TEXT);
        console_write_msg(&app->console, app->console.input);
    }

    return 0;
}
