#include "bans.h"
#include "bulletins.h"
#include <string.h>
void bans_init(bans_t* b)
{
    memset(b, 0, sizeof(*b));
    b->version = 1;
    strcpy(b->message, "BBS: Access denied. This node is banned. Contact the board owner.");
}
bool bans_contains(const bans_t* b, uint32_t node)
{
    for (unsigned i = 0; i < b->count; i++)
        if (b->nodes[i] == node)
            return true;
    return false;
}
bool bans_valid(const bans_t* b)
{
    if (b->version != 1 || b->count > BAN_LIMIT || !memchr(b->message, 0, sizeof(b->message)) || !bulletin_text_valid(b->message, BAN_MESSAGE_MAX))
        return false;
    for (unsigned i = 0; i < b->count; i++) {
        if (!b->nodes[i] || b->nodes[i] == UINT32_MAX)
            return false;
        for (unsigned j = 0; j < i; j++)
            if (b->nodes[j] == b->nodes[i])
                return false;
    }
    return true;
}
bool bans_set(bans_t* b, uint32_t node, bool add)
{
    if (!node || node == UINT32_MAX)
        return false;
    for (unsigned i = 0; i < b->count; i++)
        if (b->nodes[i] == node) {
            if (!add) {
                memmove(&b->nodes[i], &b->nodes[i + 1], (b->count - i - 1) * sizeof(uint32_t));
                b->nodes[--b->count] = 0;
            }
            return true;
        }
    if (!add)
        return true;
    if (b->count == BAN_LIMIT)
        return false;
    b->nodes[b->count++] = node;
    return true;
}
bool bans_message(bans_t* b, const char* message)
{
    if (!message || !bulletin_text_valid(message, BAN_MESSAGE_MAX))
        return false;
    strcpy(b->message, message);
    return true;
}
bool bans_parse_node(const char* s, uint32_t* out)
{
    if (!s)
        return false;
    if (*s == '!')
        ++s;
    if (strlen(s) != 8)
        return false;
    uint32_t n = 0;
    for (unsigned i = 0; i < 8; i++) {
        unsigned char c = (unsigned char)s[i];
        unsigned digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return false;
        n = (n << 4) | digit;
    }
    if (!n || n == UINT32_MAX)
        return false;
    *out = n;
    return true;
}
