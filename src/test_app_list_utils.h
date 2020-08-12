#ifndef _TEST_APP_LIST_UTILS_H_
#define _TEST_APP_LIST_UTILS_H_

/* Utilities for use with test_app_list_*.c */


#include "console.h"
#include "test_app.h"
#include "test_app_list.h"


#define WRITE_FIELD(OBJ, FIELD) _console_write_field(console, #FIELD, (OBJ)->FIELD);
#define WRITE_FIELD_BOOL(OBJ, FIELD) _console_write_field_bool(console, #FIELD, (OBJ)->FIELD);
#define WRITE_FIELD_INT(OBJ, FIELD) _console_write_field_int(console, #FIELD, (OBJ)->FIELD);
#define WRITE_FIELD_VEC(OBJ, DIMS, FIELD) _console_write_field_vec(console, #FIELD, (DIMS), (OBJ)->FIELD);


int _test_app_list_remainder(int a, int b);

void test_app_list_data_set_options(test_app_list_data_t *data,
    const char **options, int index);
void test_app_list_data_set_mode(test_app_list_data_t *data, int mode);


void _console_write_bar(console_t *console, int index, int length);
void _console_write_section(console_t *console, const char *name);
void _console_write_field_header(console_t *console, const char *name);
void _console_write_field(console_t *console, const char *name, const char *value);
void _console_write_field_bool(console_t *console, const char *name, bool value);
void _console_write_field_int(console_t *console, const char *name, int value);
void _console_write_field_vec(console_t *console, const char *name, int dims, vec_t value);
void _console_write_keyinfo(console_t *console, body_t *body, keyinfo_t *keyinfo);
void _console_write_recording(console_t *console, recording_t *rec, bool show_data);
void _console_write_options(console_t *console,
    const char **options, int index, int length);
void _console_write_options_stateset(console_t *console,
    const char **options, int index, int length,
    stateset_t *stateset, state_t *cur_state);


#endif