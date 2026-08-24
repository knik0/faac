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

#include "progress.h"

void progress_throttle_reset(progress_throttle_t *t)
{
    t->last_fired_sec = -1.0;
    t->reached_end = false;
}

bool progress_throttle_tick(progress_throttle_t *t, const progress_info_t *info,
                             double interval_sec)
{
    bool at_end = info->total_input_samples > 0 &&
                  info->current_input_samples >= info->total_input_samples;
    bool fire_edge = at_end && !t->reached_end;
    if (at_end)
        t->reached_end = true;

    bool fire = fire_edge || t->last_fired_sec < 0.0 ||
                (info->time_elapsed_sec - t->last_fired_sec) >= interval_sec;
    if (fire)
        t->last_fired_sec = info->time_elapsed_sec;
    return fire;
}
