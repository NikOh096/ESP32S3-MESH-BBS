#include "bbs.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bbs_init(bbs_t *b, bbs_save_fn save, void *ctx)
{
    memset(b, 0, sizeof(*b));
    b->next_id = 1; b->save = save; b->save_ctx = ctx;
}

bool bbs_restore(bbs_t *b, unsigned slot, const bbs_post_t *p)
{
    if (slot >= BBS_CAPACITY || p->version != BBS_STORE_VERSION || !p->id ||
        p->id == UINT32_MAX || !memchr(p->body, 0, sizeof(p->body))) return false;
    b->posts[slot] = *p;
    if (p->id >= b->next_id) b->next_id = p->id + 1;
    return true;
}

unsigned bbs_count(const bbs_t *b)
{
    unsigned n = 0;
    for (unsigned i = 0; i < BBS_CAPACITY; ++i) if (b->posts[i].id && !b->posts[i].deleted) ++n;
    return n;
}

static bool persist(bbs_t *b, unsigned slot, const bbs_post_t *p)
{
    if (!b->save || !b->save(slot, p, b->save_ctx)) return false;
    b->posts[slot] = *p;
    return true;
}

static int find_post(bbs_t *b, uint32_t id)
{
    for (unsigned i = 0; i < BBS_CAPACITY; ++i) if (b->posts[i].id == id && !b->posts[i].deleted) return (int)i;
    return -1;
}

bool bbs_delete_local(bbs_t *b, uint32_t id)
{
    int slot = find_post(b, id);
    if (slot < 0) return false;
    bbs_post_t p = b->posts[slot]; p.deleted = 1;
    memset(p.body, 0, sizeof(p.body));
    return persist(b, (unsigned)slot, &p);
}

static bool number(const char *s, uint32_t *n)
{
    if (!*s) return false;
    uint64_t v = 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9') return false;
        v = v * 10 + (unsigned)(*s - '0');
        if (v >= UINT32_MAX) return false;
    }
    *n = (uint32_t)v;
    return v > 0;
}

static const char *parse_command(const char *text, char command[12], bool *prefixed)
{
    while (*text == ' ') ++text;
    *prefixed = false;
    if (strlen(text) >= 4 && text[0] == '!' &&
        toupper((unsigned char)text[1]) == 'B' && toupper((unsigned char)text[2]) == 'B' &&
        toupper((unsigned char)text[3]) == 'S' && (!text[4] || text[4] == ' ')) {
        *prefixed = true; text += 4; while (*text == ' ') ++text;
    }
    size_t n = 0;
    while (*text && *text != ' ' && n < 11) command[n++] = (char)toupper((unsigned char)*text++);
    command[n] = 0;
    while (*text == ' ') ++text;
    if (!strcmp(command, "H") || !strcmp(command, "?") || !strcmp(command, "MENU") ||
        !strcmp(command, "HELLO") || !strcmp(command, "START") || (!n && *prefixed)) strcpy(command, "HELP");
    else if (!strcmp(command, "L")) strcpy(command, "LIST");
    else if (!strcmp(command, "R")) strcpy(command, "READ");
    else if (!strcmp(command, "P")) strcpy(command, "POST");
    return text;
}

bool bbs_is_command(const char *text)
{
    char command[12]; bool prefixed;
    parse_command(text, command, &prefixed);
    return prefixed || !strcmp(command, "HELP") || !strcmp(command, "INFO") ||
        !strcmp(command, "PING") || !strcmp(command, "LIST") || !strcmp(command, "READ") ||
        !strcmp(command, "POST") || !strcmp(command, "DEL");
}

static void preview(const char *text, char out[25])
{
    size_t n = strlen(text);
    if (n <= 24) { memcpy(out, text, n + 1); return; }
    n = 21;
    while (n && ((unsigned char)text[n] & 0xc0) == 0x80) --n;
    memcpy(out, text, n); strcpy(out + n, "...");
}

