#include "meshcore_protocol.h"
#include "meshcore_ids.h"
#include "bulletins.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void test_threads(void);
void test_node_scan(void);
static mc_ids_t ids, restarted;
static mc_identity_t saved[MC_ID_LIMIT];
static bool fail;
static unsigned writes;
static bool save(unsigned i, const mc_identity_t* entry, void* ctx) {
    (void)ctx;
    if (fail) return false;
    saved[i]=*entry; writes++; return true;
}
static void identity_tests(void) {
    uint8_t key[32]={0x12,0x34,0x56,0x78,9,10,11}, other[32];
    mc_ids_init(&ids,save,NULL);
    assert(mc_ids_assign(&ids,key)==0x12345678);
    assert(writes==1 && mc_ids_assign(&ids,key)==0x12345678 && writes==1);
    memcpy(other,key,32); other[4]++;
    assert(mc_ids_assign(&ids,other)==0x12345679); // Four-byte alias collision.
    assert(mc_ids_prefix(&ids,key)->alias==0x12345678);
    mc_ids_init(&restarted,save,NULL);
    assert(mc_ids_restore(&restarted,0,&saved[0]));
    assert(mc_ids_restore(&restarted,1,&saved[1]));
    assert(mc_ids_assign(&restarted,other)==0x12345679);
    assert(!mc_ids_restore(&restarted,2,&saved[0]));
    memcpy(other,key,32); other[31]++;
    fail=true; assert(!mc_ids_assign(&ids,other) && ids.count==2);
    fail=false; assert(mc_ids_assign(&ids,other)==0x1234567a);
    assert(!mc_ids_prefix(&ids,key)); // Six-byte prefix ambiguity must fail closed.
    assert(!mc_ids_alias(&ids,0));
    memset(key,0,32); assert(!mc_ids_assign(&ids,key));
    for(unsigned i=ids.count;i<MC_ID_LIMIT;i++) {
        memset(key,0,32); key[0]=0x80; key[6]=i>>8; key[7]=i;
        assert(mc_ids_assign(&ids,key));
    }
    key[31]=99; assert(!mc_ids_assign(&ids,key));
    assert(ids.count==MC_ID_LIMIT);
    puts("PASS: full-key identities, persistent alias collisions, prefix ambiguity, storage failure and capacity");
}
static void protocol_tests(void) {
    uint8_t b[256]={0}, key[32]={1,2,3,4,5,6,7}; mc_packet_t p;
    assert(mc_start(b,sizeof b)==15 && b[0]==1 && !memcmp(b+8,"MESHBBS",7));
    assert(mc_start(b,14)==0);
    assert(mc_query(b,sizeof b)==2 && b[0]==22 && b[1]==2);
    assert(mc_get_contacts(b,sizeof b)==1 && b[0]==4);
    assert(mc_get_time(b,sizeof b)==1 && b[0]==5);
    assert(mc_next(b,sizeof b)==1 && b[0]==10);
    // v1.17.1 legacy DM: prefix(6), path(1), type(1), Unix time(4), text.
    const uint8_t dm[]={7,1,2,3,4,5,6,0xff,0,0x04,0x03,0x02,0x01,'H','E','L','P'};
    assert(mc_parse(dm,sizeof dm,&p) && p.type==7 && p.timestamp==0x01020304 && !strcmp(p.text,"HELP"));
    assert(!memcmp(p.prefix,key,6));
    for(unsigned n=0;n<=13;n++) assert(!mc_parse(dm,n,&p));
    memcpy(b,dm,sizeof dm); b[8]=1; assert(mc_parse(b,sizeof dm,&p) && !*p.text);
    b[8]=0; b[14]=0; assert(!mc_parse(b,sizeof dm,&p));
    b[14]=0xc0; b[15]=0xaf; assert(!mc_parse(b,sizeof dm,&p));
    memset(b,0,sizeof b); b[0]=16; memcpy(b+4,dm+1,sizeof dm-1);
    assert(mc_parse(b,sizeof dm+3,&p) && !strcmp(p.text,"HELP"));
    memset(b,0,sizeof b); b[0]=13; b[4]=0x40; b[5]=0xe2; b[6]=1;
    memcpy(b+20,"Heltec V3",9); memcpy(b+60,"v1.17.1",7);
    assert(mc_parse(b,82,&p) && !strcmp(p.hardware,"Heltec V3") && !strcmp(p.version,"v1.17.1") && p.fixed_pin);
    assert(!mc_parse(b,79,&p));
    const uint8_t sent[]={6,1,0x78,0x56,0x34,0x12,0xe8,3,0,0};
    assert(mc_parse(sent,sizeof sent,&p) && p.ack==0x12345678 && p.timeout_ms==1000);
    const uint8_t ack[]={0x82,0x78,0x56,0x34,0x12,0,0,0,0};
    assert(mc_parse(ack,sizeof ack,&p) && p.ack==0x12345678);
    memset(b,0,sizeof b); b[0]=3; memcpy(b+1,key,32); memcpy(b+100,"A radio",7);
    assert(mc_parse(b,148,&p) && !strcmp(p.name,"A radio") && !memcmp(p.key,key,32));
    assert(!mc_parse(b,147,&p));
    char full[162]; memset(full,'x',160); full[160]=0;
    assert(mc_send(key,full,1700000000,b,sizeof b)==173 && !memcmp(b+7,key,6));
    assert(!mc_send(key,full,1700000000,b,172));
    full[160]='x'; full[161]=0; assert(!mc_send(key,full,1700000000,b,sizeof b));
    assert(mc_send(key,"Line 1\nLine 2",1700000000,b,sizeof b)==26 && b[19]=='\n');
    assert(!mc_send(key,"HELP",0,b,sizeof b));
    uint32_t id=mc_request_id(key,1700000000,"HELP");
    assert(id==mc_request_id(key,1700000000,"HELP"));
    assert(id!=mc_request_id(key,1700000001,"HELP"));
    // Deterministic malformed-frame sweep catches boundary reads under sanitizer builds too.
    uint32_t random=42;
    for(unsigned t=0;t<20000;t++) {
        for(unsigned i=0;i<sizeof b;i++) { random=random*1664525+1013904223; b[i]=random>>24; }
        mc_parse(b,t%sizeof b,&p);
    }
    puts("PASS: MeshCore protocol vectors, UTF-8 bounds, metadata, v2/v3 DMs, ACKs and malformed input");
}
int main(void) { protocol_tests(); identity_tests(); test_node_scan(); test_threads(); puts("PASS: MeshCore host suite"); return 0; }
