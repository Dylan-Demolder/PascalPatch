#include "meleemod/api.h"
#include "meleemod/input.h"
static uint32_t frame_token;
static void on_frame(const mm_event *event, void *user) { (void)user; if (event && (event->frame % 60u)==0u) mm_log(MM_LOG_DEBUG, "training-tools: frame heartbeat"); }
int plugin_init(const mm_plugin_context *context) { if (!context || context->api_version != MM_PLUGIN_ABI_VERSION) return -1; return mm_subscribe(MM_EVENT_FRAME, on_frame, 0, &frame_token); }
void plugin_shutdown(void) { if (frame_token) mm_unsubscribe(frame_token); frame_token=0; }
