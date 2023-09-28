// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
//
// Derived from an extract of memtest86+ test.c:
//
// MemTest86+ V5 Specific code (GPL V2.0)
// By Samuel DEMEULEMEESTER, sdemeule@memtest.org
// http://www.canardpc.com - http://www.memtest.org
// Thanks to Passmark for calculate_chunk() and various comments !
// ----------------------------------------------------
// test.c - MemTest-86  Version 3.4
//
// Released under version 2 of the Gnu Public License.
// By Chris Brady

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "error.h"
#include "test.h"
#include "config.h"

#define WANT_SIMD

#include "test_funcs.h"
#include "test_helper.h"

#define HAND_OPTIMISED  1   // Use hand-optimised assembler code for performance.

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

#if 0
// XXX how to make this work without producing GCC error: 'SSE register return with SSE disabled' ?
#pragma GCC target ("mmx", "no-sse", "no-sse2", "no-avx")
static __attribute__((noinline)) void loops_simd64(testword_t *p, testword_t *pe, testword_t pattern1)
{
    register __m64 pattern __asm__("%mm0") = convert_testword_to_simd64(pattern1);
    if (enable_nontemporal) {
        do {
            write64_simd_nt((__m64 *)p, pattern);
            p += (sizeof(*p) < 8) ? 1 : 0;
        } while (p++ < pe); // test before increment in case pointer overflows
    }
    else {
        do {
            write64_simd((__m64 *)p, pattern);
            p += (sizeof(*p) < 8) ? 1 : 0;
        } while (p++ < pe); // test before increment in case pointer overflows
    }
    __asm__ __volatile__ ("emms");
    __sync_synchronize();
}
#endif

#pragma GCC target ("mmx", "no-sse", "no-sse2", "no-avx")
static __attribute__((noinline)) void loops_simd64(testword_t *p, testword_t *pe, testword_t pattern1)
{
    if (enable_nontemporal) {
        do {
            write64_simd_nt(p, pattern1);
            p += (sizeof(*p) < 8) ? 1 : 0;
        } while (p++ < pe); // test before increment in case pointer overflows
    }
    else {
        do {
            write64_simd(p, pattern1);
            p += (sizeof(*p) < 8) ? 1 : 0;
        } while (p++ < pe); // test before increment in case pointer overflows
    }
    __asm__ __volatile__ ("emms");
    __sync_synchronize();
}

#ifdef __x86_64__
#pragma GCC target ("sse2", "no-avx")
#else
#pragma GCC target ("sse", "no-sse2", "no-avx")
#endif
static __attribute__((noinline)) void loops_simd128(testword_t *p, testword_t *pe, testword_t pattern1)
{
    __m128 pattern = convert_testword_to_simd128(pattern1);
    if (enable_nontemporal) {
        do {
            write128_simd_nt((__m128 *)p, pattern);
            p += (sizeof(*p) < 8) ? 3 : 1;
        } while (p++ < pe); // test before increment in case pointer overflows
    }
    else {
        do {
            write128_simd((__m128 *)p, pattern);
            p += (sizeof(*p) < 8) ? 3 : 1;
        } while (p++ < pe); // test before increment in case pointer overflows
    }
    __sync_synchronize();
}

#ifdef __x86_64__
#pragma GCC target ("avx")
static __attribute__((noinline)) void loops_simd256(testword_t *p, testword_t *pe, testword_t pattern1)
{
    if (enable_nontemporal) {
        do {
            write256_simd_nt(p, pattern1);
            p += (sizeof(*p) < 8) ? 7 : 3;
        } while (p++ < pe); // test before increment in case pointer overflows
    }
    else {
        do {
            write256_simd(p, pattern1);
            p += (sizeof(*p) < 8) ? 7 : 3;
        } while (p++ < pe); // test before increment in case pointer overflows
    }
    __sync_synchronize();
}
#endif

