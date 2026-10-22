#include "minmax.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const int *a;
    size_t begin;
    size_t end;
    int verbose;
    long hold_ms;
    MinMax *result;
    pthread_mutex_t *lock;
    int err;
} Task;

static MinMax scan(const int *a, size_t begin, size_t end) {
    MinMax r = {a[begin], a[begin]};
    for (size_t i = begin + 1; i < end; ++i) {
        if (a[i] < r.min) {
            r.min = a[i];
        }
        if (a[i] > r.max) {
            r.max = a[i];
        }
    }
    return r;
}

static void *worker(void *arg) {
    Task *t = arg;
    MinMax local = scan(t->a, t->begin, t->end);

    if (t->verbose) {
        fprintf(stderr, "[thread tid=%ld] range [%zu, %zu) min=%d max=%d\n",
                (long)gettid(), t->begin, t->end, local.min, local.max);
    }

    if (t->hold_ms > 0) {
        struct timespec ts = {t->hold_ms / 1000, (t->hold_ms % 1000) * 1000000L};
        if (nanosleep(&ts, NULL) == -1 && errno != EINTR) {
            perror("nanosleep");
        }
    }

    t->err = pthread_mutex_lock(t->lock);
    if (t->err != 0) {
        return NULL;
    }
    if (local.min < t->result->min) {
        t->result->min = local.min;
    }
    if (local.max > t->result->max) {
        t->result->max = local.max;
    }
    t->err = pthread_mutex_unlock(t->lock);
    return NULL;
}

MinMax minmax_seq(const int *a, size_t n) {
    return scan(a, 0, n);
}

int minmax_parallel(const int *a, size_t n, int threads, int verbose, long hold_ms, MinMax *res) {
    if (n == 0 || threads < 1) {
        return EINVAL;
    }
    if ((size_t)threads > n) {
        threads = (int)n;
    }

    pthread_t *tids = malloc((size_t)threads * sizeof *tids);
    Task *tasks = malloc((size_t)threads * sizeof *tasks);
    if (tids == NULL || tasks == NULL) {
        free(tids);
        free(tasks);
        return ENOMEM;
    }

    pthread_mutex_t lock;
    int err = pthread_mutex_init(&lock, NULL);
    if (err != 0) {
        free(tids);
        free(tasks);
        return err;
    }

    res->min = a[0];
    res->max = a[0];

    size_t chunk = n / (size_t)threads;
    size_t rest = n % (size_t)threads;
    size_t begin = 0;
    int created = 0;

    for (int i = 0; i < threads; ++i) {
        size_t len = chunk + ((size_t)i < rest ? 1 : 0);
        tasks[i] = (Task){a, begin, begin + len, verbose, hold_ms, res, &lock, 0};
        begin += len;

        err = pthread_create(&tids[i], NULL, worker, &tasks[i]);
        if (err != 0) {
            break;
        }
        ++created;
    }

    for (int i = 0; i < created; ++i) {
        int join_err = pthread_join(tids[i], NULL);
        if (err == 0) {
            err = join_err != 0 ? join_err : tasks[i].err;
        }
    }

    int destroy_err = pthread_mutex_destroy(&lock);
    if (err == 0) {
        err = destroy_err;
    }
    free(tids);
    free(tasks);
    return err;
}
