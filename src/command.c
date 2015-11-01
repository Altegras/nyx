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

#include "command.h"
#include "def.h"
#include "log.h"
#include "state.h"
#include "utils.h"

static int
handle_status_change(sender_callback_t *cb, const char **input, nyx_t *nyx, state_e new_state)
{
    const char *name = input[1];
    state_t *state = hash_get(nyx->state_map, name);

    if (state == NULL)
    {
        cb->sender(cb, "unknown watch '%s'", name);
        return 0;
    }

    /* request state change */
    set_state(state, new_state);
    cb->sender(cb, "requested %s for watch '%s'",
            state_to_human_string(new_state),
            name);

    return 1;
}

static void
send_strings(sender_callback_t *cb, const char *name, const char **strings)
{
    const char **string = strings;

    if (string == NULL || *string == NULL)
        return;

    cb->sender(cb, "%s:", name);

    while (*string)
    {
        cb->sender(cb, "  '%s'", *string);
        string++;
    }
}

static void
send_keys(sender_callback_t *cb, const char *name, hash_t *keys)
{
    if (keys == NULL || hash_count(keys) < 1)
        return;

    cb->sender(cb, "%s:", name);

    const char *key = NULL;
    void *data = NULL;
    hash_iter_t *iter = hash_iter_start(keys);

    while (hash_iter(iter, &key, &data))
    {
        const char *value = data;

        cb->sender(cb, "  %s: %s", key, value);
    }

    free(iter);
}

static int
handle_config(sender_callback_t *cb, const char **input, nyx_t *nyx)
{
    const char *name = input[1];
    state_t *state = hash_get(nyx->state_map, name);

    if (state == NULL)
    {
        cb->sender(cb, "unknown watch '%s'", name);
        return 0;
    }

    watch_t *watch = state->watch;

    cb->sender(cb, "name: %s", name);

    send_strings(cb, "start", watch->start);
    send_strings(cb, "stop", watch->stop);

    if (watch->stop_timeout)
        cb->sender(cb, "stop_timeout: %u", watch->stop_timeout);

    if (watch->dir)
        cb->sender(cb, "dir: %s", watch->dir);

    if (watch->uid)
        cb->sender(cb, "uid: %s", watch->uid);

    if (watch->gid)
        cb->sender(cb, "gid: %s", watch->gid);

    if (watch->max_memory)
        cb->sender(cb, "max_memory: %lu", watch->max_memory);

    if (watch->max_cpu)
        cb->sender(cb, "max_memory: %u", watch->max_cpu);

    if (watch->port_check)
        cb->sender(cb, "port_check: %u", watch->port_check);

    if (watch->http_check)
    {
        cb->sender(cb, "http_check: %s", watch->http_check);
        cb->sender(cb, "http_check_method: %s",
                http_method_to_string(watch->http_check_method));
        cb->sender(cb, "http_check_port: %u",
                watch->http_check_port ? watch->http_check_port : 80);
    }

    send_keys(cb, "env", watch->env);

    return 1;
}

static int
handle_history(sender_callback_t *cb, const char **input, nyx_t *nyx)
{
    const char *name = input[1];
    state_t *state = hash_get(nyx->state_map, name);

    if (state == NULL)
    {
        cb->sender(cb, "unknown watch '%s'", name);
        return 0;
    }

    if (state->history->count < 1)
        return 1;

    unsigned i = state->history->count;

    while (i-- > 0)
    {
        timestack_elem_t *elem = &state->history->elements[i];
        struct tm *time = localtime(&elem->time);

        cb->sender(cb, "%04d-%02d-%02dT%02d:%02d:%02d: %s",
            time->tm_year + 1900,
            time->tm_mon + 1,
            time->tm_mday,
            time->tm_hour,
            time->tm_min,
            time->tm_sec,
            state_to_human_string(elem->value));
    }

    return 1;
}

static int
handle_ping(sender_callback_t *cb, UNUSED const char **input, UNUSED nyx_t *nyx)
{
    return cb->sender(cb, "pong");
}

static int
handle_version(sender_callback_t *cb, UNUSED const char **input, UNUSED nyx_t *nyx)
{
    return cb->sender(cb, NYX_VERSION);
}

static int
handle_terminate(sender_callback_t *cb, UNUSED const char **input, nyx_t *nyx)
{
    /* trigger the eventfd */
    signal_eventfd(4, nyx);

    /* trigger the termination handler (if specified) */
    if (nyx->terminate_handler)
        nyx->terminate_handler(0);

    return cb->sender(cb, "ok");
}

static int
handle_quit(sender_callback_t *cb, const char **input, nyx_t *nyx)
{
    if (nyx->states)
    {
        list_node_t *node = nyx->states->head;

        /* first we trigger the stop signal on all states */
        while (node)
        {
            state_t *state = node->data;

            if (state->state != STATE_STOPPED)
                set_state(state, STATE_STOPPING);

            node = node->next;
        }
    }

    /* after that we execute the termination handler */
    return handle_terminate(cb, input, nyx);
}

