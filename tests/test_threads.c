#include "bans.h"
#include "bulletins.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bulletins_t board;
static bulletin_t disk[THREAD_SLOTS];
static bulletin_reply_t reply;
static unsigned writes;
static bool fail_disk, fail_queue;
static bulletin_queue_t queue_disk;
static bool persist_waiting(const bulletin_queue_t* q, void* ctx)
{
    (void)ctx;
    if (fail_queue)
        return false;
    queue_disk = *q;
    return true;
}
static uint32_t now = 1790200000;
static bbs_request_t visitor = { .sender = 0x12345678, .packet_id = 1 };
static bool persist_thread(unsigned i, const bulletin_t* t, void* ctx)
{
    (void)ctx;
    if (fail_disk)
        return false;
    disk[i] = *t;
    ++writes;
    return true;
}
static void command(const char* text)
{
    char converted[256], id[8];
    const char* start = text;
    bool comment = *start == '!';
    if (comment || *start == '#')
        ++start;
    if (*start >= '1' && *start <= '9') {
        char* end;
        unsigned long value = strtoul(start, &end, 10);
        if (value <= THREAD_LAST_ID && (!*end || (comment && *end == ' '))) {
            bulletin_id((uint32_t)value, id);
            snprintf(converted, sizeof(converted), "%s%s%s", comment ? "!" : "", id, end);
            text = converted;
        }
    }
    bulletins_command(&board, &visitor, "Visitor", text, now, &reply);
    assert(reply.count && reply.count <= THREAD_PACKETS);
    for (unsigned i = 0; i < reply.count; i++) {
        assert(strlen(reply.text[i]) <= BBS_REPLY_MAX);
        /* Thread packets deliberately include newline formatting. */
        char line[BBS_REPLY_MAX + 1];
        strcpy(line, reply.text[i]);
        for (char* p = line; *p; p++)
            if (*p == '\n')
                *p = ' ';
        assert(bulletin_text_valid(line, BBS_REPLY_MAX));
        if (reply.count > 1) {
            char prefix[20];
            snprintf(prefix, sizeof(prefix), "(%u/%u) ", i + 1, reply.count);
            assert(!strncmp(reply.text[i], prefix, strlen(prefix)));
        }
    }
}
static bool contains(const char* s)
{
    for (unsigned i = 0; i < reply.count; i++)
        if (strstr(reply.text[i], s))
            return true;
    return false;
}
static void test_queue(void);
void test_threads(void)
{
    bulletins_init(&board, persist_thread, NULL);
    command("UPDATE");
    assert(contains("No bulletins"));
    assert(!bulletin_create(&board, 0, "Title", "Body", 0, 0));
    assert(!bulletin_create(&board, 0, "Title", "Body", now, now));
    assert(!bulletin_create(&board, 9, "Title", "Body", now, 0));
    assert(!bulletin_text_valid("\xc0\xaf", 2));
    assert(!bulletin_text_valid("\xed\xa0\x80", 3));
    assert(!bulletin_text_valid("\xf4\x90\x80\x80", 4));
    assert(!bulletin_text_valid("a\nb", 3));
    assert(bulletin_text_valid("caf\xc3\xa9", 5));
    fail_disk = true;
    assert(!bulletin_create(&board, 0, "Title", "Body", now, 0));
    assert(!board.slots[0].active);
    fail_disk = false;
    for (unsigned i = 0; i < 9; i++)
        assert(bulletin_create(&board, i, "A long bulletin title for compact list testing", "Welcome to the bulletin board.", now, now + 86400));
    assert(!bulletin_create(&board, 0, "Overwrite", "No", now, 0));
    command("UPDATE");
    assert(reply.count <= (BBS_REPLY_MAX == 160 ? 7 : 5));
    assert(contains("9. A000009"));
    unsigned before = writes;
    const char* forbidden[] = { "POST new", "DEL 1", "DELETE 1", "BAN 12345678", "SETUP", "!x invalid", "0", "ADMIN" };
    for (unsigned i = 0; i < sizeof(forbidden) / sizeof(*forbidden); i++) {
        command(forbidden[i]);
        assert(contains("HELP"));
        assert(writes == before);
    }
    command("help");
    assert(contains("UPDATE"));
    command("ping");
    assert(contains("PONG"));
    command("!1 First comment");
    assert(contains("saved"));
    assert(board.slots[0].count == 1 && board.slots[0].unread == 1);
    before = writes;
    command("!1 First comment");
    assert(contains("already saved"));
    assert(writes == before);
    visitor.packet_id++;
    fail_disk = true;
    command("!1 Unsaved comment");
    assert(contains("NOT saved"));
    assert(board.slots[0].count == 1);
    fail_disk = false;
    command("!1 Second comment");
    assert(board.slots[0].unread == 2);
    assert(bulletin_seen(&board, 0, 1, 1));
    assert(board.slots[0].unread == 1); /* Arrived while first comment was displayed. */
    assert(bulletin_seen(&board, 0, 1, 0));
    assert(board.slots[0].unread == 1); /* An older view cannot mark newer comments read. */
    assert(!bulletin_seen(&board, 0, 2, 2));
    assert(!bulletin_seen(&board, 0, 1, 3));
    assert(bulletin_seen(&board, 0, 1, 2));
    assert(!board.slots[0].unread);
    char long_command[164] = "!1 ";
    for (unsigned i = 3; i < 163; i += 4)
        memcpy(long_command + i, "\xf0\x9f\x93\xbb", 4);
    long_command[163] = 0;
    for (unsigned i = 2; i < THREAD_COMMENTS; i++) {
        visitor.packet_id++;
        command(long_command);
        assert(contains("saved"));
    }
    visitor.packet_id++;
    command("!1 overflow");
    assert(contains("full"));
    assert(board.slots[0].count == 32);
    command("1");
    assert(reply.count > 32 && reply.count <= THREAD_PACKETS);
    assert(contains("reply 32"));
    assert(contains("UTC"));
    assert(contains("Visitor !12345678"));
    assert(contains("end (32 replies)"));
    bulletins_init(&board, persist_thread, NULL);
    for (unsigned i = 0; i < 9; i++)
        assert(bulletin_restore(&board, i, &disk[i]));
    assert(board.slots[0].count == 32 && board.slots[0].unread == 30);
    command("1");
    assert(contains("reply 32"));
    assert(bulletins_unread(&board, now));
    now += 86400;
    assert(!bulletin_visible(&board.slots[0], now));
    assert(bulletins_unread(&board, now)); /* Expiry does not mark comments read. */
    /* Revive before the expiry sweep to test the owner override. */
    assert(bulletin_expiry(&board, 0, 1, 0, now));
    assert(bulletin_visible(&board.slots[0], 0));
    assert(!bulletin_visible(&board.slots[1], 0));
    assert(!bulletin_delete(&board, 0, 2));
    fail_disk = true;
    assert(!bulletin_delete(&board, 0, 1));
    assert(board.slots[0].active);
    fail_disk = false;
    assert(bulletin_delete(&board, 0, 1));
    assert(!board.slots[0].active);
    assert(bulletin_create(&board, 0, "Replacement", "Different thread", now, 0));
    assert(board.slots[0].generation == 10 && board.slots[0].count == 0);
    assert(!bulletin_seen(&board, 0, 1, 0));
    assert(!bulletin_expiry(&board, 0, 1, 0, now));
    bulletin_t bad = disk[0];
    bad.count = 33;
    assert(!bulletin_restore(&board, 0, &bad));
    bad = disk[0];
    memset(bad.title, 'x', sizeof(bad.title));
    assert(!bulletin_restore(&board, 0, &bad));
    bans_t bans;
    bans_init(&bans);
    assert(bans_valid(&bans));
    uint32_t id = 0;
    assert(bans_parse_node("!A1B24B58", &id) && id == 0xa1b24b58);
    assert(!bans_parse_node("4b58", &id));
    assert(!bans_parse_node("!ffffffff", &id));
    assert(!bans_parse_node("!00000000", &id));
    assert(!bans_parse_node("!a1b24b5g", &id));
    assert(bans_set(&bans, id, true));
    assert(bans_contains(&bans, id));
    assert(bans_set(&bans, id, true) && bans.count == 1);
    for (unsigned i = 1; i < BAN_LIMIT; i++)
        assert(bans_set(&bans, i, true));
    assert(bans.count == 64);
    assert(!bans_set(&bans, 200, true));
    assert(bans_set(&bans, id, false));
    assert(!bans_contains(&bans, id));
    assert(bans_set(&bans, 200, true));
    assert(bans_valid(&bans));
    assert(!bans_message(&bans, ""));
    assert(bans_message(&bans, "BBS: You are banned."));
    bans_t restored;
    memcpy(&restored, &bans, sizeof(bans));
    assert(bans_valid(&restored) && bans_contains(&restored, 200));
    assert(!strcmp(restored.message, "BBS: You are banned."));
    restored.nodes[0] = restored.nodes[1];
    assert(!bans_valid(&restored));
    test_queue();
    puts("PASS: nine threads, full multipart UTF-8 replies, comments, expiry, read races, generation guards, persistence failures, radio admin rejection and bans");
}

