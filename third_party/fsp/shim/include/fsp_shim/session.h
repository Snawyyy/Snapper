// One call into a plugin as the stand-in host sees it: the effect_ref a
// plugin hands back with every callback. It owns every picture made for
// the call and, while the plugin declares its parameters, records them.

#ifndef FSP_SHIM_SESSION_H_
#define FSP_SHIM_SESSION_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "fsp_shim/ae_callbacks.h"

// A parameter as declared in PARAMS_SETUP, with its strings copied out
// of the plugin (popup entries, checkbox and button labels).
struct FspShimRecordedParam {
  PF_ParamDef def;
  std::string items;
};

struct FspShimEffectRef {
  // Index 0 is the input layer, as in After Effects.
  std::vector<PF_ParamDef> params;
  // Filled by add_param while the plugin runs PARAMS_SETUP.
  bool is_recording = false;
  std::vector<FspShimRecordedParam> recorded;

  std::mutex worlds_mutex;
  std::unordered_map<const void*, std::unique_ptr<unsigned char[]>> worlds;
};

namespace fsp_shim {

// Makes a cleared picture owned by ref (freed with it, or by
// DisposeWorld). A zero size gives a picture without pixels.
PF_Err NewWorld(FspShimEffectRef* ref, A_long width, A_long height,
                PF_PixelFormat format, PF_EffectWorld* world);
PF_Err DisposeWorld(FspShimEffectRef* ref, PF_EffectWorld* world);
PF_PixelFormat WorldFormat(const PF_EffectWorld* world);
// Bytes per pixel of format, 0 for one the stand-in does not carry.
A_long PixelBytes(PF_PixelFormat format);

// The utils callbacks handed out in in_data, and the interact callbacks.
const PF_UtilCallbacks* UtilCallbacks();
PF_InteractCallbacks InteractCallbacks();

void ClearLastError();
const char* LastErrorText();
// Prints and keeps text as the error message.
void SetLastError(const std::string& text);

}  // namespace fsp_shim

#endif  // FSP_SHIM_SESSION_H_
