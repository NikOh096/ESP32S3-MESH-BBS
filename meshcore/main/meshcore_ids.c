#include "meshcore_ids.h"
#include "meshcore_protocol.h"
#include <string.h>
void mc_ids_init(mc_ids_t* ids, mc_id_save_fn fn, void* ctx)
{
    memset(ids, 0, sizeof(*ids));
    ids->save = fn;
    ids->ctx = ctx;
}
const mc_identity_t* mc_ids_alias(const mc_ids_t* ids, uint32_t alias)
{
    for (unsigned i = 0; i < ids->count; i++)
        if (ids->entries[i].alias == alias)
            return &ids->entries[i];
    return NULL;
}
bool mc_ids_restore(mc_ids_t* ids, unsigned i, const mc_identity_t* entry)
{
    if (i != ids->count || i >= MC_ID_LIMIT || !entry->alias || entry->alias == UINT32_MAX || !mc_key_valid(entry->key) || mc_ids_alias(ids, entry->alias))
        return false;
    for (unsigned n = 0; n < ids->count; n++)
        if (!memcmp(ids->entries[n].key, entry->key, 32))
            return false;
    ids->entries[ids->count++] = *entry;
    return true;
}
uint32_t mc_ids_assign(mc_ids_t* ids, const uint8_t key[32])
{
    if (!mc_key_valid(key))
        return 0;
    for (unsigned i = 0; i < ids->count; i++)
        if (!memcmp(ids->entries[i].key, key, 32))
            return ids->entries[i].alias;
    if (ids->count == MC_ID_LIMIT)
        return 0;
    mc_identity_t entry = { 0 };
    memcpy(entry.key, key, 32);
    for (unsigned i = 0; i < 4; i++)
        entry.alias = (entry.alias << 8) | key[i];
    while (!entry.alias || entry.alias == UINT32_MAX || mc_ids_alias(ids, entry.alias))
        entry.alias = entry.alias == UINT32_MAX ? 1 : entry.alias + 1;
    if (!ids->save || !ids->save(ids->count, &entry, ids->ctx))
        return 0;
    ids->entries[ids->count++] = entry;
    return entry.alias;
}
const mc_identity_t* mc_ids_prefix(const mc_ids_t* ids, const uint8_t prefix[6])
{
    const mc_identity_t* match = NULL;
    for (unsigned i = 0; i < ids->count; i++)
        if (!memcmp(ids->entries[i].key, prefix, 6)) {
            if (match)
                return NULL; /* ambiguous prefix must never select another person's identity */
            match = &ids->entries[i];
        }
    return match;
}
