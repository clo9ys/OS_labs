#ifndef MINMAX_H
#define MINMAX_H

#include <stddef.h>

typedef struct {
    int min;
    int max;
} MinMax;

MinMax minmax_seq(const int *a, size_t n);
int minmax_parallel(const int *a, size_t n, int threads, int verbose, long hold_ms, MinMax *res);

#endif
