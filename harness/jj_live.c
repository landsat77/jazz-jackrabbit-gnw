/* Harness only (make LIVE=1): live heap allocations by call stack,
 * dumped at the end (jj_live_dump). */
#include <execinfo.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define N 65536
typedef struct { void *p; uint32_t size; void *bt[6]; int n; } rec_t;
static rec_t recs[N];
static rec_t peak_recs[N];
static uint64_t total, best;

static unsigned h(void *p) { return (unsigned)(((uintptr_t)p >> 4) * 2654435761u) % N; }

void jj_live_alloc(void *p, uint32_t size)
{
    unsigned i = h(p);
    while (recs[i].p) i = (i + 1) % N;
    recs[i].p = p;
    recs[i].size = size;
    total += size;
    void *bt[10];
    int n = backtrace(bt, 10);
    recs[i].n = 0;
    for (int k = 2; k < n && recs[i].n < 6; k++)
        recs[i].bt[recs[i].n++] = bt[k];
    if (total > best + 16384) {
        best = total;
        memcpy(peak_recs, recs, sizeof(recs));
    }
}

void jj_live_free(void *p)
{
    unsigned i = h(p);
    while (recs[i].p && recs[i].p != p) i = (i + 1) % N;
    if (!recs[i].p) return;
    total -= recs[i].size;
    recs[i].p = NULL;
    /* re-insert the following cluster */
    unsigned j = (i + 1) % N;
    while (recs[j].p) {
        rec_t r = recs[j];
        recs[j].p = NULL;
        unsigned k = h(r.p);
        while (recs[k].p) k = (k + 1) % N;
        recs[k] = r;
        j = (j + 1) % N;
    }
}

static void dump(const char *path, const rec_t *r)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (unsigned i = 0; i < N; i++) {
        if (!r[i].p) continue;
        fprintf(f, "%u", r[i].size);
        for (int k = 0; k < r[i].n; k++)
            fprintf(f, " %p", r[i].bt[k]);
        fprintf(f, "\n");
    }
    fclose(f);
}

void jj_live_dump(const char *path)
{
    char p[600];
    dump(path, recs);
    snprintf(p, sizeof(p), "%s.peak", path);
    dump(p, peak_recs);
}