int test_mov_inv_fixed(int my_cpu, int iterations, testword_t pattern1, testword_t pattern2, int simd)
{
    int ticks = 0;

    if (my_cpu == master_cpu) {
        display_test_pattern_value(pattern1);
    }
    size_t chunk_align = simd == 1 ? 64/8 : (simd == 2 ? 128/8 : (simd == 3 ? 256/8 : sizeof(testword_t)));

    // Initialize memory with the initial pattern.
    for (int i = 0; i < vm_map_size; i++) {
        testword_t *start, *end;
        calculate_chunk(&start, &end, my_cpu, i, chunk_align);
        __asm__ volatile("nop");
        if ((end - start) < (simd == 0 ? 1 : (32/8 << simd) / sizeof(testword_t)) SKIP_RANGE(1) // we need enough words for this test

        testword_t *p  = start;
        testword_t *pe = start;

        bool at_end = false;
        do {
            // take care to avoid pointer overflow
            if ((end - pe) >= SPIN_SIZE) {
                pe += SPIN_SIZE - 1;
            } else {
                at_end = true;
                pe = end;
            }
            ticks++;
            if (my_cpu < 0) {
                continue;
            }
            test_addr[my_cpu] = (uintptr_t)p;
            if (!simd) {
#if HAND_OPTIMISED
#ifdef __x86_64__
                uint64_t length = pe - p + 1;
                __asm__  __volatile__ ("\t"
                    "rep    \n\t"
                    "stosq  \n\t"
                    :
                    : "c" (length), "D" (p), "a" (pattern1)
                    :
                );
                p = pe;
#else
                uint32_t length = pe - p + 1;
                __asm__  __volatile__ ("\t"
                    "rep    \n\t"
                    "stosl  \n\t"
                    :
                    : "c" (length), "D" (p), "a" (pattern1)
                    :
                );
                p = pe;
#endif
#else
                do {
                    write_word(p, pattern1);
                } while (p++ < pe); // test before increment in case pointer overflows
#endif
            }

// SIMD code paths
            else if (simd == 1) {
                loops_simd64(p, pe, pattern1);
            }
            else if (simd == 2) {
                loops_simd128(p, pe, pattern1);
            }
#ifdef __x86_64__
            else if (simd == 3) {
                loops_simd256(p, pe, pattern1);
            }
#endif
            do_tick(my_cpu);
            BAILOUT;
        } while (!at_end && ++pe); // advance pe to next start point
    }

    // Check for the current pattern and then write the alternate pattern for
    // each memory location. Test from the bottom up and then from the top down.
    for (int i = 0; i < iterations; i++) {
        flush_caches(my_cpu);

        for (int j = 0; j < vm_map_size; j++) {
            testword_t *start, *end;
            calculate_chunk(&start, &end, my_cpu, j, chunk_align);
            if ((end - start) < (simd == 0 ? 1 : (32/8 << simd) / sizeof(testword_t)) SKIP_RANGE(1) // we need enough words for this test

            testword_t *p  = start;
            testword_t *pe = start;

            bool at_end = false;
            do {
                // take care to avoid pointer overflow
                if ((end - pe) >= SPIN_SIZE) {
                    pe += SPIN_SIZE - 1;
                } else {
                    at_end = true;
                    pe = end;
                }
                ticks++;
                if (my_cpu < 0) {
                    continue;
                }
                test_addr[my_cpu] = (uintptr_t)p;
                do {
                    testword_t actual = read_word(p);
                    if (unlikely(actual != pattern1)) {
                        data_error(p, pattern1, actual, true);
                    }
                    write_word(p, pattern2);
                } while (p++ < pe); // test before increment in case pointer overflows
                do_tick(my_cpu);
                BAILOUT;
            } while (!at_end && ++pe); // advance pe to next start point
        }

        flush_caches(my_cpu);

        for (int j = vm_map_size - 1; j >= 0; j--) {
            testword_t *start, *end;
            calculate_chunk(&start, &end, my_cpu, j, chunk_align);
            if ((end - start) < (simd == 0 ? 1 : (32/8 << simd) / sizeof(testword_t)) SKIP_RANGE(1) // we need enough words for this test

            testword_t *p  = end;
            testword_t *ps = end;

            bool at_start = false;
            do {
                // take care to avoid pointer underflow
                if ((ps - start) >= SPIN_SIZE) {
                    ps -= SPIN_SIZE - 1;
                } else {
                    at_start = true;
                    ps = start;
                }
                ticks++;
                if (my_cpu < 0) {
                    continue;
                }
                test_addr[my_cpu] = (uintptr_t)p;
                do {
                    testword_t actual = read_word(p);
                    if (unlikely(actual != pattern2)) {
                        data_error(p, pattern2, actual, true);
                    }
                    write_word(p, pattern1);
                } while (p-- > ps); // test before decrement in case pointer overflows
                do_tick(my_cpu);
                BAILOUT;
            } while (!at_start && --ps); // advance ps to next start point
        }

        __asm__ volatile("nop");
    }

    return ticks;
}
