
#ifndef _FUS_UTIL_H_
#define _FUS_UTIL_H_

#include <stdlib.h>

char *load_file(const char *filename);
size_t strnlen(const char *s, size_t maxlen);
char *strdup(const char *s1);
char *strndup(const char *s1, size_t len);

#endif
