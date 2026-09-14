#include "meleemod/api.h"
#include "meleemod/input.h"
static uint32_t token;
static void on_frame(const mm_event *event, void *user) { (void)user; if (event && mm_input_history_get(0) && (mm_input_history_get(0)->buttons != 0)) mm_log(MM_LOG_DEBUG, "input-display: button activity"); }
int plugin_init(const mm_plugin_context *context) { if (!context || context->api_version != MM_PLUGIN_ABI_VERSION) return -1; return mm_subscribe(MM_EVENT_FRAME, on_frame, 0, &token); }
void plugin_shutdown(void) { if (token) mm_unsubscribe(token); token=0; }
