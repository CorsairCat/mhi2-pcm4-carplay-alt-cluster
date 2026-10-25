#include "pcm4_projection.h"
#include "pcm4_screen_receiver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const unsigned char config[] = {1,66,0,30,255,225,0,2,0x67,0,1,0,2,0x68,0};
static const unsigned char idr[] = {0,0,0,2,0x65,0xaa};
static const unsigned char delta[] = {0,0,0,2,0x41,0xbb};
static unsigned char scratch[512000];
static int emissions;
static int collect(void *opaque, const void *data, size_t size, uint64_t timestamp)
{
    (void)opaque;
    assert(timestamp == 123);
    assert(size <= 512000 && !memcmp(data, "\0\0\0\1", 4));
    emissions++; return 0;
}
struct hmi { int takes, hides, shows, restores, fail_restore, fail_show, fail_take; uint32_t last; };
static int take(void *opaque)
{ struct hmi *h = opaque; h->takes++; return h->fail_take; }
static int visibility(void *opaque, int show)
{ struct hmi *h = opaque; if (show) { h->shows++; return h->fail_show; } h->hides++; return 0; }
static int restore(void *opaque, uint32_t context)
{ struct hmi *h = opaque; h->restores++; h->last = context; return h->fail_restore; }
static void stream_tests(void)
{
    struct pcm4_aux_stream s = {0};
    unsigned char bad[sizeof(config)], bytes[64];
    unsigned i, j;
    assert(!pcm4_aux_begin(&s, 1, collect, NULL));
    assert(pcm4_aux_access_unit(&s,1,idr,sizeof(idr),123,scratch,sizeof(scratch)) == 1);
    for (i = 0; i < sizeof(config); ++i) assert(pcm4_aux_configure(&s,1,config,i) == -1);
    memcpy(bad,config,sizeof(bad)); bad[4] = 254; /* forbidden three-byte length */
    assert(pcm4_aux_configure(&s,1,bad,sizeof(bad)) == -1);
    assert(!pcm4_aux_configure(&s,1,config,sizeof(config)));
    assert(pcm4_aux_access_unit(&s,1,delta,sizeof(delta),123,scratch,sizeof(scratch)) == 1);
    assert(!pcm4_aux_access_unit(&s,1,idr,sizeof(idr),123,scratch,sizeof(scratch)));
    assert(emissions == 1 && s.primed);
    assert(!pcm4_aux_access_unit(&s,1,delta,sizeof(delta),123,scratch,sizeof(scratch)));
    assert(pcm4_aux_access_unit(&s,1,idr,5,123,scratch,sizeof(scratch)) == -1 && !s.primed);
    assert(pcm4_aux_access_unit(&s,1,delta,sizeof(delta),123,scratch,sizeof(scratch)) == 1);
    assert(pcm4_aux_access_unit(&s,2,idr,sizeof(idr),123,scratch,sizeof(scratch)) == -1);
    /* Deterministic malformed lengths/headers at every small input extent. */
    for (i = 1; i <= sizeof(bytes); ++i) {
        for (j = 0; j < i; ++j) bytes[j] = (unsigned char)(i*31+j*17);
        assert(pcm4_aux_access_unit(&s,1,bytes,i,123,scratch,sizeof(scratch)) == -1);
    }
    pcm4_aux_end(&s,1);
    assert(pcm4_aux_begin(&s,1,collect,NULL) == -1);
    assert(!pcm4_aux_begin(&s,2,collect,NULL));
    pcm4_aux_end(&s,1); assert(s.emit); /* stale end cannot kill new stream */
}
static void handover_tests(void)
{
    struct pcm4_handover h;
    struct hmi m = {0};
    struct pcm4_handover_ops ops = {take, visibility, restore, &m};
    assert(!pcm4_handover_init(&h,&ops,100));
    assert(!pcm4_handover_stock(&h,1,72,0));
    assert(!pcm4_handover_connected(&h,1,1));
    assert(!h.owned && !m.takes); /* connection alone must never take */
    assert(!pcm4_handover_frame(&h,2) && h.owned && m.takes == 1);
    assert(!pcm4_handover_stock(&h,1,76,3) && m.takes == 1);
    assert(!pcm4_handover_stock(&h,0,900,4) && h.stock_context == 76);
    assert(!pcm4_handover_stock(&h,1,12,5) && !h.owned && m.last == 12);
    assert(!pcm4_handover_stock(&h,1,73,6) && h.owned);
    assert(!pcm4_handover_tick(&h,102) && !h.owned && !h.ready);
    assert(!pcm4_handover_frame(&h,103) && h.owned);
    m.fail_restore = 1;
    assert(pcm4_handover_connected(&h,0,104) == -1 && h.owned && h.faulted);
    m.fail_restore = 0;
    assert(!pcm4_handover_tick(&h,105) && !h.owned);
    assert(!pcm4_handover_connected(&h,1,106));
    m.fail_show = 1;
    assert(pcm4_handover_frame(&h,107) == -1 && !h.owned && h.faulted);
    m.fail_show = 0;
    assert(!pcm4_handover_frame(&h,108) && !h.owned); /* error latched until reconnect */
    assert(!pcm4_handover_connected(&h,0,109));
    assert(!pcm4_handover_connected(&h,1,110));
    assert(!pcm4_handover_frame(&h,111) && h.owned);
    assert(!pcm4_handover_tick(&h,100) && !h.owned && h.faulted); /* clock regression */
    assert(!pcm4_handover_connected(&h,0,112));
    assert(!pcm4_handover_connected(&h,1,113)); m.fail_take = 1;
    assert(pcm4_handover_frame(&h,114) == -1 && !h.owned);
}
static int object, decodes, shown;
static int open_mock(void **handle, const struct pcm4_nv_open *p)
{ (void)p; *handle = &object; return 0; }
static int configure_mock(void *h, const struct pcm4_nv_config *p)
{ (void)p; return h == &object ? 0 : -1; }
static int get_mock(void *h, void *p)
{ (void)h; memset(p,0,64); return 0; }
static int set_mock(void *h, const void *attributes)
{ const unsigned char *p = attributes; (void)h; if (p[60]) shown = !p[61]; return 0; }
static int decode_mock(void *h, const struct pcm4_nv_input *p, int flags)
{ (void)h; (void)flags; assert(p->length >= 6); decodes++; return 0; }
static int close_mock(void *h) { (void)h; return 0; }
static void integration_tests(void)
{
    struct pcm4_nv_api api = {open_mock,configure_mock,get_mock,set_mock,decode_mock,close_mock};
    struct pcm4_display d = {0};
    struct pcm4_display_config c = {1,448,448,100,60,{448,448,30},{0,0,448,448}};
    struct pcm4_projection p = {0};
    struct hmi m = {0};
    assert(!pcm4_display_open(&d,&api,&c));
    assert(!pcm4_projection_init(&p,&d,take,restore,&m,100));
    assert(!pcm4_handover_stock(&p.handover,1,72,0));
    assert(!pcm4_projection_begin(&p,1,1));
    assert(!pcm4_projection_configure(&p,1,config,sizeof(config),2));
    assert(pcm4_projection_feed(&p,1,delta,sizeof(delta),123,3,scratch,sizeof(scratch)) == 1);
    assert(!shown && !decodes);
    assert(!pcm4_projection_feed(&p,1,idr,sizeof(idr),123,4,scratch,sizeof(scratch)));
    assert(shown && decodes == 1 && m.takes == 1);
    assert(!pcm4_handover_stock(&p.handover,1,77,5));
    assert(!pcm4_projection_end(&p,1,6) && !shown && m.last == 77);
    assert(!pcm4_projection_begin(&p,2,7));
    assert(pcm4_projection_feed(&p,1,idr,sizeof(idr),123,8,scratch,sizeof(scratch)) == -1);
    assert(p.handover.connected); /* stale frame does not stop new session */
    assert(!pcm4_projection_configure(&p,2,config,sizeof(config),9));
    assert(!pcm4_projection_feed(&p,2,idr,sizeof(idr),123,10,scratch,sizeof(scratch)) && shown);
    assert(pcm4_projection_feed(&p,2,idr,5,123,11,scratch,sizeof(scratch)) == -1);
    assert(!shown && !p.handover.connected && !p.stream.emit);
    assert(!pcm4_display_close(&d));
}
static unsigned char screen_body[512000];
static int decrypt_calls, decrypt_fails;
static int decrypt_mock(void *opaque, void *data, size_t size)
{
    size_t i;
    unsigned char *p = data;
    (void)opaque; decrypt_calls++;
    if (decrypt_fails) return -1;
    for (i=0;i<size;i++) p[i] ^= 0xaa;
    return 0;
}
static size_t screen_message(unsigned char *out, unsigned opcode, const unsigned char *body, size_t size)
{
    size_t i;
    memset(out,0,128); out[0]=(unsigned char)size; out[1]=(unsigned char)(size>>8);
    out[2]=(unsigned char)(size>>16); out[3]=(unsigned char)(size>>24); out[4]=(unsigned char)opcode;
    for (i=0;i<size;i++) out[128+i] = body[i] ^ (opcode == 0 ? 0xaa : 0);
    return 128+size;
}
static void screen_tests(void)
{
    struct pcm4_nv_api api = {open_mock,configure_mock,get_mock,set_mock,decode_mock,close_mock};
    struct pcm4_display d = {0};
    struct pcm4_display_config c = {1,448,448,100,60,{448,448,30},{0,0,448,448}};
    struct pcm4_projection p = {0};
    struct pcm4_screen_receiver r;
    struct hmi m = {0};
    unsigned char wire[1024];
    size_t length=0,i;
    uint64_t generation=1;
    decodes=shown=decrypt_calls=decrypt_fails=0;
    assert(!pcm4_display_open(&d,&api,&c));
    assert(!pcm4_projection_init(&p,&d,take,restore,&m,100));
    assert(!pcm4_handover_stock(&p.handover,1,72,0));
    assert(!pcm4_projection_begin(&p,generation,1));
    assert(!pcm4_screen_receiver_init(&r,&p,generation,screen_body,sizeof(screen_body),
                                    scratch,sizeof(scratch),decrypt_mock,NULL));
    /* Continuous crypto must advance even for a VIDEO_FRAME before avcC. */
    length += screen_message(wire+length,0,delta,sizeof(delta));
    length += screen_message(wire+length,2,NULL,0);
    length += screen_message(wire+length,1,config,sizeof(config));
    length += screen_message(wire+length,0,idr,sizeof(idr));
    for (i=0;i<length;i++) assert(!pcm4_screen_receiver_feed(&r,generation,wire+i,1,2));
    assert(r.messages==4 && r.accepted_frames==1 && decrypt_calls==2 && decodes==1 && shown);
    assert(pcm4_screen_receiver_feed(&r,99,wire,1,3)==-1 && shown);
    assert(!pcm4_screen_receiver_end(&r,generation,4) && !shown);
    /* Every incomplete header extent is rejected at EOF, without dispatch. */
    for (i=1;i<128;i++) {
        generation++;
        assert(!pcm4_projection_begin(&p,generation,5));
        assert(!pcm4_screen_receiver_init(&r,&p,generation,screen_body,sizeof(screen_body),
                                        scratch,sizeof(scratch),decrypt_mock,NULL));
        assert(!pcm4_screen_receiver_feed(&r,generation,wire,i,5));
        assert(pcm4_screen_receiver_end(&r,generation,5)==-1 && !p.handover.connected);
    }
    generation++;
    assert(!pcm4_projection_begin(&p,generation,6));
    assert(!pcm4_screen_receiver_init(&r,&p,generation,screen_body,sizeof(screen_body),
                                    scratch,sizeof(scratch),decrypt_mock,NULL));
    length=screen_message(wire,1,config,sizeof(config));
    length+=screen_message(wire+length,0,idr,sizeof(idr));
    assert(!pcm4_screen_receiver_feed(&r,generation,wire,length,7) && shown); /* coalesced messages */
    decrypt_fails=1;
    length=screen_message(wire,0,idr,sizeof(idr));
    assert(pcm4_screen_receiver_feed(&r,generation,wire,length,8)==-1 && !shown && r.failed);
    decrypt_fails=0;
    generation++;
    assert(!pcm4_projection_begin(&p,generation,9));
    assert(!pcm4_screen_receiver_init(&r,&p,generation,screen_body,sizeof(screen_body),
                                    scratch,sizeof(scratch),decrypt_mock,NULL));
    memset(wire,0,128); memset(wire,255,4);
    assert(pcm4_screen_receiver_feed(&r,generation,wire,128,10)==-1 && r.failed);
    assert(!pcm4_display_close(&d));
}
int main(void)
{
    stream_tests(); handover_tests(); integration_tests(); screen_tests();
    puts("PASS: AVCC/IDR/session isolation; handover recovery; fragmented/coalesced Screen crypto/decode/show/restore");
    return 0;
}
