#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MC_FRAME_MAX 176
#define MC_TEXT_MAX 160
#define MC_TARGET_VERSION "companion-v1.17.1"
enum { MC_OK = 0,
    MC_ERROR = 1,
    MC_CONTACTS_START = 2,
    MC_CONTACT = 3,
    MC_CONTACTS_END = 4,
    MC_SELF = 5,
    MC_SENT = 6,
    MC_MESSAGE = 7,
    MC_CHANNEL = 8,
    MC_TIME = 9,
    MC_EMPTY = 10,
    MC_DEVICE = 13,
    MC_MESSAGE_V3 = 16,
    MC_CHANNEL_V3 = 17,
    MC_ADVERT = 0x80,
    MC_PATH = 0x81,
    MC_ACK = 0x82,
    MC_WAITING = 0x83,
    MC_NEW_CONTACT = 0x8a,
    MC_CONTACT_DELETED = 0x8f,
    MC_CONTACTS_FULL = 0x90 };
typedef struct {
    unsigned type, error;
    uint32_t timestamp, ack, timeout_ms, count;
    uint8_t key[32], prefix[6];
    uint8_t contact_type, text_type;
    bool fixed_pin;
    char hardware[41];
    char name[33], version[21], text[MC_TEXT_MAX + 1];
} mc_packet_t;
bool mc_parse(const uint8_t*, size_t, mc_packet_t*);
size_t mc_start(uint8_t*, size_t);
size_t mc_query(uint8_t*, size_t);
size_t mc_get_contacts(uint8_t*, size_t);
size_t mc_get_time(uint8_t*, size_t);
size_t mc_next(uint8_t*, size_t);
size_t mc_send(const uint8_t key[32], const char*, uint32_t timestamp, uint8_t*, size_t);
uint32_t mc_request_id(const uint8_t key[32], uint32_t timestamp, const char*);
bool mc_key_valid(const uint8_t key[32]);
