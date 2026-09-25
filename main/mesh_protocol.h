#pragma once
#include "bbs.h"
#include "meshtastic/mesh.pb.h"

#define MESH_WIRE_MAX 1024
#define MESH_TEXT_MAX 237
#define MESH_TARGET_VERSION "2.7.26.54e0d8d"

typedef struct {
    bbs_request_t request;
    uint8_t channel;
    char text[MESH_TEXT_MAX + 1];
} mesh_request_t;

bool mesh_decode(const uint8_t *data, size_t size, meshtastic_FromRadio *out);
bool mesh_extract(const meshtastic_MeshPacket *packet, uint32_t own_node, mesh_request_t *out);
bool mesh_routing(const meshtastic_MeshPacket *packet, uint32_t own_node,
                  uint32_t *request_id, meshtastic_Routing_Error *error);
const char *mesh_routing_error(meshtastic_Routing_Error error);
size_t mesh_config(uint32_t nonce, uint8_t *out, size_t capacity);
size_t mesh_heartbeat(uint8_t *out, size_t capacity);
size_t mesh_reply(const mesh_request_t *request, const char *text, uint32_t packet_id,
                  uint8_t hops, uint8_t *out, size_t capacity);
