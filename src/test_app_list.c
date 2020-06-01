
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

int test_app_list_cleanup_data(test_app_list_t *list){
    /* (This is a list->cleanup callback) */
    test_app_list_data_t *data = list->data;
    test_app_list_data_cleanup(data);
    free(data);
    return 0;
}

