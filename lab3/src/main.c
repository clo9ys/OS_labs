#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "minmax.h"

#define MAX_THREADS 1024
#define MAX_REPEATS 10000
#define MAX_HOLD_MS 600000

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s -t threads [-n size] [-s seed] [-r repeats] [-w hold_ms] [-c] [-v] [-q]\n", prog);
}

static int parse_long(const char *s, long min, long max, long *out) {
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < min || v > max) {
        return -1;
    }
    *out = v;
    return 0;
}

static double now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("clock_gettime");
        exit(1);
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void fill_random(int *a, size_t n, uint64_t seed) {
    uint64_t x = seed != 0 ? seed : 88172645463325252ULL;
    for (size_t i = 0; i < n; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        a[i] = (int)(uint32_t)(x >> 32);
    }
}

int main(int argc, char *argv[]) {
    long threads = 0;
    long n = 10000000;
    long repeats = 1;
    long seed = 0;
    long hold_ms = 0;
    int have_seed = 0;
    int check = 0;
    int verbose = 0;
    int quiet = 0;

    int opt;
    while ((opt = getopt(argc, argv, "t:n:s:r:w:cvq")) != -1) {
        switch (opt) {
        case 't':
            if (parse_long(optarg, 1, MAX_THREADS, &threads) == -1) {
                fprintf(stderr, "bad thread count: %s (expected 1..%d)\n", optarg, MAX_THREADS);
                return 2;
            }
            break;
        case 'n':
            if (parse_long(optarg, 1, (long)(SIZE_MAX / sizeof(int)), &n) == -1) {
                fprintf(stderr, "bad array size: %s\n", optarg);
                return 2;
            }
            break;
        case 's':
            if (parse_long(optarg, 0, LONG_MAX, &seed) == -1) {
                fprintf(stderr, "bad seed: %s\n", optarg);
                return 2;
            }
            have_seed = 1;
            break;
        case 'r':
            if (parse_long(optarg, 1, MAX_REPEATS, &repeats) == -1) {
                fprintf(stderr, "bad repeat count: %s (expected 1..%d)\n", optarg, MAX_REPEATS);
                return 2;
            }
            break;
        case 'w':
            if (parse_long(optarg, 0, MAX_HOLD_MS, &hold_ms) == -1) {
                fprintf(stderr, "bad hold time: %s (expected 0..%d ms)\n", optarg, MAX_HOLD_MS);
                return 2;
            }
            break;
        case 'c':
            check = 1;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'q':
            quiet = 1;
            break;
        default:
            usage(argv[0]);
            return 2;
        }
    }
    if (threads == 0 || optind != argc) {
        usage(argv[0]);
        return 2;
    }
    if (!have_seed) {
        seed = (long)time(NULL);
    }

    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0 &&
        (unsigned long long)n * sizeof(int) > (unsigned long long)pages * (unsigned long long)page_size / 2) {
        fprintf(stderr, "array of %ld ints does not fit into half of physical memory (%lld MB)\n",
                n, (long long)pages * page_size / (1024 * 1024));
        return 1;
    }

    int *a = malloc((size_t)n * sizeof *a);
    if (a == NULL) {
        perror("malloc");
        return 1;
    }
    fill_random(a, (size_t)n, (uint64_t)seed);

    if (!quiet) {
        fprintf(stderr, "[main pid=%ld tid=%ld] n=%ld threads=%ld repeats=%ld seed=%ld\n",
                (long)getpid(), (long)gettid(), n, threads, repeats, seed);
    }

    MinMax res = {0, 0};
    double total = 0.0;
    double best = 0.0;
    for (long r = 0; r < repeats; ++r) {
        double start = now_ms();
        int err = minmax_parallel(a, (size_t)n, (int)threads, verbose, hold_ms, &res);
        double elapsed = now_ms() - start;
        if (err != 0) {
            fprintf(stderr, "minmax_parallel: %s\n", strerror(err));
            free(a);
            return 1;
        }
        total += elapsed;
        if (r == 0 || elapsed < best) {
            best = elapsed;
        }
    }

    if (quiet) {
        printf("%ld,%ld,%.4f,%.4f\n", n, threads, total / (double)repeats, best);
    } else {
        printf("min=%d max=%d\n", res.min, res.max);
        printf("time: avg %.3f ms, best %.3f ms (%ld run(s))\n", total / (double)repeats, best, repeats);
    }

    int rc = 0;
    if (check) {
        MinMax ref = minmax_seq(a, (size_t)n);
        if (ref.min == res.min && ref.max == res.max) {
            fprintf(stderr, "check: OK\n");
        } else {
            fprintf(stderr, "check: MISMATCH (sequential min=%d max=%d)\n", ref.min, ref.max);
            rc = 1;
        }
    }

    free(a);
    return rc;
}
