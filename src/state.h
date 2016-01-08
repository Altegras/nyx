/* Copyright 2014-2015 Gregor Uhlenheuer
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

#ifndef __NYX_STATE_H__
#define __NYX_STATE_H__

#include "event.h"
#include "nyx.h"
#include "timestack.h"
#include "watch.h"

#include <semaphore.h>
#include <stdlib.h>
#include <sys/types.h>

typedef enum
{
    STATE_INIT,
    STATE_UNMONITORED,
    STATE_STARTING,
    STATE_RUNNING,
    STATE_STOPPING,
    STATE_STOPPED,
    STATE_RESTARTING,
    STATE_QUIT,
    STATE_SIZE
} state_e;

typedef struct
{
    pid_t pid;
    state_e state;
    sem_t *sem;
    pthread_t *thread;
    watch_t *watch;
    timestack_t *history;
    nyx_t *nyx;
} state_t;

const char *
state_to_human_string(state_e state);

state_t *
state_new(watch_t *watch, nyx_t *nyx);

void
state_destroy(state_t *state);

void
state_loop(state_t *state);

void *
state_loop_start(void *state);

state_t*
find_state_by_pid(list_t *states, pid_t pid);

void
set_state(state_t *state, state_e value);

int
dispatch_event(int pid, process_event_data_t *event_data, nyx_t *nyx);

int
dispatch_poll_result(int pid, int running, nyx_t *nyx);

#endif

/* vim: set et sw=4 sts=4 tw=80: */
