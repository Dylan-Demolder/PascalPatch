#ifndef PASCALPATCH_API_H
#define PASCALPATCH_API_H
#include "pascalpatch/runtime.h"
#define MM_PLUGIN_ABI_VERSION 1u
typedef struct { uint32_t api_version; const char *id; } mm_plugin_context;
typedef int (*mm_plugin_init_fn)(const mm_plugin_context *context);
typedef void (*mm_plugin_shutdown_fn)(void);
#endif
