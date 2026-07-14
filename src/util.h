
#ifndef _FUS_UTIL_H_
#define _FUS_UTIL_H_

#include <stdlib.h>
#include <stdbool.h>

#include "str_utils.h"
#include "file_utils.h"

#ifndef NO_EXECINFO
#   include <execinfo.h> /* backtrace_symbols_fd */
#   define BACKTRACE(N) { \
    void *buffer[N]; \
    backtrace_symbols_fd(buffer, N, 2); \
}
#else
#   define BACKTRACE(N) ;
#endif

#define MAX_SPACES 256

bool get_bool_env(const char *name);
int int_min(int x, int y);
int int_max(int x, int y);
int randint(int min, int max);
int linear_interpolation(int x0, int x1, int t, int t_max);
void get_spaces(char *spaces, int max_spaces, int n_spaces);

#endif
