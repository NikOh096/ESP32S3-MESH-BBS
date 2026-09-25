#include "mesh_protocol.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include <string.h>

static size_t encode(const meshtastic_ToRadio *message, uint8_t *out, size_t cap)
{
    pb_ostream_t stream = pb_ostream_from_buffer(out, cap);
    return pb_encode(&stream, meshtastic_ToRadio_fields, message) ? stream.bytes_written : 0;
}

bool mesh_decode(const uint8_t *data, size_t size, meshtastic_FromRadio *out)
{
    if (!size || size > MESH_WIRE_MAX) return false;
    memset(out, 0, sizeof(*out));
    pb_istream_t stream = pb_istream_from_buffer(data, size);
    return pb_decode(&stream, meshtastic_FromRadio_fields, out);
}

/* Reject malformed UTF-8, embedded NUL and control bytes before the command parser. */
static bool valid_text(const uint8_t *s, size_t n)
{
    for (size_t i = 0; i < n;) {
        unsigned c = s[i++], extra; uint32_t value, minimum;
        if (c < 0x80) {
            if (c < 32 && c != '\n' && c != '\r' && c != '\t') return false;
            if (c == 127) return false;
            continue;
        }
        if (c >= 0xc2 && c <= 0xdf) { extra = 1; value = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { extra = 2; value = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { extra = 3; value = c & 7; minimum = 0x10000; }
        else return false;
        if (i + extra > n) return false;
        while (extra--) {
            c = s[i++]; if ((c & 0xc0) != 0x80) return false;
            value = (value << 6) | (c & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}

bool mesh_extract(const meshtastic_MeshPacket *p, uint32_t own, mesh_request_t *out)
{
    if (!own || !p->from || p->from == own || p->from == UINT32_MAX || p->to != own ||
        !p->id || p->which_payload_variant != meshtastic_MeshPacket_decoded_tag ||
        p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP ||
        p->decoded.emoji || p->decoded.request_id) return false;
    size_t n = p->decoded.payload.size;
    if (!n || n > MESH_TEXT_MAX || !valid_text(p->decoded.payload.bytes, n)) return false;
    memset(out, 0, sizeof(*out));
    out->request.sender = p->from; out->request.packet_id = p->id;
    out->request.received_at = p->rx_time;
    out->request.authenticated = p->pki_encrypted && p->public_key.size == 32;
    if (out->request.authenticated) memcpy(out->request.key, p->public_key.bytes, 32);
    out->channel = p->channel;
    memcpy(out->text, p->decoded.payload.bytes, n);
    for (size_t i = 0; i < n; ++i) if (out->text[i] == '\r' || out->text[i] == '\n' || out->text[i] == '\t') out->text[i] = ' ';
    while (n && out->text[n - 1] == ' ') out->text[--n] = 0;
    return n > 0;
}

size_t mesh_config(uint32_t nonce, uint8_t *out, size_t cap)
{
    meshtastic_ToRadio msg = meshtastic_ToRadio_init_zero;
    msg.which_payload_variant = meshtastic_ToRadio_want_config_id_tag;
    msg.want_config_id = nonce;
    return encode(&msg, out, cap);
}

bool mesh_routing(const meshtastic_MeshPacket *p, uint32_t own,
                  uint32_t *request_id, meshtastic_Routing_Error *error)
{
    if (!own || p->to != own || p->which_payload_variant != meshtastic_MeshPacket_decoded_tag ||
        p->decoded.portnum != meshtastic_PortNum_ROUTING_APP || !p->decoded.request_id ||
        p->decoded.payload.size > sizeof(p->decoded.payload.bytes)) return false;
    meshtastic_Routing routing = meshtastic_Routing_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(p->decoded.payload.bytes, p->decoded.payload.size);
    if (!pb_decode(&stream, meshtastic_Routing_fields, &routing) ||
        routing.which_variant != meshtastic_Routing_error_reason_tag) return false;
    *request_id = p->decoded.request_id; *error = routing.error_reason;
    return true;
}

const char *mesh_routing_error(meshtastic_Routing_Error error)
{
    switch (error) {
    case meshtastic_Routing_Error_NONE: return "ACK";
    case meshtastic_Routing_Error_NO_ROUTE: return "NO_ROUTE";
    case meshtastic_Routing_Error_GOT_NAK: return "NAK";
    case meshtastic_Routing_Error_TIMEOUT: return "TIMEOUT";
    case meshtastic_Routing_Error_NO_INTERFACE: return "NO_INTERFACE";
    case meshtastic_Routing_Error_MAX_RETRANSMIT: return "MAX_RETRANSMIT";
    case meshtastic_Routing_Error_NO_CHANNEL: return "NO_CHANNEL";
    case meshtastic_Routing_Error_TOO_LARGE: return "TOO_LARGE";
    case meshtastic_Routing_Error_NO_RESPONSE: return "NO_RESPONSE";
    case meshtastic_Routing_Error_DUTY_CYCLE_LIMIT: return "DUTY_CYCLE_LIMIT";
    case meshtastic_Routing_Error_PKI_FAILED: return "PKI_FAILED";
    case meshtastic_Routing_Error_PKI_UNKNOWN_PUBKEY: return "PKI_UNKNOWN_KEY";
    case meshtastic_Routing_Error_PKI_SEND_FAIL_PUBLIC_KEY: return "PKI_SEND_NO_KEY";
    case meshtastic_Routing_Error_RATE_LIMIT_EXCEEDED: return "RATE_LIMIT";
    default: return "ROUTING_ERROR";
    }
}

size_t mesh_heartbeat(uint8_t *out, size_t cap)
{
    meshtastic_ToRadio msg = meshtastic_ToRadio_init_zero;
    msg.which_payload_variant = meshtastic_ToRadio_heartbeat_tag;
    /* Nonce 1 requests a nodeinfo broadcast on the target release. Keep this zero. */
    msg.heartbeat.nonce = 0;
    return encode(&msg, out, cap);
}

size_t mesh_reply(const mesh_request_t *r, const char *text, uint32_t id,
                  uint8_t hops, uint8_t *out, size_t cap)
{
    size_t n = strlen(text);
    if (!n || n > BBS_REPLY_MAX || !id || !r->request.sender || r->request.sender == UINT32_MAX) return 0;
    meshtastic_ToRadio msg = meshtastic_ToRadio_init_zero;
    msg.which_payload_variant = meshtastic_ToRadio_packet_tag;
    meshtastic_MeshPacket *p = &msg.packet;
    p->to = r->request.sender; p->id = id; p->channel = r->channel;
    p->hop_limit = hops > 7 ? 7 : hops; p->want_ack = true;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->decoded.reply_id = r->request.packet_id;
    p->decoded.payload.size = (pb_size_t)n;
    memcpy(p->decoded.payload.bytes, text, n);
    if (r->request.authenticated) {
        p->pki_encrypted = true;
        p->public_key.size = 32; memcpy(p->public_key.bytes, r->request.key, 32);
    }
    return encode(&msg, out, cap);
}
