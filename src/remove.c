/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2010 Membase, Inc.
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

#include "internal.h"

/**
 * Send a delete command to the correct server
 *
 * @author Trond Norbye
 * @todo improve the error handling
 */
LIBMEMBASE_API
libmembase_error_t libmembase_remove(libmembase_t instance,
                                     const void *key, size_t nkey,
                                     uint64_t cas)
{
    // we need a vbucket config before we can start removing the item..
    libmembase_ensure_vbucket_config(instance);
    assert(instance->vbucket_config);

    uint16_t vb;
    vb = (uint16_t)vbucket_get_vbucket_by_key(instance->vbucket_config,
                                              key, nkey);
    libmembase_server_t *server;
    server = instance->servers + instance->vb_server_map[vb];
    protocol_binary_request_delete req = {
        .message = {
            .header.request = {
                .magic = PROTOCOL_BINARY_REQ,
                .opcode = PROTOCOL_BINARY_CMD_DELETE,
                .keylen = ntohs((uint16_t)nkey),
                .extlen = 0,
                .datatype = PROTOCOL_BINARY_RAW_BYTES,
                .vbucket = ntohs(vb),
                .bodylen = ntohl((uint32_t)nkey),
                .opaque = ++instance->seqno,
                .cas = cas
            },
        }
    };

    libmembase_server_start_packet(server, req.bytes, sizeof(req.bytes));
    libmembase_server_write_packet(server, key, nkey);
    libmembase_server_end_packet(server);
    libmembase_server_send_packets(server);

    return LIBMEMBASE_SUCCESS;
}