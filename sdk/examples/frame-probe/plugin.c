#include "meleemod/api.h"
#include "meleemod/input.h"
static uint32_t token;
static void on_frame(const mm_event *event, void *user) {
    const mm_input_sample *sample;
    (void)user;
    sample = mm_input_history_get(0);
    if (event && sample && event->frame == sample->frame) mm_log(MM_LOG_INFO, "frame-probe frame/input observed");
}
int plugin_init(const mm_plugin_context *context) {
    if (!context || context->api_version != MM_PLUGIN_ABI_VERSION) return -1;
    if (mm_subscribe(MM_EVENT_FRAME, on_frame, 0, &token) != 0) return -1;
    return 0;
}
void plugin_shutdown(void) { if (token) mm_unsubscribe(token); token=0; }
