#include "meleemod/bridge.h"
#include <string.h>

typedef struct { uint32_t h[8]; uint32_t bits; uint32_t used; unsigned char data[64]; } mm_sha256;
static uint32_t mm_rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32u - n)); }
static uint32_t mm_ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static uint32_t mm_maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t mm_s0(uint32_t x) { return mm_rotr(x,2) ^ mm_rotr(x,13) ^ mm_rotr(x,22); }
static uint32_t mm_s1(uint32_t x) { return mm_rotr(x,6) ^ mm_rotr(x,11) ^ mm_rotr(x,25); }
static uint32_t mm_g0(uint32_t x) { return mm_rotr(x,7) ^ mm_rotr(x,18) ^ (x >> 3); }
static uint32_t mm_g1(uint32_t x) { return mm_rotr(x,17) ^ mm_rotr(x,19) ^ (x >> 10); }
static const uint32_t mm_k[64] = {
    0x428a2f98UL,0x71374491UL,0xb5c0fbcfUL,0xe9b5dba5UL,0x3956c25bUL,0x59f111f1UL,0x923f82a4UL,0xab1c5ed5UL,
    0xd807aa98UL,0x12835b01UL,0x243185beUL,0x550c7dc3UL,0x72be5d74UL,0x80deb1feUL,0x9bdc06a7UL,0xc19bf174UL,
    0xe49b69c1UL,0xefbe4786UL,0x0fc19dc6UL,0x240ca1ccUL,0x2de92c6fUL,0x4a7484aaUL,0x5cb0a9dcUL,0x76f988daUL,
    0x983e5152UL,0xa831c66dUL,0xb00327c8UL,0xbf597fc7UL,0xc6e00bf3UL,0xd5a79147UL,0x06ca6351UL,0x14292967UL,
    0x27b70a85UL,0x2e1b2138UL,0x4d2c6dfcUL,0x53380d13UL,0x650a7354UL,0x766a0abbUL,0x81c2c92eUL,0x92722c85UL,
    0xa2bfe8a1UL,0xa81a664bUL,0xc24b8b70UL,0xc76c51a3UL,0xd192e819UL,0xd6990624UL,0xf40e3585UL,0x106aa070UL,
    0x19a4c116UL,0x1e376c08UL,0x2748774cUL,0x34b0bcb5UL,0x391c0cb3UL,0x4ed8aa4aUL,0x5b9cca4fUL,0x682e6ff3UL,
    0x748f82eeUL,0x78a5636fUL,0x84c87814UL,0x8cc70208UL,0x90befffaUL,0xa4506cebUL,0xbef9a3f7UL,0xc67178f2UL
};
static void mm_sha_block(mm_sha256 *s, const unsigned char *data) {
    uint32_t w[64], a,b,c,d,e,f,g,h,t1,t2; unsigned i;
    for (i=0; i<16; i++) w[i]=((uint32_t)data[i*4]<<24)|((uint32_t)data[i*4+1]<<16)|((uint32_t)data[i*4+2]<<8)|data[i*4+3];
    for (i=16; i<64; i++) w[i]=mm_g1(w[i-2])+w[i-7]+mm_g0(w[i-15])+w[i-16];
    a=s->h[0]; b=s->h[1]; c=s->h[2]; d=s->h[3]; e=s->h[4]; f=s->h[5]; g=s->h[6]; h=s->h[7];
    for (i=0; i<64; i++) { t1=h+mm_s1(e)+mm_ch(e,f,g)+mm_k[i]+w[i]; t2=mm_s0(a)+mm_maj(a,b,c); h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2; }
    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d; s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}
