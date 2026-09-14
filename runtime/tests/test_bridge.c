#include "meleemod/bridge.h"
#include <assert.h>
#include <string.h>
static unsigned char rx[2048], tx[2048];
static uint32_t rx_size, tx_size, seen_size, seen_request;
static int disconnected, seen;
static int read_frame(void *user, unsigned char *buffer, uint32_t capacity, uint32_t *size) { (void)user; if (!rx_size) return MM_BRIDGE_IO_NO_DATA; assert(rx_size <= capacity); memcpy(buffer, rx, rx_size); *size = rx_size; rx_size = 0; return MM_BRIDGE_IO_OK; }
static int write_frame(void *user, const unsigned char *buffer, uint32_t size) { (void)user; assert(size <= sizeof(tx)); memcpy(tx, buffer, size); tx_size = size; return MM_BRIDGE_IO_OK; }
static void on_message(uint8_t kind, uint32_t request, const unsigned char *payload, uint32_t size, void *user) { (void)user; assert(kind == MM_BRIDGE_KIND_DATA); assert(size == 3); assert(memcmp(payload, "abc", 3) == 0); seen = 1; seen_request = request; seen_size = size; }
static void on_disconnect(void *user) { (void)user; disconnected++; }
static void put32(unsigned char *p, uint32_t x) { p[0]=(unsigned char)(x>>24); p[1]=(unsigned char)(x>>16); p[2]=(unsigned char)(x>>8); p[3]=(unsigned char)x; }
static void make_frame(unsigned char *p, unsigned char kind, uint32_t request, const unsigned char *payload, uint32_t size, const unsigned char *checksum) { memcpy(p, "MMB1", 4); p[4]=1; p[5]=kind; put32(p+6, request); put32(p+10, size); memcpy(p+14, checksum, 16); if (size) memcpy(p+30, payload, size); }
int main(void) {
    mm_bridge_endpoint endpoint;
    mm_bridge_io io={read_frame,write_frame,on_message,on_disconnect,0};
    static const unsigned char hello_sum[16]={0xb4,0x07,0x11,0xa8,0x8c,0x70,0x39,0x75,0x6f,0xb8,0xa7,0x38,0x27,0xea,0xbe,0x2c};
    static const unsigned char empty_sum[16]={0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24};
    static const unsigned char abc_sum[16]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23};
    static const unsigned char version[4]={0,0,0,1};
    static const unsigned char abc[3]={'a','b','c'};
    assert(mm_bridge_attach(&endpoint,&io)==0);
    assert(mm_bridge_connect(&endpoint)==0);
    assert(tx_size==34 && memcmp(tx+14,hello_sum,16)==0);
    make_frame(rx,MM_BRIDGE_KIND_HELLO_ACK,1,version,4,hello_sum); rx_size=34;
    assert(mm_bridge_poll(&endpoint,0)==1);
    make_frame(rx,MM_BRIDGE_KIND_DATA,42,abc,3,abc_sum); rx_size=33;
    assert(mm_bridge_poll(&endpoint,1)==1);
    assert(seen && seen_request==42 && seen_size==3);
    assert(mm_bridge_tick(&endpoint,60)==1);
    assert(tx_size==30 && memcmp(tx+14,empty_sum,16)==0);
    make_frame(rx,MM_BRIDGE_KIND_DATA,43,abc,3,abc_sum); rx[14]^=1; rx_size=33;
    assert(mm_bridge_poll(&endpoint,61)<0 && disconnected==1);
    mm_bridge_disconnect(&endpoint);
    return 0;
}
