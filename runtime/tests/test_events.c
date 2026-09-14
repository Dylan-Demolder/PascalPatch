#include <assert.h>
#include "meleemod/runtime.h"
static int seen; static void cb(const mm_event *e,void *u){(void)e; seen += *(int*)u;}
int main(void){int one=1; uint32_t t=0; mm_runtime_init(); assert(mm_subscribe(MM_EVENT_FRAME,cb,&one,&t)==0); mm_event e={MM_EVENT_FRAME,1,0,0}; assert(mm_dispatch(&e)==1 && seen==1); assert(mm_unsubscribe(t)==0); assert(mm_dispatch(&e)==0); assert(mm_runtime_shutdown()==0); return 0;}
