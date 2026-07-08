// java_sort.c

#include "java_sort.h"
#include <stdlib.h>
#include <string.h>

static void mergeSort(void** src, void** dest, int low, int high, int off, JavaSortCmp cmp, void* ctx) {
    int length = high - low;
    if (length < 7) {
        for (int i = low; i < high; ++i) {
            for (int j = i; j > low && cmp(dest[j - 1], dest[j], ctx) > 0; --j) {
                void* t = dest[j];
                dest[j] = dest[j - 1];
                dest[j - 1] = t;
            }
        }
        return;
    }
    int destLow = low, destHigh = high;
    low += off;
    high += off;
    int mid = (int)((unsigned)(low + high) >> 1);
    mergeSort(dest, src, low, mid, -off, cmp, ctx);
    mergeSort(dest, src, mid, high, -off, cmp, ctx);
    if (cmp(src[mid - 1], src[mid], ctx) <= 0) {
        memcpy(dest + destLow, src + low, (size_t)length * sizeof(void*));
        return;
    }
    for (int i = destLow, p = low, q = mid; i < destHigh; ++i) {
        if (q >= high || (p < mid && cmp(src[p], src[q], ctx) <= 0)) dest[i] = src[p++];
        else dest[i] = src[q++];
    }
}

void JavaSort_sort(void** a, int n, JavaSortCmp cmp, void* ctx) {
    if (n <= 1) return;
    void** aux = (void**)malloc((size_t)n * sizeof(void*));
    if (!aux) return;
    memcpy(aux, a, (size_t)n * sizeof(void*));
    mergeSort(aux, a, 0, n, 0, cmp, ctx);
    free(aux);
}
