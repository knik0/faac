/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2026 Nils Schimmelmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef FAAC_ATOMIC_H
#define FAAC_ATOMIC_H

/* One-shot initialization shared by every encoder handle in the process.
 * State runs 0 (untouched), 1 (init in flight), 2 (published). The first
 * caller to claim the state runs init and publishes with release; everyone
 * else observes with acquire, spinning only while init is in flight. */

#if !defined(__STDC_NO_ATOMICS__) && defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#include <stdatomic.h>
typedef atomic_int faac_once_t;
#define FAAC_ONCE_INIT 0
#define faac_once_load(p)    atomic_load_explicit((p), memory_order_acquire)
#define faac_once_publish(p) atomic_store_explicit((p), 2, memory_order_release)
static inline int faac_once_claim(faac_once_t *p)
{
    int expected = 0;
    return atomic_compare_exchange_strong_explicit(p, &expected, 1,
                                                   memory_order_acq_rel,
                                                   memory_order_acquire);
}
#elif defined(_MSC_VER)
#include <intrin.h>
/* The Interlocked family are full barriers on every MSVC target, which a
 * volatile read is not outside /volatile:ms. */
typedef long faac_once_t;
#define FAAC_ONCE_INIT 0
#define faac_once_load(p)    _InterlockedCompareExchange((p), 0, 0)
#define faac_once_publish(p) _InterlockedExchange((p), 2)
#define faac_once_claim(p)   (_InterlockedCompareExchange((p), 1, 0) == 0)
#else
/* No atomics on this toolchain: concurrent opens need caller serialization,
 * as they always did. */
typedef int faac_once_t;
#define FAAC_ONCE_INIT 0
#define faac_once_load(p)    (*(p))
#define faac_once_publish(p) (*(p) = 2)
#define faac_once_claim(p)   (*(p) == 0 ? (*(p) = 1) : 0)
#endif

static inline void faac_once_run(faac_once_t *state, void (*init)(void))
{
    if (faac_once_load(state) == 2)
        return;
    if (faac_once_claim(state))
    {
        init();
        faac_once_publish(state);
        return;
    }
    while (faac_once_load(state) != 2)
        ;
}

#endif /* FAAC_ATOMIC_H */