static int
handle_stop(sender_callback_t *cb, const char **input, nyx_t *nyx)
{
    return handle_status_change(cb, input, nyx, STATE_STOPPING);
}

static int
handle_restart(sender_callback_t *cb, const char **input, nyx_t *nyx)
{
    return handle_status_change(cb, input, nyx, STATE_RESTARTING);
}

static int
handle_start(sender_callback_t *cb, const char **input, nyx_t *nyx)
{
    return handle_status_change(cb, input, nyx, STATE_STARTING);
}

static int
handle_watches(sender_callback_t *cb, UNUSED const char **input, nyx_t *nyx)
{
    if (!nyx->states)
        return 0;

    list_node_t *node = nyx->states->head;

    while (node)
    {
        state_t *state = node->data;

        if (!state)
            continue;

        cb->sender(cb, "%s", state->watch->name);

        node = node->next;
    }

    return 1;
}

static int
handle_reload(sender_callback_t *cb, UNUSED const char **input, nyx_t *nyx)
{
    if (!nyx->options.config_file)
    {
        cb->sender(cb, "no config file to reload");
        return 0;
    }

    nyx_reload(nyx);
    cb->sender(cb, "ok");

    return 1;
}

static int
handle_status(sender_callback_t *cb, const char **input, nyx_t *nyx)
{
    const char *name = input[1];
    state_t *state = hash_get(nyx->state_map, name);

    if (state == NULL)
    {
        cb->sender(cb, "unknown watch '%s'", name);
        return 0;
    }

    /* print pid if running */
    if (state->state == STATE_RUNNING && state->pid)
    {
        cb->sender(cb, "%s: %s (PID %d)",
                name,
                state_to_human_string(state->state),
                state->pid);
    }
    else
        cb->sender(cb, "%s: %s", name, state_to_human_string(state->state));

    return 1;
}

#define CMD(t, n, h, a, d) \
    { .type = t, .name = n, .handler = h, .min_args = a, .cmd_length = LEN(n), \
      .description = d }

static command_t commands[] =
{
    CMD(CMD_PING,       "ping",       handle_ping,       0,
            "ping the nyx server"),
    CMD(CMD_VERSION,    "version",    handle_version,    0,
            "request the nyx server version"),
    CMD(CMD_WATCHES,    "watches",    handle_watches,    0,
            "get the list of watches"),
    CMD(CMD_START,      "start",      handle_start,      1,
            "start the specified watch"),
    CMD(CMD_STOP,       "stop",       handle_stop,       1,
            "stop the specified watch"),
    CMD(CMD_RESTART,    "restart",    handle_restart,    1,
            "restart the specified watch"),
    CMD(CMD_STATUS,     "status",     handle_status,     1,
            "request the watch's status"),
    CMD(CMD_HISTORY,    "history",    handle_history,    1,
            "get the latest events of the specified watch"),
    CMD(CMD_CONFIG,     "config",     handle_config,     1,
            "get the configuration of the specified watch"),
    CMD(CMD_RELOAD,     "reload",     handle_reload,     0,
            "reload the nyx configuration"),
    CMD(CMD_TERMINATE,  "terminate",  handle_terminate,  0,
            "terminate the nyx server"),
    CMD(CMD_QUIT,       "quit",       handle_quit,       0,
            "stop the nyx server and all watched processes")
};

#undef CMD

static unsigned int
command_max_length(void)
{
    int idx = 0;
    unsigned int len = 0;

    while (idx < CMD_SIZE)
    {
        unsigned int cmd_len = commands[idx++].cmd_length;
        len = MAX(len, cmd_len);
    }

    return len;
}

static void
print_command(FILE *out, unsigned int pad, command_t *cmd)
{
    unsigned int i = 0;

    fprintf(out, "  %s", cmd->name);

    for (i = cmd->cmd_length; i < pad; i++)
        fputc(' ', out);

    fprintf(out, "%s\n", cmd->description);
}

void
print_commands(FILE *out)
{
    int idx = 0;
    unsigned int pad_to = command_max_length() + 2;

    while (idx < CMD_SIZE)
    {
        print_command(out, pad_to, &commands[idx++]);
    }
}

command_t *
parse_command(const char **input)
{
    size_t i = 0;
    size_t size = LEN(commands);
    unsigned int args = 0;
    command_t *command = commands;

    /* no input commands given at all */
    if (input == NULL)
        return NULL;

    while (i < size)
    {
        if (!strncmp(command->name, *input, command->cmd_length))
        {
            /* check if necessary arguments are given */
            args = count_args(input) - 1;

            if (args < command->min_args)
            {
                log_error("Command '%s' requires a minimum of %d arguments",
                        command->name,
                        command->min_args);
                return NULL;
            }

            return command;
        }

        command++; i++;
    }

    return NULL;
}

/* vim: set et sw=4 sts=4 tw=80: */