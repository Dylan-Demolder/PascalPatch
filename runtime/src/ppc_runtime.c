#include "pascalpatch/runtime.h"
extern void OSReport(char *, ...);
static mm_game_context context={MM_RUNTIME_API_VERSION,0};
static int active=0;
uint32_t mm_api_version(void){return MM_RUNTIME_API_VERSION;}
void mm_log(mm_log_level level,const char *message){(void)level; if(message) OSReport("[pascalpatch] %s\n",message);}
int mm_runtime_init(void){if(active)return 0; mm_events_reset(); context.event_count=0; active=1; return 0;}
int mm_runtime_shutdown(void){if(!active)return 0; mm_events_reset(); active=0; return 0;}
const mm_game_context *mm_game_context_get(void){return &context;}
