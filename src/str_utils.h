#ifndef _STR_UTILS_H_
#define _STR_UTILS_H_

#include <stdlib.h>
#include <stdbool.h>

bool streq(const char *s1, const char *s2);
char *strdup(const char *s1);
char *strdupcat(const char *s1, const char *s2);
size_t strnlen(const char *s, size_t maxlen);
char *strndup(const char *s1, size_t len);
int strlen_of_int(int i);
void strncpy_of_int(char *s, int i, int i_len);
char *strdup_of_int(int i);

#endif