/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2011 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/* Include config.h to get the definition of hrtime_t for platforms
** without it...
*/
#include "config.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <event.h>
#include <libcouchbase/couchbase.h>

#include "server.h"

int error = 0;

static void error_callback(libcouchbase_t instance,
                           libcouchbase_error_t err,
                           const char *errinfo)
{
    (void)instance;
    fprintf(stderr, "Error %s", libcouchbase_strerror(instance, err));
    if (errinfo) {
        fprintf(stderr, ": %s", errinfo);
    }
    fprintf(stderr, "\n");
    abort();
    exit(EXIT_FAILURE);
}

static void timings_callback(libcouchbase_t instance,
                             const void *cookie,
                             libcouchbase_timeunit_t timeunit,
                             libcouchbase_uint32_t min,
                             libcouchbase_uint32_t max,
                             libcouchbase_uint32_t total,
                             libcouchbase_uint32_t maxtotal)
{
    FILE *fp = (void*)cookie;
    int num, ii;

    fprintf(fp, "[%3u - %3u]", min, max);
    switch (timeunit) {
    case LIBCOUCHBASE_TIMEUNIT_NSEC:
        fprintf(fp, "ns");
        break;
    case LIBCOUCHBASE_TIMEUNIT_USEC:
        fprintf(fp, "us");
        break;
    case LIBCOUCHBASE_TIMEUNIT_MSEC:
        fprintf(fp, "ms");
        break;
    case LIBCOUCHBASE_TIMEUNIT_SEC:
        fprintf(fp, "s");
        break;
    default:
        ;
    }

    num = (int)((float)20.0 * (float)total / (float)maxtotal);

    fprintf(fp, " |");
    for (ii = 0; ii < num; ++ii) {
        fprintf(fp, "#");
    }

    fprintf(fp, " - %u\n", total);
    (void)cookie;
    (void)maxtotal;
    (void)instance;
}

int main(int argc, char **argv)
{
    FILE *fp;
    struct event_base *evbase;
    const void *mock;
    const char *http;
    struct libcouchbase_io_opt_st *io;
    libcouchbase_t instance;
    int ii;

    (void)argc; (void)argv;

    fp = stdout;
    if (getenv("LIBCOUCHBASE_VERBOSE_TESTS") == NULL) {
        fp = fopen("/dev/null", "w");
    }

    evbase = event_base_new();
    if (evbase == NULL) {
        fprintf(stderr, "Failed to create event base\n");
        return 1;
    }

    mock = start_mock_server(NULL);
    if (mock == NULL) {
        fprintf(stderr, "Failed to start mock server\n");
        return 1;
    }

    http = get_mock_http_server(mock);

    io = libcouchbase_create_io_ops(LIBCOUCHBASE_IO_OPS_LIBEVENT, evbase, NULL);
    if (io == NULL) {
        fprintf(stderr, "Failed to create IO instance\n");
        return 1;
    }
    instance = libcouchbase_create(http, "Administrator",
                                   "password", NULL, io);
    if (instance == NULL) {
        fprintf(stderr, "Failed to create libcouchbase instance\n");
        event_base_free(evbase);
        return 1;
    }

    (void)libcouchbase_set_error_callback(instance, error_callback);
    if (libcouchbase_connect(instance) != LIBCOUCHBASE_SUCCESS) {
        fprintf(stderr, "Failed to connect libcouchbase instance to server\n");
        event_base_free(evbase);
        return 1;
    }

    /* Wait for the connect to compelete */
    libcouchbase_wait(instance);

    libcouchbase_enable_timings(instance);
    libcouchbase_store(instance, NULL, LIBCOUCHBASE_SET, "counter", 7,
                       "0", 1, 0, 0, 0);
    libcouchbase_wait(instance);
    for (ii = 0; ii < 100; ++ii) {
        libcouchbase_arithmetic(instance, NULL, "counter", 7, 1, 0, 1, 0);
        libcouchbase_wait(instance);
    }
    fprintf(fp, "              +---------+---------+\n");
    libcouchbase_get_timings(instance, fp, timings_callback);
    fprintf(fp, "              +--------------------\n");
    libcouchbase_disable_timings(instance);

    shutdown_mock_server(mock);

    return error;
}
