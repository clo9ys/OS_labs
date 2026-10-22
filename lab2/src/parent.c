#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"

static int child_path(const char *argv0, char *buf, size_t size) {
    const char *slash = strrchr(argv0, '/');
    int n;
    if (slash == NULL) {
        n = snprintf(buf, size, "./child");
    } else {
        n = snprintf(buf, size, "%.*s/child", (int)(slash - argv0), argv0);
    }
    return (n < 0 || (size_t)n >= size) ? -1 : 0;
}

static void close_fd(int *fd) {
    if (*fd != -1 && close(*fd) == -1) {
        perror("close");
    }
    *fd = -1;
}

static pid_t spawn_child(const char *path, char *name, int pipes[2][2], int files[2], int i) {
    pid_t pid = fork();
    if (pid != 0) {
        return pid;
    }

    if (dup2(pipes[i][0], STDIN_FILENO) == -1 || dup2(files[i], STDOUT_FILENO) == -1) {
        perror("dup2");
        _exit(127);
    }
    for (int j = 0; j < 2; ++j) {
        close_fd(&pipes[j][0]);
        close_fd(&pipes[j][1]);
        close_fd(&files[j]);
    }
    signal(SIGPIPE, SIG_DFL);

    char *const args[] = {"child", name, NULL};
    execv(path, args);
    fprintf(stderr, "[%s pid=%ld] execv %s: %s\n", name, (long)getpid(), path, strerror(errno));
    _exit(127);
}

static int wait_child(const char *name, pid_t pid) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }
    if (WIFEXITED(status)) {
        fprintf(stderr, "[parent pid=%ld] %s pid=%ld exited with code %d\n",
                (long)getpid(), name, (long)pid, WEXITSTATUS(status));
        return WEXITSTATUS(status) == 0 ? 0 : -1;
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "[parent pid=%ld] %s pid=%ld killed by signal %d\n",
                (long)getpid(), name, (long)pid, WTERMSIG(status));
    }
    return -1;
}

int main(int argc, char *argv[]) {
    unsigned seed = (unsigned)time(NULL) ^ (unsigned)getpid();
    if (argc == 3 && strcmp(argv[1], "--seed") == 0) {
        seed = (unsigned)strtoul(argv[2], NULL, 10);
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--seed N]\n", argv[0]);
        return 2;
    }

    char path[PATH_MAX];
    if (child_path(argv[0], path, sizeof path) == -1) {
        fprintf(stderr, "path to child is too long\n");
        return 1;
    }
    fprintf(stderr, "[parent pid=%ld] started, child program: %s, seed=%u\n", (long)getpid(), path, seed);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    char *names[2] = {"child1", "child2"};
    int files[2] = {-1, -1};
    int pipes[2][2] = {{-1, -1}, {-1, -1}};
    pid_t pids[2] = {-1, -1};
    long sent[2] = {0, 0};
    long lost = 0;
    int rc = 1;

    LineReader in;
    lr_init(&in, STDIN_FILENO);
    int interactive = isatty(STDIN_FILENO);
    char *line;
    size_t len;
    LineStatus st;

    for (int i = 0; i < 2; ++i) {
        if (interactive) {
            fprintf(stderr, "File name for %s: ", names[i]);
        }
        st = lr_next(&in, &line, &len);
        if (st == LR_ERROR) {
            perror("read");
            goto out;
        }
        if (st == LR_EOF) {
            fprintf(stderr, "expected two file names\n");
            goto out;
        }
        files[i] = open(line, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (files[i] == -1) {
            fprintf(stderr, "open %s: %s\n", line, strerror(errno));
            goto out;
        }
    }

    struct stat info[2];
    for (int i = 0; i < 2; ++i) {
        if (fstat(files[i], &info[i]) == -1) {
            perror("fstat");
            goto out;
        }
    }
    if (info[0].st_dev == info[1].st_dev && info[0].st_ino == info[1].st_ino) {
        fprintf(stderr, "child1 and child2 must write to different files\n");
        goto out;
    }

    for (int i = 0; i < 2; ++i) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            goto out;
        }
    }

    for (int i = 0; i < 2; ++i) {
        pids[i] = spawn_child(path, names[i], pipes, files, i);
        if (pids[i] == -1) {
            perror("fork");
            goto out;
        }
        fprintf(stderr, "[parent pid=%ld] started %s pid=%ld\n", (long)getpid(), names[i], (long)pids[i]);
    }

    for (int i = 0; i < 2; ++i) {
        close_fd(&pipes[i][0]);
        close_fd(&files[i]);
    }

    srand(seed);
    long lineno = 0;
    while ((st = lr_next(&in, &line, &len)) == LR_LINE) {
        ++lineno;
        int i = (rand() % 100 < 80) ? 0 : 1;

        if (pipes[i][1] == -1) {
            ++lost;
            fprintf(stderr, "[parent pid=%ld] line %ld -> pipe%d: pipe is closed, line lost\n",
                    (long)getpid(), lineno, i + 1);
            continue;
        }
        if (write_all(pipes[i][1], line, len) == -1 || write_all(pipes[i][1], "\n", 1) == -1) {
            if (errno == EPIPE) {
                fprintf(stderr, "[parent pid=%ld] %s stopped reading pipe%d (EPIPE)\n",
                        (long)getpid(), names[i], i + 1);
            } else {
                perror("write");
            }
            close_fd(&pipes[i][1]);
            ++lost;
            continue;
        }
        ++sent[i];
        fprintf(stderr, "[parent pid=%ld] line %ld -> pipe%d (%s)\n", (long)getpid(), lineno, i + 1, names[i]);
    }
    if (st == LR_ERROR) {
        perror("read");
    } else if (lost == 0) {
        rc = 0;
    }

out:
    for (int i = 0; i < 2; ++i) {
        close_fd(&pipes[i][0]);
        close_fd(&pipes[i][1]);
        close_fd(&files[i]);
    }
    for (int i = 0; i < 2; ++i) {
        if (pids[i] > 0 && wait_child(names[i], pids[i]) == -1) {
            rc = 1;
        }
    }
    if (pids[0] > 0 && pids[1] > 0) {
        long total = sent[0] + sent[1];
        fprintf(stderr, "[parent pid=%ld] done: pipe1 %ld (%.1f%%), pipe2 %ld (%.1f%%), lost %ld\n",
                (long)getpid(), sent[0], total ? 100.0 * sent[0] / total : 0.0,
                sent[1], total ? 100.0 * sent[1] / total : 0.0, lost);
    }
    lr_free(&in);
    return rc;
}
