#ifndef MELEEMOD_INPUT_H
#define MELEEMOD_INPUT_H
#if defined(__MWERKS__)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed short int16_t;
#else
#include <stdint.h>
#endif
#define MM_INPUT_HISTORY_CAPACITY 120u
typedef struct { uint32_t frame; uint8_t port; uint16_t buttons; int16_t stick_x; int16_t stick_y; uint8_t trigger_l; uint8_t trigger_r; } mm_input_sample;
void mm_input_history_reset(void);
int mm_input_history_push(const mm_input_sample *sample);
uint32_t mm_input_history_count(void);
/* index 0 is newest; samples are read-only and invalid indices return 0. */
const mm_input_sample *mm_input_history_get(uint32_t newest_index);
#endif
