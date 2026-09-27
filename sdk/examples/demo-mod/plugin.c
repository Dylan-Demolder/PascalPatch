#include "pascalpatch/api.h"
#include "pascalpatch/input.h"
static uint32_t token;
static uint32_t frames;
static void on_frame(const mm_event *event, void *user) {
    (void)user;
    if (!event) return;
    if (!frames++) mm_log(MM_LOG_INFO, "demo-mod: first frame callback");
    if (mm_input_history_get(0) && (mm_input_history_get(0)->buttons != 0)) mm_log(MM_LOG_DEBUG, "demo-mod: controller activity");
}
int plugin_init(const mm_plugin_context *context) {
    if (!context || context->api_version != MM_PLUGIN_ABI_VERSION) return -1;
    mm_log(MM_LOG_INFO, "demo-mod: initialized");
    return mm_subscribe(MM_EVENT_FRAME, on_frame, 0, &token);
}
void plugin_shutdown(void) {
    if (token) mm_unsubscribe(token);
    token=0; frames=0;
}
