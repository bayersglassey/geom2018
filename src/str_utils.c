
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "str_utils.h"


bool streq(const char *s1, const char *s2){
    if(s1 == NULL || s2 == NULL)return s1 == s2;
    return strcmp(s1, s2) == 0;
}

char *strdup(const char *s1){
    size_t s1_len = strlen(s1);
    char *s2 = malloc(s1_len + 1);
    if(s2 == NULL)return NULL;
    strcpy(s2, s1);
    return s2;
}

char *strdupcat(const char *s1, const char *s2){
    size_t s1_len = strlen(s1);
    size_t s2_len = strlen(s2);
    size_t s3_len = s1_len + s2_len;
    char *s3 = malloc(s3_len + 1);
    if(s3 == NULL)return NULL;

    strcpy(s3, s1);
    strcpy(s3+s1_len, s2);

    s3[s3_len] = '\0';
    return s3;
}

#ifdef _WANT_STRNLEN
size_t strnlen(const char *s, size_t maxlen){
    size_t len = 0;
    while(len < maxlen && s[len] != '\0')len++;
    return len;
}
#endif

#ifdef _WANT_STRNDUP
char *strndup(const char *s1, size_t len){
    size_t s_len = strnlen(s1, len);
    char *s2 = malloc(s_len + 1);
    if(s2 == NULL)return NULL;
    strncpy(s2, s1, len);
    s2[s_len] = '\0';
    return s2;
}
#endif

int strlen_of_int(int i){
    /* Basically log(i), except that strlen of "0" is 1, and strlen of a
    negative number includes a space for the '-' */
    if(i == 0)return 1;
    if(i < 0)return strlen_of_int(-i) + 1;
    int len = 0;
    while(i != 0){
        len++;
    i /= 10;
    }
    return len;
}

void strncpy_of_int(char *s, int i, int i_len){
    /* i_len should be strlen_of_int(i) */
    if(i == 0){
        *s = '0';
        return;}
    if(i < 0){
        *s = '-';
        strncpy_of_int(s+1, -i, i_len-1);
        return;}
    while(i_len > 0){
        s[i_len - 1] = '0' + i % 10;
        i /= 10;
        i_len--;
    }
}

char *strdup_of_int(int i){
    int i_len = strlen_of_int(i);
    char *s = malloc(sizeof(*s) * i_len + 1);
    if(s == NULL)return NULL;
    strncpy_of_int(s, i, i_len);
    s[i_len] = '\0';
    return s;
}
