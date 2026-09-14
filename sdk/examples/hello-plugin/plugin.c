#include "meleemod/api.h"
int plugin_init(const mm_plugin_context *context) { if (!context || context->api_version != MM_PLUGIN_ABI_VERSION) return -1; mm_log(MM_LOG_INFO, "hello-plugin initialized"); return 0; }
void plugin_shutdown(void) { mm_log(MM_LOG_INFO, "hello-plugin shut down"); }
