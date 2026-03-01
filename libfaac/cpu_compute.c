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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cpu_compute.h"

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
# ifdef _MSC_VER
#  include <intrin.h>
# elif defined(__GNUC__) || defined(__clang__)
#  include <cpuid.h>
# endif
#endif

unsigned int get_cpu_caps(void)
{
    unsigned int caps = CPU_CAP_NONE;

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
    unsigned int eax, ebx, ecx, edx;
    unsigned int max_leaf;

# ifdef _MSC_VER
    int cpu_info[4];
    __cpuid(cpu_info, 0);
    max_leaf = (unsigned int)cpu_info[0];
# else
    __cpuid(0, max_leaf, ebx, ecx, edx);
# endif

    if (max_leaf >= 1) {
# ifdef _MSC_VER
        __cpuid(cpu_info, 1);
        eax = (unsigned int)cpu_info[0];
        ebx = (unsigned int)cpu_info[1];
        ecx = (unsigned int)cpu_info[2];
        edx = (unsigned int)cpu_info[3];
# else
        __get_cpuid(1, &eax, &ebx, &ecx, &edx);
# endif
        if (edx & (1 << 26)) // SSE2
            caps |= CPU_CAP_SSE2;
    }
#endif

    return caps;
}