bool bbs_handle(bbs_t *b, const bbs_request_t *r, const char *text, char out[BBS_REPLY_MAX + 1])
{
    out[0] = 0;
    char command[12]; bool prefixed;
    text = parse_command(text, command, &prefixed);
    if (!strcmp(command, "HELP")) {
        char topic[12]; bool ignored;
        parse_command(text, topic, &ignored);
        if (!strcmp(topic, "POST")) strcpy(out, "BBS: POST your message saves a PUBLIC bulletin, max 160 UTF-8 bytes. Example: POST Trail cleanup Saturday 9am. Wait for a saved post number; READ that number checks it.");
        else if (!strcmp(topic, "READ") || !strcmp(topic, "LIST")) strcpy(out, "BBS: LIST shows newest posts with previews. LIST 2 gets the next page. READ 12 retrieves post #12. Messages appear as DMs here. Posts stay until deleted or replaced by newer ones.");
        else if (!strcmp(topic, "DEL")) strcpy(out, "BBS: DEL 12 deletes your post #12 using the same Meshtastic node and original PKI key. Other posts require the operator. Reading does not delete a post.");
        else strcpy(out, "BBS PUBLIC noticeboard: LIST [page], READ id, POST text, DEL id, INFO, PING. HELP POST or HELP READ for examples. 160-byte posts; newest 64 kept. Wait 10s between commands.");
    } else if (!strcmp(command, "INFO")) {
        snprintf(out, BBS_REPLY_MAX + 1, "BBS v0.3: %u/%u PUBLIC posts saved locally. LIST to browse; POST text to publish. Survives restart. Oldest posts rotate out when full. No private mailbox or automatic chat archive.", bbs_count(b), BBS_CAPACITY);
    } else if (!strcmp(command, "PING")) {
        strcpy(out, "BBS: PONG. Your command reached the BBS and this is its reply. HELP for commands; LIST for saved posts.");
    } else if (!strcmp(command, "POST")) {
        size_t len = strlen(text);
        if (!len || len > BBS_BODY_MAX) {
            snprintf(out, BBS_REPLY_MAX + 1, "BBS: POST text (1-%u UTF-8 bytes). Posts are public.", BBS_BODY_MAX); return true;
        }
        for (unsigned i = 0; i < BBS_CAPACITY; ++i) {
            const bbs_post_t *p = &b->posts[i];
            if (r->packet_id && p->id && p->author == r->sender && p->request_id == r->packet_id) {
                snprintf(out, BBS_REPLY_MAX + 1, "BBS: Request already saved as #%" PRIu32 ".", p->id); return true;
            }
        }
        if (b->next_id == UINT32_MAX) { strcpy(out, "BBS: ID space exhausted; contact operator."); return true; }
        unsigned slot = 0;
        for (unsigned i = 0; i < BBS_CAPACITY; ++i) {
            if (!b->posts[i].id) { slot = i; break; }
            if (b->posts[i].id < b->posts[slot].id) slot = i;
        }
        bbs_post_t p = {0};
        p.version = BBS_STORE_VERSION; p.id = b->next_id; p.author = r->sender;
        p.request_id = r->packet_id; p.received_at = r->received_at; p.authenticated = r->authenticated;
        memcpy(p.author_key, r->key, 32); memcpy(p.body, text, len + 1);
        if (persist(b, slot, &p)) {
            ++b->next_id; snprintf(out, BBS_REPLY_MAX + 1, "BBS: Public post #%" PRIu32 " saved. READ %" PRIu32 " to retrieve it; LIST to browse.", p.id, p.id);
        } else strcpy(out, "BBS: Storage error. Post was NOT saved.");
    } else if (!strcmp(command, "READ") || !strcmp(command, "DEL")) {
        uint32_t id;
        if (!number(text, &id)) { snprintf(out, BBS_REPLY_MAX + 1, "BBS: Use %s id.", command); return true; }
        int slot = find_post(b, id);
        if (slot < 0) { snprintf(out, BBS_REPLY_MAX + 1, "BBS: Post #%" PRIu32 " not found.", id); return true; }
        const bbs_post_t *p = &b->posts[slot];
        if (!strcmp(command, "READ")) {
            snprintf(out, BBS_REPLY_MAX + 1, "BBS #%" PRIu32 " !%08" PRIx32 ": %s", p->id, p->author, p->body);
        } else if (r->sender != p->author || !r->authenticated || !p->authenticated || memcmp(r->key, p->author_key, 32)) {
            strcpy(out, "BBS: DEL requires the post author's original PKI identity. Otherwise ask the operator.");
        } else if (bbs_delete_local(b, id)) {
            snprintf(out, BBS_REPLY_MAX + 1, "BBS: Deleted #%" PRIu32 ".", id);
        } else strcpy(out, "BBS: Storage error; deletion NOT saved.");
    } else if (!strcmp(command, "LIST")) {
        uint32_t page = 1;
        if (*text && !number(text, &page)) { strcpy(out, "BBS: Use LIST or LIST page."); return true; }
        unsigned order[BBS_CAPACITY], count = 0;
        for (unsigned i = 0; i < BBS_CAPACITY; ++i) if (b->posts[i].id && !b->posts[i].deleted) order[count++] = i;
        for (unsigned i = 0; i < count; ++i) for (unsigned j = i + 1; j < count; ++j) {
            if (b->posts[order[j]].id > b->posts[order[i]].id) { unsigned tmp = order[i]; order[i] = order[j]; order[j] = tmp; }
        }
        unsigned pages = (count + 2) / 3;
        if (!count) { strcpy(out, "BBS: No posts yet. POST text to add a public bulletin."); return true; }
        if (page > pages) { snprintf(out, BBS_REPLY_MAX + 1, "BBS: Choose LIST 1 through LIST %u.", pages); return true; }
        size_t used = (size_t)snprintf(out, BBS_REPLY_MAX + 1, "BBS page %" PRIu32 "/%u", page, pages);
        for (unsigned i = (page - 1) * 3; i < count && i < page * 3; ++i) {
            const bbs_post_t *p = &b->posts[order[i]];
            char excerpt[25]; preview(p->body, excerpt);
            used += (size_t)snprintf(out + used, BBS_REPLY_MAX + 1 - used, "\n#%" PRIu32 " !%08" PRIx32 ": %s", p->id, p->author, excerpt);
        }
        if (page < pages) snprintf(out + used, BBS_REPLY_MAX + 1 - used, "\nREAD id | LIST %" PRIu32, page + 1);
        else snprintf(out + used, BBS_REPLY_MAX + 1 - used, "\nREAD id for text.");
    } else if (prefixed) strcpy(out, "BBS: Unknown command. Send HELP.");
    else return false;
    return true;
}
