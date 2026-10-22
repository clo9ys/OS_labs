#include "utils.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LR_CHUNK 4096

int write_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

void lr_init(LineReader *r, int fd) {
    r->fd = fd;
    r->buf = NULL;
    r->start = 0;
    r->len = 0;
    r->cap = 0;
    r->eof = 0;
}

void lr_free(LineReader *r) {
    free(r->buf);
    r->buf = NULL;
    r->start = 0;
    r->len = 0;
    r->cap = 0;
}

LineStatus lr_next(LineReader *r, char **line, size_t *len) {
    for (;;) {
        char *nl = NULL;
        if (r->len > r->start) {
            nl = memchr(r->buf + r->start, '\n', r->len - r->start);
        }
        if (nl != NULL) {
            *nl = '\0';
            *line = r->buf + r->start;
            *len = (size_t)(nl - *line);
            r->start += *len + 1;
            return LR_LINE;
        }
        if (r->eof) {
            if (r->start == r->len) {
                return LR_EOF;
            }
            r->buf[r->len] = '\0';
            *line = r->buf + r->start;
            *len = r->len - r->start;
            r->start = r->len;
            return LR_LINE;
        }

        if (r->start > 0) {
            memmove(r->buf, r->buf + r->start, r->len - r->start);
            r->len -= r->start;
            r->start = 0;
        }
        if (r->cap - r->len < LR_CHUNK + 1) {
            size_t new_cap = r->cap ? r->cap : LR_CHUNK + 1;
            while (new_cap - r->len < LR_CHUNK + 1) {
                new_cap *= 2;
            }
            char *p = realloc(r->buf, new_cap);
            if (p == NULL) {
                return LR_ERROR;
            }
            r->buf = p;
            r->cap = new_cap;
        }

        ssize_t n = read(r->fd, r->buf + r->len, LR_CHUNK);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return LR_ERROR;
        }
        if (n == 0) {
            r->eof = 1;
        } else {
            r->len += (size_t)n;
        }
    }
}

static size_t utf8_char_len(const char *s, size_t len, size_t i) {
    unsigned char c = (unsigned char)s[i];
    size_t n = 1;
    if ((c & 0xE0) == 0xC0) {
        n = 2;
    } else if ((c & 0xF0) == 0xE0) {
        n = 3;
    } else if ((c & 0xF8) == 0xF0) {
        n = 4;
    }
    if (i + n > len) {
        return 1;
    }
    for (size_t k = 1; k < n; ++k) {
        if (((unsigned char)s[i + k] & 0xC0) != 0x80) {
            return 1;
        }
    }
    return n;
}

void reverse_utf8(const char *src, size_t len, char *dst) {
    size_t i = 0;
    while (i < len) {
        size_t n = utf8_char_len(src, len, i);
        memcpy(dst + len - i - n, src + i, n);
        i += n;
    }
}