static void test_queue(void)
{
    memset(disk, 0, sizeof(disk));
    memset(&queue_disk, 0, sizeof(queue_disk));
    bulletins_init(&board, persist_thread, NULL);
    bulletins_queue_storage(&board, persist_waiting);
    now = 1790200000;
    visitor.sender = 0x12345678;
    visitor.packet_id = 100;
    for (unsigned i = 0; i < 9; i++)
        assert(bulletin_create(&board, i, "Owner thread", "Body", now, now + 100 + i));
    command("CREATE Visitor title | Visitor body");
    assert(contains("queue #1"));
    assert(contains("scheduled:"));
    assert(board.queue.count == 1);
    unsigned before = writes;
    command("CREATE Visitor title | Visitor body");
    assert(board.queue.count == 1 && writes == before);
    visitor.packet_id++;
    fail_queue = true;
    command("CREATE Not saved | Body");
    assert(contains("NOT queued"));
    assert(board.queue.count == 1);
    fail_queue = false;
    command("CREATE Second title | Second body");
    assert(contains("queue #2"));
    assert(board.queue.count == 2);
    command("QUEUE");
    assert(contains("queue #1") && contains("queue #2"));
    now += 100;
    fail_queue = true;
    assert(!bulletins_tick(&board, now));
    assert(board.slots[0].generation == 10 && board.slots[0].author_packet == 100);
    assert(board.queue.count == 2);
    /* Reboot at the commit boundary: publishing succeeded, dequeue did not. */
    bulletins_init(&board, persist_thread, NULL);
    bulletins_queue_storage(&board, persist_waiting);
    for (unsigned i = 0; i < 9; i++)
        assert(bulletin_restore(&board, i, &disk[i]));
    assert(bulletins_restore_queue(&board, &queue_disk));
    fail_queue = false;
    assert(bulletins_tick(&board, now));
    assert(board.queue.count == 1);
    assert(board.slots[0].generation == 10);
    assert(bulletin_index(&board, 0, now) == 1);
    command("UPDATE");
    assert(contains("1. A000002"));
    assert(contains("9. A000010"));
    command("10");
    assert(contains("Visitor body"));
    assert(contains("Expires:"));
    assert(contains("Visitor !12345678"));
    visitor.packet_id++;
    command("!10 Same thread after renumbering");
    assert(contains("saved to A000010"));
    assert(board.slots[0].count == 1);
    command("1");
    assert(contains("unavailable"));
    assert(board.slots[1].count == 0);
    command("4294967296");
    assert(contains("HELP"));
    command("!4294967296 bad");
    assert(contains("HELP"));
    now++;
    assert(bulletins_tick(&board, now));
    assert(board.queue.count == 0 && board.slots[1].generation == 11);
    assert(board.slots[1].expires == now + 86400);
    assert(board.slots[0].expires == now - 1 + 86400);
    assert(bulletin_index(&board, 0, now) == 2);
    command("#11");
    assert(contains("Second body"));
    command("!11 comment");
    assert(contains("saved to A000011"));
    /* No scheduled expiry means the queue must say so, without inventing an ETA. */
    for (unsigned i = 0; i < 9; i++)
        assert(bulletin_expiry(&board, i, board.slots[i].generation, 0, now));
    visitor.packet_id++;
    command("CREATE Waiting | No deadline");
    assert(contains("No slot has a scheduled expiry"));
    assert(bulletins_ban_pending(&board, visitor.sender));
    assert(!board.queue.count);
    for (unsigned i = 0; i < THREAD_QUEUE; i++) {
        visitor.packet_id++;
        command("CREATE Queued | Body");
        assert(contains("Saved in queue"));
    }
    visitor.packet_id++;
    command("CREATE Overflow | Body");
    assert(contains("NOT saved"));
    assert(board.queue.count == THREAD_QUEUE);
    assert(bulletins_ban_pending(&board, visitor.sender));
    assert(!board.queue.count);
    assert(bulletin_delete(&board, 2, board.slots[2].generation));
    visitor.packet_id++;
    command("CREATE Public | New body");
    assert(contains("Published as A000012"));
    assert(board.slots[2].expires == now + 86400);
    now += 86400;
    assert(bulletins_tick(&board, now));
    assert(!board.slots[2].active);
    assert(!strcmp(board.slots[2].body, ""));
    assert(bulletin_create(&board, 2, "After expiry", "New ID", now, 0));
    assert(board.slots[2].generation == 13);
    assert(bulletins_total_threads(&board) == 13);
    assert(bulletins_total_replies(&board) == 2);
    for (unsigned i = 0; i < 9; i++)
        if (board.slots[i].active)
            assert(bulletin_delete(&board, i, board.slots[i].generation));
    assert(bulletins_total_threads(&board) == 13 && bulletins_total_replies(&board) == 2);
    bulletin_t high = board.slots[2];
    high.generation = 999999;
    high.lifetime_threads = 999999;
    assert(bulletin_restore(&board, 2, &high));
    assert(bulletin_create(&board, 2, "Rollover", "Keep counting", now, 0));
    char id[8];
    bulletin_id(board.slots[2].generation, id);
    assert(!strcmp(id, "B000000"));
    assert(bulletins_total_threads(&board) == 1000000);
    command("B000000");
    assert(contains("Keep counting"));
    command("!B000000 still permanent");
    assert(contains("saved"));
    assert(bulletin_delete(&board, 2, 1000000));
    bulletins_init(&board, persist_thread, NULL);
    for (unsigned i = 0; i < 9; i++)
        assert(bulletin_restore(&board, i, &disk[i]));
    assert(bulletins_total_threads(&board) == 1000000 && bulletins_total_replies(&board) == 3);
    assert(bulletin_create(&board, 0, "After restart", "Next ID", now, 0));
    bulletin_id(board.slots[0].generation, id);
    assert(!strcmp(id, "B000001"));
    bulletin_id(THREAD_LAST_ID, id);
    assert(!strcmp(id, "Z999999"));
    uint64_t total = bulletins_total_threads(&board);
    command("rules");
    assert(reply.count == RULE_COUNT && contains("not an emergency service"));
    assert(rules_set(&board.rules, 0, "Keep it friendly and local."));
    command("RULES"); assert(contains("Keep it friendly and local."));
    for(unsigned i=0;i<reply.count;i++) assert(strlen(reply.text[i])<=BBS_REPLY_MAX);
    rules_t rules_before=board.rules;
    assert(!rules_set(&board.rules, RULE_COUNT, "Out of bounds"));
    assert(!rules_set(&board.rules, 0, "bad\nline"));
    assert(!rules_set(&board.rules, 0, "\xc0\xaf"));
    char too_long[162]; memset(too_long,'x',161);too_long[161]=0;
    assert(!rules_set(&board.rules,0,too_long));
    assert(!memcmp(&rules_before,&board.rules,sizeof(rules_before)));
    for(unsigned i=1;i<RULE_COUNT;i++) assert(rules_set(&board.rules,i,""));
    assert(!rules_set(&board.rules,0,""));
    command("RULES change policy"); assert(contains("not recognized"));
    command("RULES"); assert(reply.count==1 && contains("friendly"));
    assert(bulletins_total_threads(&board)==total);
    command("HELP"); assert(contains("RULES"));
    puts("PASS: configurable public RULES, owner-side validation, no radio edits, UTF-8, packet bounds and empty-rule policy");
    puts("PASS: stable IDs, moving list positions, public CREATE, expiry deletion, persistent FIFO, queue capacity, ban removal, commit-boundary reboot and full 24h after promotion");
}
