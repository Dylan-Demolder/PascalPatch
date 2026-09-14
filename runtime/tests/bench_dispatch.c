#include "meleemod/events.h"
#include "meleemod/runtime.h"
#include <stdio.h>
#include <time.h>
static void callback(const mm_event *event, void *user) { (void)event; *(volatile unsigned*)user += 1; }
static double seconds(void) { return (double)clock() / (double)CLOCKS_PER_SEC; }
int main(void) {
    const unsigned n=1000000; unsigned seen=0; uint32_t token=0; mm_event event={MM_EVENT_FRAME,1,0,0};
    mm_runtime_init(); double start=seconds(); for(unsigned i=0;i<n;i++) mm_dispatch(&event); double empty=seconds()-start;
    mm_subscribe(MM_EVENT_FRAME,callback,&seen,&token); start=seconds(); for(unsigned i=0;i<n;i++) mm_dispatch(&event); double one=seconds()-start;
    printf("dispatch_empty_ns=%.2f dispatch_one_subscriber_ns=%.2f callbacks=%u\n",empty*1e9/n,one*1e9/n,seen);
    return (seen==n)?0:1;
}
