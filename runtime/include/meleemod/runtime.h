#ifndef MELEEMOD_RUNTIME_H
#define MELEEMOD_RUNTIME_H
#include <stdint.h>
#include "events.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MM_RUNTIME_API_VERSION 1u
typedef enum { MM_LOG_DEBUG=0, MM_LOG_INFO=1, MM_LOG_WARN=2, MM_LOG_ERROR=3 } mm_log_level;
uint32_t mm_api_version(void);
void mm_log(mm_log_level level, const char *message);
int mm_runtime_init(void);
int mm_runtime_shutdown(void);
const mm_game_context *mm_game_context_get(void);
#ifdef __cplusplus
}
#endif
#endif
