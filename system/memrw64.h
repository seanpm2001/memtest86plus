// SPDX-License-Identifier: GPL-2.0
#ifndef MEMRW64_H
#define MEMRW64_H
/**
 * \file
 *
 * Provides some 64-bit memory access functions. These stop the compiler
 * optimizing accesses which need to be ordered and atomic. Mostly used
 * for accessing memory-mapped hardware registers.
 *
 *//*
 * Copyright (C) 2021-2022 Martin Whitaker.
 */

#include <stdint.h>

/**
 * Reads and returns the value stored in the 64-bit memory location pointed
 * to by ptr.
 */
static inline uint64_t read64(const volatile uint64_t *ptr)
{
    uint64_t val;
    __asm__ __volatile__(
        "movq %1, %0"
        : "=r" (val)
        : "m" (*ptr)
        : "memory"
    );
    return val;
}

/**
 * Writes val to the 64-bit memory location pointed to by ptr.
 */
static inline void write64(const volatile uint64_t *ptr, uint64_t val)
{
    __asm__ __volatile__(
        "movq %1, %0"
        :
        : "m" (*ptr),
          "r" (val)
        : "memory"
    );
}

/**
 * Writes val to the 64-bit memory location pointed to by ptr, using non-temporal hint.
 */
static inline void write64_nt(const volatile uint64_t *ptr, uint64_t val)
{
    __asm__ __volatile__(
        "movnti %1, %0"
        :
        : "m" (*ptr),
          "r" (val)
        : "memory"
    );
}

#ifdef WANT_SIMD

#include <immintrin.h>

// XXX how to make this work without producing GCC error: 'SSE register return with SSE disabled' ?
#if 0
#pragma GCC target ("mmx", "no-sse", "no-sse2", "no-avx")
static inline __m64 convert_testword_to_simd64(uint64_t val)
{
    return (__m64)val;
}
#endif

/**
 * Writes val to the 64-bit memory location pointed to by ptr, using MMX register as source operand.
 */
#pragma GCC target ("mmx", "no-sse", "no-sse2", "no-avx")
static inline void write64_simd(const volatile uint64_t *ptr, uint64_t val)
{
    __m64 val2 = (__m64)val;
    __asm__ __volatile__(
        "movq %1, %0"
        :
        : "m" (*ptr),
          "y" (val2)
        : "memory"
    );
}

/**
 * Writes val to the 64-bit memory location pointed to by ptr, using MMX register as source operand, using non-temporal hint.
 */
#pragma GCC target ("mmx", "no-sse", "no-sse2", "no-avx")
static inline void write64_simd_nt(const volatile uint64_t *ptr, uint64_t val)
{
    __m64 val2 = (__m64)val;
    __asm__ __volatile__(
        "movntq %1, %0"
        :
        : "m" (*ptr),
          "y" (val2)
        : "memory"
    );
}

#pragma GCC target ("sse2")
static inline __m128 convert_testword_to_simd128(uint64_t val)
{
    return (__m128)_mm_set1_epi64x(val);
}

/**
 * Writes val to the 128-bit memory location pointed to by ptr, using SSE register as source operand.
 */
#pragma GCC target ("sse2")
static inline void write128_simd(const volatile __m128 *ptr, __m128 val)
{
    __asm__ __volatile__(
        "movdqa %1, %0"
        :
        : "m" (*ptr),
          "x" (val)
        : "memory"
    );
}

/**
 * Writes val to the 128-bit memory location pointed to by ptr, using SSE register as source operand, using non-temporal hint.
 */
#pragma GCC target ("sse2")
static inline void write128_simd_nt(const volatile __m128 *ptr, __m128 val)
{
    __asm__ __volatile__(
        "movntdq %1, %0"
        :
        : "m" (*ptr),
          "x" (val)
        : "memory"
    );
}

/**
 * Reads and returns the value stored in the 128-bit memory location pointed to by ptr.
 */
#pragma GCC target ("sse2")
static inline __m128 read128_simd(const volatile __m128 *ptr)
{
    __m128 val;
    __asm__ __volatile__(
        "movdqa %1, %0"
        : "=x" (val)
        : "m" (*ptr)
        : "memory"
    );
    return val;
}

#pragma GCC target ("sse2")
static inline int compare128_simd(__m128 val1, __m128 val2)
{
    return _mm_movemask_pd(_mm_cmpeq_pd((__m128d)val1, (__m128d)val2));
}

/**
 * Writes val to the 256-bit memory location pointed to by ptr, using AVX register as source operand.
 */
#pragma GCC target ("avx")
static inline void write256_simd(const volatile __m256 *ptr, __m256 val)
{
    __asm__ __volatile__(
        "vmovdqa %1, %0"
        :
        : "m" (*ptr),
          "x" (val)
        : "memory"
    );
}

/**
 * Writes val to the 256-bit memory location pointed to by ptr, using AVX register as source operand, using non-temporal hint.
 */
#pragma GCC target ("avx")
static inline void write256_simd_nt(const volatile __m256 *ptr, __m256 val)
{
    __asm__ __volatile__(
        "vmovntdq %1, %0"
        :
        : "m" (*ptr),
          "x" (val)
        : "memory"
    );
}

/**
 * Reads and returns the value stored in the 128-bit memory location pointed to by ptr.
 */
#pragma GCC target ("avx")
static inline __m256 read256_simd(const volatile __m256 *ptr)
{
    __m256 val;
    __asm__ __volatile__(
        "vmovdqa %1, %0"
        : "=x" (val)
        : "m" (*ptr)
        : "memory"
    );
    return val;
}

#pragma GCC target ("avx")
static inline __m256 convert_testword_to_simd256(uint64_t val)
{
    return (__m256)_mm256_set1_epi64x(val);
}

#pragma GCC target ("avx")
static inline int compare256_simd(__m256 val1, __m256 val2)
{
    return _mm256_movemask_pd(_mm256_cmp_pd((__m256d)val1, (__m256d)val2, 0));
}

#endif

/**
 * Writes val to the 64-bit memory location pointed to by ptr. Reads it
 * back (and discards it) to ensure the write is complete.
 */
static inline void flush64(const volatile uint64_t *ptr, uint64_t val)
{
    __asm__ __volatile__(
        "movl %1, %0\n"
        "movl %0, %1"
        :
        : "m" (*ptr),
          "r" (val)
        : "memory"
    );
}

#endif // MEMRW64_H
