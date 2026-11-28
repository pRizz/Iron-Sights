/* A stable sort for Baked Lighting's tree build (MAC-002 Task 5).
 *
 * port_baked_trace.c splits each node at the median of its triangles sorted
 * by centroid along one axis, and many centroids are equal there (level
 * geometry is full of axis-aligned quads). Which of two equal triangles goes
 * left then depends on the sort: glibc's qsort is a merge sort and keeps
 * equal keys in the order they came, macOS's qsort does not. The trees
 * differed, and with them which of two surfaces at one distance a ray hits,
 * so a macOS bake coloured a few vertices otherwise than a Linux one (11 of
 * 38 comparison cases with every fork option on). A macOS build sorts with
 * this instead, so its bake is the Linux builds'. The other builds keep
 * SDL_qsort and compile nothing here. scripts/port/test_baked_trace.sh tests
 * it on every host. */
#ifndef PORT_BAKED_SORT_H
#define PORT_BAKED_SORT_H

#include <stdint.h>

/* Sorts ORDER's COUNT triangle indices by CENT[index * 3 + AXIS], smallest
 * first; indices with equal keys keep the order they had. SCRATCH is COUNT
 * entries of working space. A bottom-up merge sort. */
static inline void portBakeStableSort(uint32_t* order, uint32_t count, uint32_t* scratch, const float* cent,
                                      int axis) {
    for (uint32_t width = 1; width < count; width *= 2) {
        for (uint32_t lo = 0; lo < count; lo += 2 * width) {
            const uint32_t mid = lo + width < count ? lo + width : count;
            const uint32_t hi = lo + 2 * width < count ? lo + 2 * width : count;
            uint32_t a = lo, b = mid, out = lo;
            /* the right run's entry goes first only when its key is smaller */
            while (a < mid && b < hi) {
                scratch[out++] = cent[order[b] * 3 + axis] < cent[order[a] * 3 + axis] ? order[b++] : order[a++];
            }
            while (a < mid) scratch[out++] = order[a++];
            while (b < hi) scratch[out++] = order[b++];
        }
        /* a loop, not memcpy: port_baked_trace.c includes only SDL.h */
        for (uint32_t i = 0; i < count; i++) order[i] = scratch[i];
    }
}

#endif
