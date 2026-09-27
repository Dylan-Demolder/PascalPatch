#include "pascalpatch/input.h"
#include <string.h>
static mm_input_sample history[MM_INPUT_HISTORY_CAPACITY];
static uint32_t head=0;
static uint32_t count=0;
void mm_input_history_reset(void){ memset(history,0,sizeof(history)); head=0; count=0; }
int mm_input_history_push(const mm_input_sample *sample){ if(!sample)return -1; head=(head+MM_INPUT_HISTORY_CAPACITY-1)%MM_INPUT_HISTORY_CAPACITY; history[head]=*sample; if(count<MM_INPUT_HISTORY_CAPACITY)count++; return 0; }
uint32_t mm_input_history_count(void){return count;}
const mm_input_sample *mm_input_history_get(uint32_t newest_index){ if(newest_index>=count)return 0; return &history[(head+newest_index)%MM_INPUT_HISTORY_CAPACITY]; }
