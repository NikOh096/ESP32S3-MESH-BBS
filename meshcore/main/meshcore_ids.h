#pragma once
#include <stdbool.h>
#include <stdint.h>
#define MC_ID_LIMIT 512
typedef struct {
    uint32_t alias;
    uint8_t key[32];
} mc_identity_t;
typedef bool (*mc_id_save_fn)(unsigned, const mc_identity_t*, void*);
typedef struct {
    unsigned count;
    mc_identity_t entries[MC_ID_LIMIT];
    mc_id_save_fn save;
    void* ctx;
} mc_ids_t;
void mc_ids_init(mc_ids_t*, mc_id_save_fn, void*);
bool mc_ids_restore(mc_ids_t*, unsigned, const mc_identity_t*);
uint32_t mc_ids_assign(mc_ids_t*, const uint8_t key[32]);
const mc_identity_t* mc_ids_prefix(const mc_ids_t*, const uint8_t prefix[6]);
const mc_identity_t* mc_ids_alias(const mc_ids_t*, uint32_t);
