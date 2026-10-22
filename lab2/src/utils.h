#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

typedef enum { LR_LINE, LR_EOF, LR_ERROR } LineStatus;

typedef struct {
    int fd;
    char *buf;
    size_t start;
    size_t len;
    size_t cap;
    int eof;
} LineReader;

int write_all(int fd, const char *buf, size_t len);

void lr_init(LineReader *r, int fd);
LineStatus lr_next(LineReader *r, char **line, size_t *len);
void lr_free(LineReader *r);

void reverse_utf8(const char *src, size_t len, char *dst);

#endif
