#include "bulletins.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool bulletin_text_valid(const char* s, size_t limit)
{
    size_t n = strlen(s);
    if (!n || n > limit)
        return false;
    for (size_t i = 0; i < n;) {
        uint32_t cp = (unsigned char)s[i++], min = 0;
        unsigned more = 0;
        if (cp < 0x20 || cp == 0x7f)
            return false;
        if (cp < 128)
            continue;
        if (cp >= 0xc2 && cp <= 0xdf) {
            more = 1;
            cp &= 31;
            min = 128;
        } else if (cp >= 0xe0 && cp <= 0xef) {
            more = 2;
            cp &= 15;
            min = 2048;
        } else if (cp >= 0xf0 && cp <= 0xf4) {
            more = 3;
            cp &= 7;
            min = 65536;
        } else
            return false;
        while (more--) {
            if (i >= n || ((unsigned char)s[i] & 0xc0) != 0x80)
                return false;
            cp = (cp << 6) | ((unsigned char)s[i++] & 63);
        }
        if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return false;
    }
    return true;
}
void bulletins_init(bulletins_t* b, bulletin_save_fn save, void* ctx)
{
    memset(b, 0, sizeof(*b));
    b->save = save;
    b->ctx = ctx;
    b->queue.version = 1;
    rules_init(&b->rules);
}
bool bulletin_restore(bulletins_t* b, unsigned slot, const bulletin_t* t)
{
    if (slot >= THREAD_SLOTS || t->version != THREAD_VERSION || !t->generation || t->generation > THREAD_LAST_ID || t->count > THREAD_COMMENTS || t->unread > t->count || t->active > 1 || !memchr(t->title, 0, sizeof(t->title)) || !memchr(t->body, 0, sizeof(t->body)))
        return false;
    if (t->active && (!bulletin_text_valid(t->title, THREAD_TITLE) || !bulletin_text_valid(t->body, THREAD_TEXT)))
        return false;
    for (unsigned i = 0; i < t->count; i++) {
        const bulletin_comment_t* c = &t->comments[i];
        if (!memchr(c->name, 0, sizeof(c->name)) || !memchr(c->text, 0, sizeof(c->text)) || !bulletin_text_valid(c->name, THREAD_NAME) || !bulletin_text_valid(c->text, THREAD_TEXT))
            return false;
    }
    if (!memchr(t->author_name, 0, sizeof(t->author_name)) || !bulletin_text_valid(t->author_name, THREAD_NAME))
        return false;
    b->slots[slot] = *t;
    return true;
}
bool bulletin_visible(const bulletin_t* t, uint32_t now)
{
    return t->active && (!t->expires || (now >= 1700000000 && now < t->expires));
}
uint64_t bulletins_total_threads(const bulletins_t* b)
{
    uint64_t total = 0;
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].lifetime_threads > total)
            total = b->slots[i].lifetime_threads;
    return total;
}
uint64_t bulletins_total_replies(const bulletins_t* b)
{
    uint64_t total = 0;
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].lifetime_replies > total)
            total = b->slots[i].lifetime_replies;
    return total;
}
void bulletin_id(uint32_t value, char out[8])
{
    if (!value || value > THREAD_LAST_ID) {
        strcpy(out, "-------");
        return;
    }
    out[0] = (char)('A' + value / 1000000);
    value %= 1000000;
    for (int i = 6; i >= 1; --i) {
        out[i] = (char)('0' + value % 10);
        value /= 10;
    }
    out[7] = 0;
}
static bool save(bulletins_t* b, unsigned slot, bulletin_t* t)
{
    t->lifetime_threads = bulletins_total_threads(b) + (t->active && !b->slots[slot].active ? 1 : 0);
    t->lifetime_replies = bulletins_total_replies(b) + (t->active && b->slots[slot].active && t->count > b->slots[slot].count ? t->count - b->slots[slot].count : 0);
    if (!b->save || !b->save(slot, t, b->ctx))
        return false;
    b->slots[slot] = *t;
    return true;
}
bool bulletin_create(bulletins_t* b, unsigned slot, const char* title, const char* body, uint32_t now, uint32_t expires)
{
    if (slot >= THREAD_SLOTS || now < 1700000000 || (expires && expires <= now) || !bulletin_text_valid(title, THREAD_TITLE) || !bulletin_text_valid(body, THREAD_TEXT) || b->slots[slot].active || b->slots[slot].generation == UINT32_MAX)
        return false;
    uint32_t generation = 0;
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].generation > generation)
            generation = b->slots[i].generation;
    if (generation >= THREAD_LAST_ID)
        return false;
    bulletin_t t = { .version = THREAD_VERSION, .generation = generation + 1, .created = now, .expires = expires, .active = 1 };
    strcpy(t.author_name, "Owner");
    strcpy(t.title, title);
    strcpy(t.body, body);
    return save(b, slot, &t);
}
bool bulletin_delete(bulletins_t* b, unsigned slot, uint32_t generation)
{
    if (slot >= THREAD_SLOTS || !b->slots[slot].active || b->slots[slot].generation != generation)
        return false;
    bulletin_t t = { .version = THREAD_VERSION, .generation = generation, .author_node = b->slots[slot].author_node, .author_packet = b->slots[slot].author_packet };
    strcpy(t.author_name, "Deleted");
    return save(b, slot, &t);
}
bool bulletin_seen(bulletins_t* b, unsigned slot, uint32_t generation, unsigned count_viewed)
{
    if (slot >= THREAD_SLOTS || b->slots[slot].generation != generation || !b->slots[slot].active || count_viewed > b->slots[slot].count)
        return false;
    if (!b->slots[slot].unread)
        return true;
    bulletin_t t = b->slots[slot];
    unsigned remaining = t.count - count_viewed;
    if (remaining >= t.unread)
        return true;
    t.unread = (uint16_t)remaining;
    return save(b, slot, &t);
}
bool bulletin_expiry(bulletins_t* b, unsigned slot, uint32_t generation, uint32_t expires, uint32_t now)
{
    if (slot >= THREAD_SLOTS || now < 1700000000 || (expires && expires <= now) || !b->slots[slot].active || b->slots[slot].generation != generation)
        return false;
    bulletin_t t = b->slots[slot];
    t.expires = expires;
    return save(b, slot, &t);
}
bool bulletins_unread(const bulletins_t* b, uint32_t now)
{
    (void)now;
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].active && b->slots[i].unread)
            return true;
    return false;
}
static void emit(bulletin_reply_t* r, const char* s)
{
    while (*s && r->count < THREAD_PACKETS) {
        size_t n = strlen(s);
        if (n > BBS_REPLY_MAX - 12) {
            n = BBS_REPLY_MAX - 12;
            while (n && ((unsigned char)s[n] & 0xc0) == 0x80)
                --n;
        }
        memcpy(r->text[r->count], s, n);
        r->text[r->count++][n] = 0;
        s += n;
    }
}
static void stamp(uint32_t at, char out[24])
{
    if (at < 1700000000) {
        strcpy(out, "time unknown");
        return;
    }
    time_t raw = at;
    struct tm date;
#ifdef _WIN32
    gmtime_s(&date, &raw);
#else
    gmtime_r(&raw, &date);
#endif
    strftime(out, 24, "%Y-%m-%d %H:%M UTC", &date);
}
int bulletin_free(const bulletins_t* b)
{
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (!b->slots[i].active)
            return (int)i;
    return -1;
}
int bulletin_by_id(const bulletins_t* b, uint32_t id)
{
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].active && b->slots[i].generation == id)
            return (int)i;
    return -1;
}
int bulletin_index(const bulletins_t* b, unsigned ordinal, uint32_t now)
{
    uint32_t after = 0;
    int result = -1;
    for (unsigned rank = 0; rank <= ordinal; rank++) {
        uint32_t next = UINT32_MAX;
        result = -1;
        for (unsigned i = 0; i < THREAD_SLOTS; i++)
            if (bulletin_visible(&b->slots[i], now) && b->slots[i].generation > after && b->slots[i].generation < next) {
                next = b->slots[i].generation;
                result = (int)i;
            }
        if (result < 0)
            return -1;
        after = next;
    }
    return result;
}
unsigned bulletins_active(const bulletins_t* b, uint32_t now)
{
    unsigned count = 0;
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (bulletin_visible(&b->slots[i], now))
            ++count;
    return count;
}
void bulletins_queue_storage(bulletins_t* b, bulletin_queue_save_fn fn) { b->save_queue = fn; }
bool bulletins_restore_queue(bulletins_t* b, const bulletin_queue_t* q)
{
    if (q->version != 1 || q->count > THREAD_QUEUE)
        return false;
    for (unsigned i = 0; i < q->count; i++) {
        const bulletin_pending_t* p = &q->entries[i];
        if (!p->node || p->node == UINT32_MAX || !p->packet_id || !memchr(p->name, 0, sizeof(p->name)) || !memchr(p->title, 0, sizeof(p->title)) || !memchr(p->body, 0, sizeof(p->body)) || !bulletin_text_valid(p->name, THREAD_NAME) || !bulletin_text_valid(p->title, THREAD_TITLE) || !bulletin_text_valid(p->body, THREAD_TEXT))
            return false;
        for (unsigned j = 0; j < i; j++)
            if (q->entries[j].node == p->node && q->entries[j].packet_id == p->packet_id)
                return false;
    }
    b->queue = *q;
    return true;
}
static bool save_queue(bulletins_t* b, const bulletin_queue_t* q)
{
    if (!b->save_queue || !b->save_queue(q, b->ctx))
        return false;
    b->queue = *q;
    return true;
}
static bool remove_pending(bulletins_t* b, unsigned index)
{
    bulletin_queue_t* q = malloc(sizeof(*q));
    if (!q)
        return false;
    *q = b->queue;
    memmove(&q->entries[index], &q->entries[index + 1], (q->count - index - 1) * sizeof(q->entries[0]));
    memset(&q->entries[--q->count], 0, sizeof(q->entries[0]));
    bool ok = save_queue(b, q);
    free(q);
    return ok;
}
bool bulletins_ban_pending(bulletins_t* b, uint32_t node)
{
    for (unsigned i = 0; i < b->queue.count;)
        if (b->queue.entries[i].node == node) {
            if (!remove_pending(b, i))
                return false;
        } else
            ++i;
    return true;
}
bool bulletins_remove_pending(bulletins_t* b, uint32_t node, uint32_t packet)
{
    for (unsigned i = 0; i < b->queue.count; i++)
        if (b->queue.entries[i].node == node && b->queue.entries[i].packet_id == packet)
            return remove_pending(b, i);
    return false;
}
static int published(const bulletins_t* b, uint32_t node, uint32_t packet)
{
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].author_node == node && b->slots[i].author_packet == packet)
            return (int)i;
    return -1;
}
static bool publish(bulletins_t* b, unsigned slot, const bulletin_pending_t* p, uint32_t now)
{
    uint32_t generation = 0;
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].generation > generation)
            generation = b->slots[i].generation;
    if (generation >= THREAD_LAST_ID || now > UINT32_MAX - 86400)
        return false;
    bulletin_t* t = calloc(1, sizeof(*t));
    if (!t)
        return false;
    t->version = THREAD_VERSION;
    t->generation = generation + 1;
    t->created = now;
    t->expires = now + 86400;
    t->active = 1;
    t->author_node = p->node;
    t->author_packet = p->packet_id;
    strcpy(t->author_name, p->name);
    strcpy(t->title, p->title);
    strcpy(t->body, p->body);
    bool ok = save(b, slot, t);
    free(t);
    return ok;
}
bool bulletins_tick(bulletins_t* b, uint32_t now)
{
    if (now < 1700000000)
        return true;
    /* Retire records before publishing. Every write is committed before success.
     * If power fails between publish and queue removal, sender/packet deduplication
     * recognizes the published record and completes the queue removal on reboot. */
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].active && b->slots[i].expires && b->slots[i].expires <= now)
            if (!bulletin_delete(b, i, b->slots[i].generation))
                return false;
    while (b->queue.count) {
        const bulletin_pending_t* p = &b->queue.entries[0];
        if (published(b, p->node, p->packet_id) < 0) {
            int slot = bulletin_free(b);
            if (slot < 0)
                break;
            if (!publish(b, (unsigned)slot, p, now))
                return false;
        }
        if (!remove_pending(b, 0))
            return false;
    }
    return true;
}
static bool parse_id(const char* text, uint32_t* id, const char** end)
{
    if (*text == '#')
        ++text;
    unsigned char letter = (unsigned char)toupper((unsigned char)*text++);
    if (letter < 'A' || letter > 'Z')
        return false;
    uint32_t value = (uint32_t)(letter - 'A') * 1000000;
    uint32_t digits = 0;
    for (unsigned i = 0; i < 6; i++) {
        if (*text < '0' || *text > '9')
            return false;
        digits = digits * 10 + (unsigned)(*text++ - '0');
    }
    value += digits;
    if (!value || value > THREAD_LAST_ID)
        return false;
    *id = value;
    *end = text;
    return true;
}
static void queue_message(const bulletins_t* b, unsigned position, bulletin_reply_t* r)
{
    uint32_t next = UINT32_MAX;
    for (unsigned i = 0; i < THREAD_SLOTS; i++)
        if (b->slots[i].active && b->slots[i].expires && b->slots[i].expires < next)
            next = b->slots[i].expires;
    char text[189], date[24];
    stamp(next, date);
    if (next == UINT32_MAX)
        snprintf(text, sizeof(text), "BBS: Saved in queue #%u. No slot has a scheduled expiry; waiting for owner removal. Your 24 hours starts when published. QUEUE checks status.", position);
    else
        snprintf(text, sizeof(text), "BBS: Saved in queue #%u. Next slot scheduled: %s (may change). Your 24 hours starts when published. QUEUE checks status.", position, date);
    emit(r, text);
}
static void command_reply(bulletins_t* b, const bbs_request_t* req, const char* name, const char* command, uint32_t now, bulletin_reply_t* r)
{
    r->count = 0;
    uint32_t thread_id = 0;
    const char* id_end = NULL;
    if (!bulletins_tick(b, now)) {
        emit(r, "BBS: Storage maintenance failed. Please retry later; no new action was taken.");
        return;
    }
    while (*command == ' ')
        ++command;
    char upper[16];
    size_t length = strlen(command);
    unsigned i;
    for (i = 0; i < length && i < sizeof(upper) - 1; i++)
        upper[i] = (char)toupper((unsigned char)command[i]);
    upper[i] = 0;
    if (!strcmp(upper, "RULES")) {
        for (unsigned rule = 0; rule < RULE_COUNT; rule++)
            if (b->rules.lines[rule][0]) {
                char line[180];
                snprintf(line, sizeof(line), "BBS RULES: %s", b->rules.lines[rule]);
                emit(r, line);
            }
        return;
    }
    if (!strcmp(upper, "PING")) {
        emit(r, "BBS: PONG. Send UPDATE for bulletins or HELP for commands.");
        return;
    }
    if (!strcmp(upper, "HELP") || !strcmp(upper, "?")) {
        emit(r, "BBS: UPDATE lists threads, oldest first. Send A000001 to read that ID; !A000001 text replies. CREATE title | text starts a 24h thread or queues it. QUEUE checks your place. PING tests delivery.");
        emit(r, "RULES: board rules. IDs never change. Titles: 48 bytes; text: 160 UTF-8 bytes. Wait 10s between requests and let multipart replies finish. Moderation/settings are app-only.");
        return;
    }
    if (!strcmp(upper, "QUEUE")) {
        unsigned found = 0;
        for (i = 0; i < b->queue.count; i++)
            if (b->queue.entries[i].node == req->sender) {
                queue_message(b, i + 1, r);
                ++found;
            }
        if (!found)
            emit(r, "BBS: You have no waiting bulletins. Send UPDATE to see published threads.");
        return;
    }
    if (!strncmp(upper, "CREATE ", 7)) {
        if (now < 1700000000) {
            emit(r, "BBS: Clock awaiting sync; cannot start a timed bulletin yet. Ask the owner to open the app.");
            return;
        }
        for (i = 0; i < b->queue.count; i++)
            if (b->queue.entries[i].node == req->sender && b->queue.entries[i].packet_id == req->packet_id) {
                queue_message(b, i + 1, r);
                return;
            }
        if (published(b, req->sender, req->packet_id) >= 0) {
            emit(r, "BBS: That creation request was already processed. Send UPDATE.");
            return;
        }
        const char *start = command + 7, *divider = strchr(start, '|');
        if (!divider) {
            emit(r, "BBS: Use CREATE title | bulletin text. Titles: 48 bytes; text: 160 UTF-8 bytes. Visitor threads expire after 24 hours.");
            return;
        }
        while (*start == ' ')
            ++start;
        const char* end = divider;
        while (end > start && end[-1] == ' ')
            --end;
        const char* body = divider + 1;
        while (*body == ' ')
            ++body;
        bulletin_pending_t item = { .node = req->sender, .packet_id = req->packet_id, .submitted = now };
        size_t n = (size_t)(end - start);
        if (!n || n > THREAD_TITLE || !bulletin_text_valid(body, THREAD_TEXT)) {
            emit(r, "BBS: Use CREATE title | text, with a 1-48 byte title and 1-160 byte body.");
            return;
        }
        memcpy(item.title, start, n);
        strcpy(item.body, body);
        if (!bulletin_text_valid(item.title, THREAD_TITLE)) {
            emit(r, "BBS: Invalid title; use ordinary UTF-8 text.");
            return;
        }
        if (name && bulletin_text_valid(name, THREAD_NAME))
            strcpy(item.name, name);
        else
            snprintf(item.name, sizeof(item.name), "!%08" PRIx32, req->sender);
        int slot = bulletin_free(b);
        if (slot >= 0 && !b->queue.count) {
            if (!publish(b, (unsigned)slot, &item, now)) {
                emit(r, "BBS: Storage error; bulletin NOT saved.");
                return;
            }
            char text[160], date[24], id[8];
            bulletin_id(b->slots[slot].generation, id);
            stamp(now + 86400, date);
            snprintf(text, sizeof(text), "BBS: Published as %s. Read: %s; reply: !%s text. Expires: %s.", id, id, id, date);
            emit(r, text);
            return;
        }
        if (b->queue.count == THREAD_QUEUE) {
            emit(r, "BBS: All 9 active slots and 32 waiting places are full. Bulletin NOT saved; try again later.");
            return;
        }
        bulletin_queue_t* q = malloc(sizeof(*q));
        if (!q) {
            emit(r, "BBS: Memory busy; bulletin NOT saved.");
            return;
        }
        *q = b->queue;
        q->entries[q->count++] = item;
        bool ok = save_queue(b, q);
        free(q);
        if (ok)
            queue_message(b, b->queue.count, r);
        else
            emit(r, "BBS: Storage error; bulletin NOT queued.");
        return;
    }
    if (!strcmp(upper, "UPDATE") || !strcmp(upper, "LIST")) {
        /* Keep each list entry intact after the multipart numbering is added. */
        char page[BBS_REPLY_MAX - 11] = "BBS: position. ID title. Read/reply by ID\n", line[100];
        unsigned count = 0;
        for (i = 0; i < THREAD_SLOTS; i++) {
            int slot = bulletin_index(b, i, now);
            if (slot < 0)
                break;
            const bulletin_t* t = &b->slots[slot];
            char id[8];
            bulletin_id(t->generation, id);
            snprintf(line, sizeof(line), "%u. %s %s (%u replies)\n", i + 1, id, t->title, t->count);
            if (strlen(page) + strlen(line) >= sizeof(page)) {
                emit(r, page);
                page[0] = 0;
            }
            strcat(page, line);
            ++count;
        }
        if (count)
            emit(r, page);
        else
            emit(r, now < 1700000000 ? "BBS: Clock awaiting sync; timed threads are withheld." : "BBS: No bulletins. Start one: CREATE title | text (24 hours).");
        return;
    }
    if (parse_id(command, &thread_id, &id_end) && !*id_end) {
        char id[8];
        bulletin_id(thread_id, id);
        int slot = bulletin_by_id(b, thread_id);
        if (slot < 0 || !bulletin_visible(&b->slots[slot], now)) {
            emit(r, "BBS: Bulletin ID unavailable or expired. Send UPDATE.");
            return;
        }
        const bulletin_t* t = &b->slots[slot];
        r->thread_id = t->generation;
        r->expires = t->expires;
        char line[400], date[24];
        stamp(t->created, date);
        snprintf(line, sizeof(line), "BBS %s: %s\n%s | %s !%08" PRIx32 "\n%s", id, t->title, date, t->author_name, t->author_node, t->body);
        emit(r, line);
        for (i = 0; i < t->count; i++) {
            const bulletin_comment_t* c = &t->comments[i];
            stamp(c->at, date);
            snprintf(line, sizeof(line), "BBS %s reply %u | %s\n%s !%08" PRIx32 "\n%s", id, i + 1, date, c->name, c->node, c->text);
            emit(r, line);
        }
        stamp(t->expires, date);
        snprintf(line, sizeof(line), "BBS %s end (%u replies). Reply: !%s text. This ID stays fixed.\nExpires: %s.", id, t->count, id, t->expires ? date : "never (owner setting)");
        emit(r, line);
        return;
    }
    if (command[0] == '!' && parse_id(command + 1, &thread_id, &id_end) && *id_end == ' ') {
        char id[8];
        bulletin_id(thread_id, id);
        int slot = bulletin_by_id(b, thread_id);
        const char* text = id_end + 1;
        while (*text == ' ')
            ++text;
        if (slot < 0 || !bulletin_visible(&b->slots[slot], now)) {
            emit(r, "BBS: Bulletin ID unavailable or expired. Send UPDATE.");
            return;
        }
        const bulletin_t* old = &b->slots[slot];
        if (!bulletin_text_valid(text, THREAD_TEXT)) {
            emit(r, "BBS: Use !ID your comment, maximum 160 UTF-8 bytes.");
            return;
        }
        for (i = 0; i < old->count; i++)
            if (old->comments[i].node == req->sender && old->comments[i].packet_id == req->packet_id) {
                emit(r, "BBS: That comment is already saved.");
                return;
            }
        if (old->count == THREAD_COMMENTS) {
            emit(r, "BBS: This thread has 32 replies and is full. Existing replies remain readable.");
            return;
        }
        bulletin_t* next = malloc(sizeof(*next));
        if (!next) {
            emit(r, "BBS: Memory busy; comment NOT saved.");
            return;
        }
        *next = *old;
        bulletin_comment_t* c = &next->comments[next->count++];
        memset(c, 0, sizeof(*c));
        c->node = req->sender;
        c->packet_id = req->packet_id;
        c->at = now;
        if (name && bulletin_text_valid(name, THREAD_NAME))
            strcpy(c->name, name);
        else
            snprintf(c->name, sizeof(c->name), "!%08" PRIx32, req->sender);
        strcpy(c->text, text);
        next->unread++;
        bool ok = save(b, (unsigned)slot, next);
        free(next);
        if (ok) {
            char text[100];
            snprintf(text, sizeof(text), "BBS: Comment saved to %s. Send %s to read the thread.", id, id);
            emit(r, text);
        } else
            emit(r, "BBS: Storage error; comment NOT saved.");
        return;
    }
    emit(r, "BBS: Command not recognized. Send HELP for commands and a short guide, or UPDATE for bulletins. Moderation/settings are owner-app only.");
}
void bulletins_command(bulletins_t* b, const bbs_request_t* req, const char* name, const char* command, uint32_t now, bulletin_reply_t* r)
{
    r->thread_id = 0;
    r->expires = 0;
    command_reply(b, req, name, command, now, r);
    if (r->count > 1)
        for (unsigned i = 0; i < r->count; i++) {
            char part[BBS_REPLY_MAX + 1];
            int prefix = snprintf(part, sizeof(part), "(%u/%u) ", i + 1, r->count);
            size_t length = strlen(r->text[i]);
            if (length > sizeof(part) - (size_t)prefix - 1)
                length = sizeof(part) - (size_t)prefix - 1;
            memcpy(part + prefix, r->text[i], length);
            part[prefix + length] = 0;
            strcpy(r->text[i], part);
        }
}
