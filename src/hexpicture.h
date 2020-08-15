#ifndef _HEXPICTURE_H_
#define _HEXPICTURE_H_

typedef struct hexpicture {
    int dummy;
} hexpicture_t;



void hexpicture_cleanup(hexpicture_t *pic);
void hexpicture_init(hexpicture_t *pic);
int hexpicture_parse(hexpicture_t *pic,
    const char **lines, size_t lines_len,
    bool verbose);



#endif