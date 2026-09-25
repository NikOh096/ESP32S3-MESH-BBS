#include "rules.h"
#include "bulletins.h"
#include <string.h>
void rules_init(rules_t* r)
{
    memset(r, 0, sizeof(*r));
    r->version = 1;
    const char* defaults[RULE_COUNT] = {
        "Be respectful. No threats, harassment, hate, or illegal content. Keep discussion useful to the community.",
        "Treat posts as public. Do not share passwords, private locations, or anyone's personal details without consent.",
        "This BBS is not an emergency service. Delivery and monitoring are not guaranteed; use local emergency services for urgent help.",
        "Verify claims before acting or reposting. Label uncertainty. Node names and IDs do not prove who someone is.",
        "Keep messages brief and relevant. No spam or repeated requests. Wait 10 seconds between commands and let multipart replies finish.",
        "The owner may remove posts or ban nodes. New visitor threads expire 24 hours after publication. Send HELP for commands."
    };
    for (unsigned i = 0; i < RULE_COUNT; i++)
        strcpy(r->lines[i], defaults[i]);
}
bool rules_valid(const rules_t* r)
{
    if (r->version != 1)
        return false;
    bool any = false;
    for (unsigned i = 0; i < RULE_COUNT; i++) {
        if (!memchr(r->lines[i], 0, RULE_BYTES + 1))
            return false;
        if (r->lines[i][0]) {
            if (!bulletin_text_valid(r->lines[i], RULE_BYTES))
                return false;
            any = true;
        }
    }
    return any;
}
bool rules_set(rules_t* r, unsigned index, const char* text)
{
    if (index >= RULE_COUNT || !text || (*text && !bulletin_text_valid(text, RULE_BYTES)))
        return false;
    rules_t next = *r;
    strcpy(next.lines[index], text);
    if (!rules_valid(&next))
        return false;
    *r = next;
    return true;
}
