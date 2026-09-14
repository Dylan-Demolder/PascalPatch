#ifndef MELEEMOD_EVENTS_H
#define MELEEMOD_EVENTS_H
#if defined(__MWERKS__)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed short int16_t;
#else
#include <stdint.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define MM_EVENT_QUEUE_CAPACITY 32u
typedef enum { MM_EVENT_FRAME=1, MM_EVENT_MATCH_START=2, MM_EVENT_MATCH_END=3 } mm_event_type;
typedef struct { mm_event_type type; uint32_t frame; const void *payload; uint32_t payload_size; } mm_event;
typedef void (*mm_event_callback)(const mm_event *event, void *user);
typedef struct { uint32_t api_version; uint32_t event_count; } mm_game_context;
int mm_subscribe(mm_event_type type, mm_event_callback callback, void *user, uint32_t *token);
int mm_unsubscribe(uint32_t token);
int mm_dispatch(const mm_event *event);
void mm_events_reset(void);
uint32_t mm_dropped_events(void);
#ifdef __cplusplus
}
#endif
#endif
