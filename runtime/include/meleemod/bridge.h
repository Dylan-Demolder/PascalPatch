#ifndef MELEEMOD_BRIDGE_H
#define MELEEMOD_BRIDGE_H
#if defined(__MWERKS__)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
#else
#include <stdint.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define MM_BRIDGE_VERSION 1u
#define MM_BRIDGE_MAX_PAYLOAD 1024u
#define MM_BRIDGE_HEADER_SIZE 30u
#define MM_BRIDGE_HEARTBEAT_FRAMES 60u
#define MM_BRIDGE_KIND_HELLO 1u
#define MM_BRIDGE_KIND_HELLO_ACK 2u
#define MM_BRIDGE_KIND_HEARTBEAT 3u
#define MM_BRIDGE_KIND_HEARTBEAT_ACK 4u
#define MM_BRIDGE_KIND_DATA 5u
#define MM_BRIDGE_KIND_DISCONNECT 6u
#define MM_BRIDGE_IO_NO_DATA 0
#define MM_BRIDGE_IO_OK 1
#define MM_BRIDGE_IO_DISCONNECTED -1
#define MM_BRIDGE_IO_ERROR -2
typedef int (*mm_bridge_read_fn)(void *user, unsigned char *buffer, uint32_t capacity, uint32_t *size);
typedef int (*mm_bridge_write_fn)(void *user, const unsigned char *buffer, uint32_t size);
typedef void (*mm_bridge_message_fn)(uint8_t kind, uint32_t request_id, const unsigned char *payload, uint32_t size, void *user);
typedef void (*mm_bridge_disconnect_fn)(void *user);
typedef struct { mm_bridge_read_fn read; mm_bridge_write_fn write; mm_bridge_message_fn message; mm_bridge_disconnect_fn disconnected; void *user; } mm_bridge_io;
typedef struct { mm_bridge_io io; uint32_t next_request; uint32_t last_heartbeat; uint8_t connected; uint8_t negotiated; unsigned char frame[MM_BRIDGE_HEADER_SIZE + MM_BRIDGE_MAX_PAYLOAD]; } mm_bridge_endpoint;
int mm_bridge_attach(mm_bridge_endpoint *endpoint, const mm_bridge_io *io);
int mm_bridge_connect(mm_bridge_endpoint *endpoint);
int mm_bridge_poll(mm_bridge_endpoint *endpoint, uint32_t frame);
int mm_bridge_tick(mm_bridge_endpoint *endpoint, uint32_t frame);
int mm_bridge_send(mm_bridge_endpoint *endpoint, uint32_t request_id, const unsigned char *payload, uint32_t size);
void mm_bridge_disconnect(mm_bridge_endpoint *endpoint);
#ifdef __cplusplus
}
#endif
#endif
