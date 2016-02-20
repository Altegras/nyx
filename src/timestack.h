/* Copyright 2014-2016 Gregor Uhlenheuer
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <time.h>

typedef struct
{
    time_t time;
    int32_t value;
} timestack_elem_t;

typedef struct
{
    uint32_t count;
    uint32_t max;
    timestack_elem_t *elements;
} timestack_t;

timestack_t *
timestack_new(uint32_t max);

void
timestack_add(timestack_t *timestack, int32_t value);

void
timestack_clear(timestack_t *timestack);

int32_t
timestack_oldest(timestack_t *timestack);

int32_t
timestack_newest(timestack_t *timestack);

void
timestack_dump(timestack_t *timestack, const char* (*writer)(int32_t));

void
timestack_destroy(timestack_t *timestack);

/* vim: set et sw=4 sts=4 tw=80: */
