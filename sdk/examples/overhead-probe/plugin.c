#include "pascalpatch/api.h"
#include "pascalpatch/input.h"
extern long long OSGetTime(void);
extern void OSReport(char *, ...);
static uint32_t token;
static uint32_t frames;
static unsigned long long last_time;
static void on_frame(const mm_event *event, void *user) {
    unsigned long long start,end; (void)user; if (!event) return;
    start=OSGetTime(); (void)mm_input_history_get(0); end=OSGetTime();
    if ((frames++ % 60u)==0u) {
        if (last_time) OSReport("[pascalpatch] overhead-probe: 60-frame interval ticks %llu input-read ticks %llu\n",start-last_time,end-start);
        last_time=start;
    }
}
int plugin_init(const mm_plugin_context *context) { if (!context || context->api_version != MM_PLUGIN_ABI_VERSION) return -1; return mm_subscribe(MM_EVENT_FRAME,on_frame,0,&token); }
void plugin_shutdown(void) { OSReport("[pascalpatch] overhead-probe: shutdown hook\n"); if (token) mm_unsubscribe(token); token=0; frames=0; last_time=0; }
