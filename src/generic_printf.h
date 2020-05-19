#ifndef _GENERIC_PRINTF_H_
#define _GENERIC_PRINTF_H_

#include <stdarg.h>

typedef int generic_putc_callback_t(void *data, char c);

int generic_printf(generic_putc_callback_t *callback, void *callback_data,
    const char *msg, ...);
int generic_vprintf(generic_putc_callback_t *callback, void *callback_data,
    const char *msg, va_list vlist);

#endif