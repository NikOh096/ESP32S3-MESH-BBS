#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BBS_CAPACITY 64
#define BBS_BODY_MAX 160
#ifdef MESHBBS_MESHCORE
#define BBS_REPLY_MAX 160
#else
#define BBS_REPLY_MAX 200
#endif
#define BBS_STORE_VERSION 1

typedef struct {
    uint32_t version, id, author, request_id, received_at;
    uint8_t author_key[32];
    uint8_t authenticated, deleted;
    char body[BBS_BODY_MAX + 1];
} bbs_post_t;

typedef struct {
    uint32_t sender, packet_id, received_at;
    bool authenticated;
    uint8_t key[32];
} bbs_request_t;

typedef bool (*bbs_save_fn)(unsigned slot, const bbs_post_t *post, void *ctx);
typedef struct {
    bbs_post_t posts[BBS_CAPACITY];
    uint32_t next_id;
    bbs_save_fn save;
    void *save_ctx;
} bbs_t;

void bbs_init(bbs_t *bbs, bbs_save_fn save, void *ctx);
bool bbs_restore(bbs_t *bbs, unsigned slot, const bbs_post_t *post);
bool bbs_handle(bbs_t *bbs, const bbs_request_t *req, const char *text, char reply[BBS_REPLY_MAX + 1]);
bool bbs_delete_local(bbs_t *bbs, uint32_t id);
unsigned bbs_count(const bbs_t *bbs);
/* Recognition is separate so ordinary chat cannot consume command rate limits. */
bool bbs_is_command(const char *text);
