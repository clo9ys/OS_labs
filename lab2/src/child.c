#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

int main(int argc, char *argv[]) {
    const char *name = argc > 1 ? argv[1] : "child";
    long pid = (long)getpid();

    fprintf(stderr, "[%s pid=%ld ppid=%ld] started\n", name, pid, (long)getppid());

    LineReader in;
    lr_init(&in, STDIN_FILENO);
    char *line;
    size_t len;
    long count = 0;
    int rc = 0;

    for (;;) {
        LineStatus st = lr_next(&in, &line, &len);
        if (st == LR_EOF) {
            break;
        }
        if (st == LR_ERROR) {
            perror("child: read");
            rc = 1;
            break;
        }

        char *out = malloc(len + 1);
        if (out == NULL) {
            perror("child: malloc");
            rc = 1;
            break;
        }
        reverse_utf8(line, len, out);
        out[len] = '\n';
        int write_ans = write_all(STDOUT_FILENO, out, len + 1);
        free(out);
        if (write_ans == -1) {
            perror("child: write");
            rc = 1;
            break;
        }
        ++count;
        fprintf(stderr, "[%s pid=%ld] line %ld reversed\n", name, pid, count);
    }

    if (rc == 0) {
        fprintf(stderr, "[%s pid=%ld] EOF on stdin, %ld line(s) processed, exiting\n", name, pid, count);
    }
    lr_free(&in);
    return rc;
}
