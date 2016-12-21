/*  =========================================================================
    fty_metric_composite_configurator - Metrics calculator configurator

    Copyright (C) 2014 - 2015 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    fty_metric_composite_configurator - Metrics calculator configurator
@discuss
@end
*/

#include <getopt.h>

#include "fty_metric_composite_classes.h"

#define str(x) #x

static const char *AGENT_NAME = "fty-metric-composite-configurator";
static const char *ENDPOINT = "ipc://@/malamute";
static const char *DIRECTORY = "/var/lib/bios/composite-metrics";
static const char *STATE_FILE = "/var/lib/bios/composite-metrics/configurator_state_file";

#define DEFAULT_LOG_LEVEL LOG_WARNING

void usage () {
    puts ("fty-metric-composite-configurator [options] ...\n"
          "  --log-level / -l       bios log level\n"
          "                         overrides setting in env. variable BIOS_LOG_LEVEL\n"
          "  --output-dir / -s      directory, where configuration files would be created (directory MUST exist)\n"
          "  --state-file / -s      TODO\n"
          "  --help / -h            this information\n"
          );
}

static int
s_timer_event (zloop_t *loop, int timer_id, void *output)
{
    char *env = getenv ("BIOS_DO_SENSOR_PROPAGATION");
    if (env == NULL)
        zstr_sendx (output, "IS_PROPAGATION_NEEDED", "true", NULL);
    else
        zstr_sendx (output, "IS_PROPAGATION_NEEDED", env, NULL);
    return 0;
}

int get_log_level (const char *level) {
    if (streq (level, str(LOG_DEBUG))) {
        return LOG_DEBUG;
    }
    else
    if (streq (level, str(LOG_INFO))) {
        return LOG_INFO;
    }
    else
    if (streq (level, str(LOG_WARNING))) {
        return LOG_WARNING;
    }
    else
    if (streq (level, str(LOG_ERR))) {
        return LOG_ERR;
    }
    else
    if (streq (level, str(LOG_CRIT))) {
        return LOG_CRIT;
    }
    return -1;
}

int main (int argc, char *argv [])
{
    int help = 0;
    int log_level = -1;
    char *state_file = NULL;
    char *output_dir = NULL;

    while (true) {
        static struct option long_options[] =
        {
            {"help",            no_argument,        0,  1},
            {"log-level",       required_argument,  0,  'l'},
            {"state-file",      required_argument,  0,  's'},
            {"output-dir",      required_argument,  0,  'o'},
            {0,                 0,                  0,  0}
        };

        int option_index = 0;
        int c = getopt_long (argc, argv, "hl:s:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 'l':
            {
                log_level = get_log_level (optarg);
                break;
            }
            case 's':
            {
                state_file = optarg;
                break;
            }
            case 'o':
            {
                output_dir = optarg;
                break;
            }
            case 'h':
            default:
            {
                help = 1;
                break;
            }
        }
    }
    if (help) {
        usage ();
        return EXIT_FAILURE;
    }

    // log_level cascade (priority ascending)
    //  1. default value
    //  2. env. variable
    //  3. command line argument
    //  4. actor message - NOT IMPLEMENTED YET
    if (log_level == -1) {
        char *env_log_level = getenv ("BIOS_LOG_LEVEL");
        if (env_log_level) {
            log_level = get_log_level (env_log_level);
            if (log_level == -1)
                log_level = DEFAULT_LOG_LEVEL;
        }
        else {
            log_level = DEFAULT_LOG_LEVEL;
        }
    }
    log_set_level (log_level);

    // Set default state file on empty
    if (!state_file) {
        state_file = strdup (STATE_FILE);
    }
    if (!output_dir) {
        output_dir = strdup (DIRECTORY);
    }
    log_debug ("state file == '%s'", state_file ? state_file : "(null)");
    log_debug ("output_dir == '%s'", output_dir ? output_dir : "(null)");

    zactor_t *server = zactor_new (fty_metric_composite_configurator_server, (void *) AGENT_NAME);
    if (!server) {
        log_critical ("zactor_new (task = 'fty_metric_composite_configurator_server', args = 'NULL') failed");
        return EXIT_FAILURE;
    }
    zstr_sendx (server,  "STATE_FILE", state_file, NULL);
    zstr_sendx (server,  "CFG_DIRECTORY", output_dir, NULL);
    zstr_sendx (server,  "LOAD", NULL);
    zstr_sendx (server,  "CONNECT", ENDPOINT, NULL);
    zstr_sendx (server,  "PRODUCER", "_METRICS_UNAVAILABLE", NULL);
    zstr_sendx (server,  "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);

    zloop_t *check_configuration_trigger = zloop_new();
    // one in a minute
    zloop_timer (check_configuration_trigger, 60*1000, 0, s_timer_event, server);
    zloop_start (check_configuration_trigger);
    
    zloop_destroy (&check_configuration_trigger);

    zstr_free (&state_file);
    zstr_free (&output_dir);
    zactor_destroy (&server);
    return EXIT_SUCCESS;
}
