#include "meleemod/api.h"
static uint32_t token;
static void on_ready(const mm_event *event, void *user) { (void)user; if (event && event->type == MM_EVENT_RUNTIME_READY) mm_log(MM_LOG_INFO, "startup-probe runtime-ready observed"); }
int plugin_init(const mm_plugin_context *context) { if (!context || context->api_version != MM_PLUGIN_ABI_VERSION) return -1; if (mm_subscribe(MM_EVENT_RUNTIME_READY, on_ready, 0, &token) != 0) return -1; mm_log(MM_LOG_INFO, "startup-probe initialized"); return 0; }
void plugin_shutdown(void) { if (token) mm_unsubscribe(token); token=0; }
