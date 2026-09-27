/* Compatibility: MeleeMod is now PascalPatch.
 *
 * New plugins include "pascalpatch/plugin.h" and export pp_plugin_load. This
 * header keeps plugins written against the MeleeMod names building unchanged:
 * the table is the same one, and the runtime still loads a DLL that exports
 * mm_native_load.
 */
#ifndef MELEEMOD_NATIVE_H
#define MELEEMOD_NATIVE_H
#include "../pascalpatch/plugin.h"

#define MM_NATIVE_ABI PP_PLUGIN_ABI
typedef pp_cpu mm_cpu;
typedef pp_guest_fn mm_guest_fn;
typedef pp_frame_fn mm_frame_fn;
typedef pp_host mm_host;
typedef pp_plugin_load_fn mm_native_load_fn;
#define MM_NATIVE_LOAD_SYMBOL "mm_native_load"

#endif
