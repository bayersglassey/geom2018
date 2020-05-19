
#include <stdarg.h>

#include "generic_printf.h"


int generic_printf(generic_putc_callback_t *callback, void *callback_data,
    const char *msg, ...
){
    int err = 0;
    va_list vlist;
    va_start(vlist, msg);
    err = generic_vprintf(callback, callback_data, msg, vlist);
    if(err)goto done;
done:
    va_end(vlist);
    return err;
}

int generic_vprintf(generic_putc_callback_t *callback, void *callback_data,
    const char *msg, va_list vlist
){
    int err;

    char c;
    while(c = *msg, c != '\0'){
        if(c == '%'){
            msg++;
            c = *msg;
            if(c != '%'){
                if(c == 'i'){
                    int i = va_arg(vlist, int);

                    /* 2^64 has 20 digits in base 10 */
                    static const int max_digits = 20;
                    int digits[max_digits];
                    int digit_i = 0;

                    /* in case i == 0 */
                    digits[0] = 0;

                    if(i < 0){
                        i = -i;
                        err = callback(callback_data, '-');
                        if(err)return err;
                    }

                    while(i > 0 && digit_i < max_digits){
                        digits[digit_i] = i % 10;
                        i /= 10;
                        digit_i++;
                    }

                    if(digit_i > 0)digit_i--;
                    while(digit_i >= 0){
                        char c = '0' + digits[digit_i];
                        err = callback(callback_data, c);
                        if(err)return err;
                        digit_i--;
                    }
                }else if(c == 'c'){
                    char c = va_arg(vlist, int);
                    err = callback(callback_data, c);
                    if(err)return err;
                }else if(c == 's'){
                    char *s = va_arg(vlist, char *);
                    char c;
                    while(c = *s, c != '\0'){
                        err = callback(callback_data, c);
                        if(err)return err;
                        s++;
                    }
                }else{
                    /* Unsupported percent+character, so just output them
                    back as-is */
                    err = callback(callback_data, '%');
                    if(err)return err;
                    err = callback(callback_data, c);
                    if(err)return err;
                }
                msg++;
                continue;
            }
        }

        err = callback(callback_data, c);
        if(err)return err;
        msg++;
    }

    return 0;
}
