// java_sort.h: Arrays.sort(Object[], Comparator), the legacy merge sort

#ifndef JAVA_SORT_H
#define JAVA_SORT_H

typedef int (*JavaSortCmp)(const void* a, const void* b, void* ctx);

// sorts the pointers in place, stable even with a comparator that never returns 0
void JavaSort_sort(void** a, int n, JavaSortCmp cmp, void* ctx);

#endif
