#include <assert.h>
#include "pascalpatch/input.h"
int main(void){ mm_input_sample s={0,1,2,3,4,5,6}; mm_input_history_reset(); assert(mm_input_history_count()==0); for(unsigned i=0;i<MM_INPUT_HISTORY_CAPACITY+3;i++){s.frame=i; assert(mm_input_history_push(&s)==0);} assert(mm_input_history_count()==MM_INPUT_HISTORY_CAPACITY); assert(mm_input_history_get(0)->frame==MM_INPUT_HISTORY_CAPACITY+2); assert(mm_input_history_get(MM_INPUT_HISTORY_CAPACITY-1)->frame==3); assert(mm_input_history_get(MM_INPUT_HISTORY_CAPACITY)==0); return 0;}
