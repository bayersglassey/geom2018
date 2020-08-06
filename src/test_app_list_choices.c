
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"



/************************
* TEST_APP_LIST_CHOICES *
*************************/

int test_app_open_list_choices(test_app_t *app, const char *title,
    const char **options, int options_length
){
    test_app_list_data_t *new_data = test_app_list_data_create(app);
    if(new_data == NULL)return 1;

    new_data->options = options;
    new_data->options_length = options_length;
    new_data->options_index = 0;

    return test_app_open_list(app, title,
        0, 0,
        new_data,
        &test_app_list_choices_step,
        &test_app_list_choices_render,
        &test_app_list_choices_select_item,
        &test_app_list_cleanup_data);
}

int test_app_list_choices_step(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    data->length = 0;
    data->index = 0;
    data->item = NULL;
    data->options_index = _remainder(list->index_y, data->options_length);
    return 0;
}

int test_app_list_choices_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_options(console, data->options,
        data->options_index, data->options_length);
    return 0;
}

int test_app_list_choices_select_item(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    // "return" data->options_index to list->prev...
    return test_app_close_list(data->app);
}