static void mm_sha_init(mm_sha256 *s) { static const uint32_t initial[8]={0x6a09e667UL,0xbb67ae85UL,0x3c6ef372UL,0xa54ff53aUL,0x510e527fUL,0x9b05688cUL,0x1f83d9abUL,0x5be0cd19UL}; unsigned i; for(i=0;i<8;i++)s->h[i]=initial[i]; s->bits=0; s->used=0; }
static void mm_sha_update(mm_sha256 *s, const unsigned char *data, uint32_t len) { uint32_t i; for(i=0;i<len;i++){s->data[s->used++]=data[i]; if(s->used==64){mm_sha_block(s,s->data);s->bits+=512;s->used=0;}} }
static void mm_sha_final(mm_sha256 *s, unsigned char out[32]) { unsigned i; uint32_t bits; s->bits += s->used*8; s->data[s->used++]=0x80; while(s->used!=56){if(s->used==64){mm_sha_block(s,s->data);s->used=0;}s->data[s->used++]=0;} bits=s->bits; s->data[56]=0;s->data[57]=0;s->data[58]=0;s->data[59]=0; s->data[60]=(unsigned char)(bits>>24);s->data[61]=(unsigned char)(bits>>16);s->data[62]=(unsigned char)(bits>>8);s->data[63]=(unsigned char)bits; mm_sha_block(s,s->data); for(i=0;i<8;i++){out[i*4]=(unsigned char)(s->h[i]>>24);out[i*4+1]=(unsigned char)(s->h[i]>>16);out[i*4+2]=(unsigned char)(s->h[i]>>8);out[i*4+3]=(unsigned char)s->h[i];} }
static void mm_put32(unsigned char *p, uint32_t x) { p[0]=(unsigned char)(x>>24); p[1]=(unsigned char)(x>>16); p[2]=(unsigned char)(x>>8); p[3]=(unsigned char)x; }
static uint32_t mm_get32(const unsigned char *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static void mm_checksum(const unsigned char *payload, uint32_t size, unsigned char out[16]) { mm_sha256 sha; unsigned char full[32]; unsigned i; mm_sha_init(&sha); mm_sha_update(&sha,payload,size); mm_sha_final(&sha,full); for(i=0;i<16;i++)out[i]=full[i]; }
static int mm_frame_write(mm_bridge_endpoint *e, uint8_t kind, uint32_t request, const unsigned char *payload, uint32_t size) { unsigned char checksum[16]; if(!e || !e->io.write || size>MM_BRIDGE_MAX_PAYLOAD)return -1; e->frame[0]='M';e->frame[1]='M';e->frame[2]='B';e->frame[3]='1';e->frame[4]=MM_BRIDGE_VERSION;e->frame[5]=kind;mm_put32(e->frame+6,request);mm_put32(e->frame+10,size);mm_checksum(payload,size,checksum); memcpy(e->frame+14,checksum,16); if(size)memcpy(e->frame+30,payload,size); if(e->io.write(e->io.user,e->frame,30+size)!=MM_BRIDGE_IO_OK)return -1; return 0; }
static void mm_disconnect_notify(mm_bridge_endpoint *e) { mm_bridge_disconnect_fn fn; void *user; if(!e || !e->connected)return; fn=e->io.disconnected;user=e->io.user;e->connected=0;e->negotiated=0;if(fn)fn(user); }
int mm_bridge_attach(mm_bridge_endpoint *e, const mm_bridge_io *io) { if(!e || !io || !io->read || !io->write){return -1;} memset(e,0,sizeof(*e));e->io=*io;e->next_request=1;return 0; }
int mm_bridge_connect(mm_bridge_endpoint *e) { unsigned char payload[4]; if(!e || !e->io.write || e->connected)return -1; payload[0]=0;payload[1]=0;payload[2]=0;payload[3]=(unsigned char)MM_BRIDGE_VERSION;e->connected=1;e->negotiated=0;if(mm_frame_write(e,MM_BRIDGE_KIND_HELLO,e->next_request++,payload,4)!=0){mm_disconnect_notify(e);return -1;}return 0; }
int mm_bridge_send(mm_bridge_endpoint *e, uint32_t request, const unsigned char *payload, uint32_t size) { if(!e || !e->connected || !e->negotiated || (size && !payload))return -1; if(mm_frame_write(e,MM_BRIDGE_KIND_DATA,request,payload,size)!=0){mm_disconnect_notify(e);return -1;}return 0; }
static int mm_frame_read(mm_bridge_endpoint *e, uint8_t *kind, uint32_t *request, const unsigned char **payload, uint32_t *size) { unsigned char expected[16]; uint32_t n; if(e->frame[0]!='M'||e->frame[1]!='M'||e->frame[2]!='B'||e->frame[3]!='1'||e->frame[4]!=MM_BRIDGE_VERSION)return -1; n=mm_get32(e->frame+10);if(n>MM_BRIDGE_MAX_PAYLOAD||n+30>sizeof(e->frame))return -1;mm_checksum(e->frame+30,n,expected);if(memcmp(expected,e->frame+14,16)!=0)return -1;*kind=e->frame[5];*request=mm_get32(e->frame+6);*size=n;*payload=e->frame+30;return 0; }
int mm_bridge_poll(mm_bridge_endpoint *e, uint32_t frame) { int rc; uint32_t size,request; uint8_t kind; const unsigned char *payload; unsigned char version[4]; if(!e || !e->connected)return 0; rc=e->io.read(e->io.user,e->frame,sizeof(e->frame),&size);if(rc==MM_BRIDGE_IO_NO_DATA)return 0;if(rc!=MM_BRIDGE_IO_OK||size<30||size>sizeof(e->frame)){mm_disconnect_notify(e);return -1;}if(mm_get32(e->frame+10)+30!=size||mm_frame_read(e,&kind,&request,&payload,&size)!=0){mm_disconnect_notify(e);return -2;}if(kind==MM_BRIDGE_KIND_HELLO){if(size<4||mm_get32(payload)!=MM_BRIDGE_VERSION){mm_disconnect_notify(e);return -3;}version[0]=0;version[1]=0;version[2]=0;version[3]=(unsigned char)MM_BRIDGE_VERSION;if(mm_frame_write(e,MM_BRIDGE_KIND_HELLO_ACK,request,version,4)!=0){mm_disconnect_notify(e);return -1;}e->negotiated=1;return 1;}if(kind==MM_BRIDGE_KIND_HELLO_ACK){if(size<4||mm_get32(payload)!=MM_BRIDGE_VERSION){mm_disconnect_notify(e);return -3;}e->negotiated=1;return 1;}if(kind==MM_BRIDGE_KIND_HEARTBEAT){if(mm_frame_write(e,MM_BRIDGE_KIND_HEARTBEAT_ACK,request,0,0)!=0){mm_disconnect_notify(e);return -1;}return 1;}if(kind==MM_BRIDGE_KIND_HEARTBEAT_ACK)return 1;if(kind==MM_BRIDGE_KIND_DISCONNECT){mm_disconnect_notify(e);return 1;}if(kind==MM_BRIDGE_KIND_DATA){if(!e->negotiated){mm_disconnect_notify(e);return -3;}if(e->io.message)e->io.message(kind,request,payload,size,e->io.user);return 1;}mm_disconnect_notify(e);(void)frame;return -3; }
int mm_bridge_tick(mm_bridge_endpoint *e, uint32_t frame) { if(!e || !e->connected || !e->negotiated)return 0;if((uint32_t)(frame-e->last_heartbeat)<MM_BRIDGE_HEARTBEAT_FRAMES)return 0;if(mm_frame_write(e,MM_BRIDGE_KIND_HEARTBEAT,e->next_request++,0,0)!=0){mm_disconnect_notify(e);return -1;}e->last_heartbeat=frame;return 1; }
void mm_bridge_disconnect(mm_bridge_endpoint *e) { if(!e)return;mm_disconnect_notify(e);memset(e,0,sizeof(*e)); }


volatile mm_bridge_mailbox_state mm_bridge_mailbox = {
    {'M','M','B','X'}, MM_BRIDGE_VERSION, 0, 0, {0}, {0}
};

volatile mm_bridge_mailbox_state *mm_bridge_mailbox_get(void) { return &mm_bridge_mailbox; }
uint32_t mm_bridge_mailbox_address(void) { return (uint32_t)(unsigned long)&mm_bridge_mailbox; }
void mm_bridge_mailbox_reset(void) {
    mm_bridge_mailbox.magic[0]='M'; mm_bridge_mailbox.magic[1]='M'; mm_bridge_mailbox.magic[2]='B'; mm_bridge_mailbox.magic[3]='X';
    mm_bridge_mailbox.version=MM_BRIDGE_VERSION; mm_bridge_mailbox.host_size=0; mm_bridge_mailbox.game_size=0;
}
static int mm_mailbox_read(void *user, unsigned char *buffer, uint32_t capacity, uint32_t *size) {
    volatile mm_bridge_mailbox_state *state=(volatile mm_bridge_mailbox_state *)user; uint32_t n;
    if (!state || !buffer || !size) return MM_BRIDGE_IO_ERROR;
    n=state->host_size; if (!n) return MM_BRIDGE_IO_NO_DATA; if (n>capacity || n>MM_BRIDGE_MAILBOX_CAPACITY) return MM_BRIDGE_IO_ERROR;
    memcpy(buffer,(const void *)state->host_payload,n); state->host_size=0; *size=n; return MM_BRIDGE_IO_OK;
}
static int mm_mailbox_write(void *user, const unsigned char *buffer, uint32_t size) {
    volatile mm_bridge_mailbox_state *state=(volatile mm_bridge_mailbox_state *)user;
    if (!state || (size && !buffer) || size>MM_BRIDGE_MAILBOX_CAPACITY || state->game_size) return MM_BRIDGE_IO_ERROR;
    if (size) {
        memcpy((void *)state->game_payload,buffer,size);
    }
    state->game_size=size;
    return MM_BRIDGE_IO_OK;
}
void mm_bridge_mailbox_make_io(mm_bridge_io *io) {
    if (!io) return;
    memset(io,0,sizeof(*io));
    io->read=mm_mailbox_read; io->write=mm_mailbox_write; io->user=(void *)&mm_bridge_mailbox;
}
