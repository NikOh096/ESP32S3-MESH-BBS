#pragma once
#include "bbs.h"
#include "rules.h"
#define THREAD_SLOTS 9
#define THREAD_COMMENTS 32
#define THREAD_TITLE 48
#define THREAD_NAME 32
#define THREAD_TEXT 160
#define THREAD_VERSION 4
#define THREAD_LAST_ID 25999999u
#define THREAD_PACKETS 80
#define THREAD_QUEUE 32
typedef struct {
    uint32_t node, packet_id, at;
    char name[THREAD_NAME + 1], text[THREAD_TEXT + 1];
} bulletin_comment_t;
typedef struct {
    uint32_t version, generation, created, expires;
    uint16_t count, unread;
    uint8_t active;
    char title[THREAD_TITLE + 1], body[THREAD_TEXT + 1];
    bulletin_comment_t comments[THREAD_COMMENTS];
    uint32_t author_node, author_packet;
    char author_name[THREAD_NAME + 1];
    uint64_t lifetime_threads, lifetime_replies;
} bulletin_t;
typedef struct {
    uint32_t node, packet_id, submitted;
    char name[THREAD_NAME + 1], title[THREAD_TITLE + 1], body[THREAD_TEXT + 1];
} bulletin_pending_t;
typedef struct {
    uint32_t version, count;
    bulletin_pending_t entries[THREAD_QUEUE];
} bulletin_queue_t;
typedef bool (*bulletin_save_fn)(unsigned, const bulletin_t*, void*);
typedef bool (*bulletin_queue_save_fn)(const bulletin_queue_t*, void*);
typedef struct {
    bulletin_t slots[THREAD_SLOTS];
    bulletin_save_fn save;
    void* ctx;
    bulletin_queue_t queue;
    bulletin_queue_save_fn save_queue;
    rules_t rules;
} bulletins_t;
typedef struct {
    unsigned count;
    uint32_t thread_id, expires;
    char text[THREAD_PACKETS][BBS_REPLY_MAX + 1];
} bulletin_reply_t;
void bulletins_init(bulletins_t*, bulletin_save_fn, void*);
bool bulletin_restore(bulletins_t*, unsigned, const bulletin_t*);
bool bulletins_restore_queue(bulletins_t*, const bulletin_queue_t*);
void bulletins_queue_storage(bulletins_t*, bulletin_queue_save_fn);
bool bulletins_tick(bulletins_t*, uint32_t now);
bool bulletins_ban_pending(bulletins_t*, uint32_t node);
bool bulletins_remove_pending(bulletins_t*, uint32_t node, uint32_t packet);
int bulletin_index(const bulletins_t*, unsigned ordinal, uint32_t now);
int bulletin_by_id(const bulletins_t*, uint32_t generation);
unsigned bulletins_active(const bulletins_t*, uint32_t now);
int bulletin_free(const bulletins_t*);
bool bulletin_visible(const bulletin_t*, uint32_t now);
bool bulletin_create(bulletins_t*, unsigned, const char*, const char*, uint32_t now, uint32_t expires);
bool bulletin_delete(bulletins_t*, unsigned, uint32_t generation);
bool bulletin_seen(bulletins_t*, unsigned, uint32_t generation, unsigned count_viewed);
bool bulletin_expiry(bulletins_t*, unsigned, uint32_t generation, uint32_t expires, uint32_t now);
bool bulletins_unread(const bulletins_t*, uint32_t now);
bool bulletin_text_valid(const char*, size_t limit);
void bulletin_id(uint32_t, char out[8]);
uint64_t bulletins_total_threads(const bulletins_t*);
uint64_t bulletins_total_replies(const bulletins_t*);
/* Radio visitors can create 24-hour bulletins and comment. Moderation is app-only. */
void bulletins_command(bulletins_t*, const bbs_request_t*, const char* name,
    const char* command, uint32_t now, bulletin_reply_t*);
