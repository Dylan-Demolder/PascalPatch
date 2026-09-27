#include "pascalpatch/events.h"
#include <string.h>
typedef struct { unsigned char used; mm_event_type type; mm_event_callback callback; void *user; uint32_t token; } mm_slot;
static mm_slot slots[MM_EVENT_QUEUE_CAPACITY];
static uint32_t next_token=1;
static uint32_t dropped=0;
int mm_subscribe(mm_event_type type, mm_event_callback cb, void *user, uint32_t *token) {
    unsigned i;
    if (!cb || !token || type == 0) return -1;
    for (i=0; i<MM_EVENT_QUEUE_CAPACITY; i++) {
        if (!slots[i].used) {
            slots[i].used=1; slots[i].type=type; slots[i].callback=cb; slots[i].user=user;
            slots[i].token=next_token++; if (!slots[i].token) slots[i].token=next_token++;
            *token=slots[i].token; return 0;
        }
    }
    dropped++; return -2;
}
int mm_unsubscribe(uint32_t token) {
    unsigned i;
    if (!token) return -1;
    for (i=0; i<MM_EVENT_QUEUE_CAPACITY; i++) if (slots[i].used && slots[i].token==token) { memset(&slots[i],0,sizeof(slots[i])); return 0; }
    return -2;
}
int mm_dispatch(const mm_event *event) {
    unsigned i;
    int count=0;
    if (!event || !event->type) return -1;
    for (i=0; i<MM_EVENT_QUEUE_CAPACITY; i++) if (slots[i].used && slots[i].type==event->type) { slots[i].callback(event,slots[i].user); count++; }
    return count;
}
void mm_events_reset(void) { memset(slots,0,sizeof(slots)); next_token=1; dropped=0; }
uint32_t mm_dropped_events(void) { return dropped; }
