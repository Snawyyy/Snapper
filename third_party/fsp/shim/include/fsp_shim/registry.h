// The plugins Snapper carries: each one's folder name and entry points.
// The list itself is in registry.cpp, one line per plugin.

#ifndef FSP_SHIM_REGISTRY_H_
#define FSP_SHIM_REGISTRY_H_

#include "fsp_shim/ae_callbacks.h"

struct FspPluginEntry {
  const char* id;
  PF_EffectMainFunc effect_main;
  PF_PluginDataEntryFunc2 data_entry;
};

// Every plugin in the list, in order; *count gets their number.
const FspPluginEntry* FspPluginEntries(int* count);

#endif  // FSP_SHIM_REGISTRY_H_
