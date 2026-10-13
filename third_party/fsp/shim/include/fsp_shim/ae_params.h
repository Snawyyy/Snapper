// Snapper's stand-in for the After Effects effect API: the PF_ADD_
// macros a plugin's ParamsSetup declares its parameters with. As in
// After Effects they fill the caller's `def`, hand it to
// in_data->inter.add_param and return from the caller on an error; the
// caller clears `def` itself (AEFX_CLR_STRUCT) between parameters.
// Written for Snapper; see ae_types.h.

#ifndef FSP_SHIM_AE_PARAMS_H_
#define FSP_SHIM_AE_PARAMS_H_

#include "fsp_shim/ae_macros.h"

#define FSP_SHIM_ADD_DEF(NAME, ID) \
  fsp_shim_strnncpy(def.name, (NAME), sizeof(def.name)); \
  def.uu.id = (ID); \
  const PF_Err add_err = PF_ADD_PARAM(in_data, -1, &def); \
  const bool add_failed = add_err != PF_Err_NONE; \
  if (add_failed) { return add_err; }

#define PF_ADD_SLIDER(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, \
                      DFLT, ID) \
  do { \
    def.param_type = PF_Param_SLIDER; \
    def.u.sd.value_str[0] = def.u.sd.value_desc[0] = '\0'; \
    def.u.sd.valid_min = (VALID_MIN); \
    def.u.sd.valid_max = (VALID_MAX); \
    def.u.sd.slider_min = (SLIDER_MIN); \
    def.u.sd.slider_max = (SLIDER_MAX); \
    def.u.sd.value = def.u.sd.dephault = (DFLT); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_FIXED(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, \
                     DFLT, PREC, DISP, FLAGS, ID) \
  do { \
    def.param_type = PF_Param_FIX_SLIDER; \
    def.u.fd.value_str[0] = def.u.fd.value_desc[0] = '\0'; \
    def.u.fd.valid_min = FLOAT2FIX(VALID_MIN); \
    def.u.fd.valid_max = FLOAT2FIX(VALID_MAX); \
    def.u.fd.slider_min = FLOAT2FIX(SLIDER_MIN); \
    def.u.fd.slider_max = FLOAT2FIX(SLIDER_MAX); \
    def.u.fd.value = def.u.fd.dephault = FLOAT2FIX(DFLT); \
    def.u.fd.precision = static_cast<A_short>(PREC); \
    def.u.fd.display_flags = static_cast<A_short>(DISP); \
    def.flags |= (FLAGS); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_PERCENT(NAME, DFLT, ID) \
  PF_ADD_FIXED(NAME, 0, 100, 0, 100, DFLT, 1, \
               PF_ValueDisplayFlag_PERCENT, 0, ID)

#define PF_ADD_FLOAT_SLIDERX(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, \
                             SLIDER_MAX, DFLT, PREC, DISP, FLAGS, ID) \
  do { \
    def.param_type = PF_Param_FLOAT_SLIDER; \
    def.u.fs_d.value_desc[0] = '\0'; \
    def.u.fs_d.valid_min = static_cast<PF_FpShort>(VALID_MIN); \
    def.u.fs_d.valid_max = static_cast<PF_FpShort>(VALID_MAX); \
    def.u.fs_d.slider_min = static_cast<PF_FpShort>(SLIDER_MIN); \
    def.u.fs_d.slider_max = static_cast<PF_FpShort>(SLIDER_MAX); \
    def.u.fs_d.value = (DFLT); \
    def.u.fs_d.dephault = static_cast<PF_FpShort>(DFLT); \
    def.u.fs_d.precision = static_cast<A_short>(PREC); \
    def.u.fs_d.display_flags = static_cast<A_short>(DISP); \
    def.flags |= (FLAGS); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_FLOAT_SLIDER(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, \
                            SLIDER_MAX, CURVE_TOLERANCE, DFLT, PREC, DISP, \
                            WANT_PHASE, ID) \
  do { \
    def.u.fs_d.curve_tolerance = static_cast<PF_FpShort>(CURVE_TOLERANCE); \
    def.u.fs_d.fs_flags = (WANT_PHASE) ? PF_FSliderFlag_WANT_PHASE : 0; \
    PF_ADD_FLOAT_SLIDERX(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, \
                         SLIDER_MAX, DFLT, PREC, DISP, 0, ID); \
  } while (0)

#define PF_ADD_ANGLE(NAME, DFLT, ID) \
  do { \
    def.param_type = PF_Param_ANGLE; \
    def.u.ad.value = def.u.ad.dephault = FLOAT2FIX(DFLT); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_CHECKBOX(NAME_A, NAME_B, DFLT, FLAGS, ID) \
  do { \
    def.param_type = PF_Param_CHECKBOX; \
    def.u.bd.value = (DFLT); \
    def.u.bd.dephault = static_cast<PF_Boolean>(def.u.bd.value); \
    def.u.bd.u.nameptr = (NAME_B); \
    def.flags = (FLAGS); \
    FSP_SHIM_ADD_DEF(NAME_A, ID) \
  } while (0)

#define PF_ADD_CHECKBOXX(NAME, DFLT, FLAGS, ID) \
  PF_ADD_CHECKBOX(NAME, "", DFLT, FLAGS, ID)

#define PF_ADD_COLOR(NAME, RED, GREEN, BLUE, ID) \
  do { \
    def.param_type = PF_Param_COLOR; \
    def.u.cd.value.red = static_cast<A_u_char>(RED); \
    def.u.cd.value.green = static_cast<A_u_char>(GREEN); \
    def.u.cd.value.blue = static_cast<A_u_char>(BLUE); \
    def.u.cd.value.alpha = 255; \
    def.u.cd.dephault = def.u.cd.value; \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

// Point defaults are in percent of the layer, as in After Effects.
#define PF_ADD_POINT(NAME, X_DFLT, Y_DFLT, RESTRICT_BOUNDS, ID) \
  do { \
    def.param_type = PF_Param_POINT; \
    def.u.td.restrict_bounds = (RESTRICT_BOUNDS); \
    def.u.td.x_value = def.u.td.x_dephault = FLOAT2FIX(X_DFLT); \
    def.u.td.y_value = def.u.td.y_dephault = FLOAT2FIX(Y_DFLT); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_POPUPX(NAME, NUM_CHOICES, DFLT, ITEMS, FLAGS, ID) \
  do { \
    def.param_type = PF_Param_POPUP; \
    def.u.pd.num_choices = static_cast<A_short>(NUM_CHOICES); \
    def.u.pd.dephault = static_cast<A_short>(DFLT); \
    def.u.pd.value = def.u.pd.dephault; \
    def.u.pd.u.namesptr = (ITEMS); \
    def.flags = (FLAGS); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_POPUP(NAME, NUM_CHOICES, DFLT, ITEMS, ID) \
  do { \
    def.param_type = PF_Param_POPUP; \
    def.u.pd.num_choices = static_cast<A_short>(NUM_CHOICES); \
    def.u.pd.dephault = static_cast<A_short>(DFLT); \
    def.u.pd.value = def.u.pd.dephault; \
    def.u.pd.u.namesptr = (ITEMS); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_LAYER(NAME, DFLT, ID) \
  do { \
    def.param_type = PF_Param_LAYER; \
    def.u.ld.dephault = (DFLT); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_TOPICX(NAME, FLAGS, ID) \
  do { \
    def.param_type = PF_Param_GROUP_START; \
    def.flags = (FLAGS); \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#define PF_ADD_TOPIC(NAME, ID) PF_ADD_TOPICX(NAME, 0, ID)

#define PF_END_TOPIC(ID) \
  do { \
    def.param_type = PF_Param_GROUP_END; \
    FSP_SHIM_ADD_DEF("", ID) \
  } while (0)

#define PF_ADD_BUTTON(PARAM_NAME, BUTTON_NAME, PUI_FLAGS, PARAM_FLAGS, ID) \
  do { \
    def.param_type = PF_Param_BUTTON; \
    def.u.button_d.u.namesptr = (BUTTON_NAME); \
    def.ui_flags = (PUI_FLAGS); \
    def.flags = (PARAM_FLAGS); \
    FSP_SHIM_ADD_DEF(PARAM_NAME, ID) \
  } while (0)

#define PF_ADD_NULL(NAME, ID) \
  do { \
    def.param_type = PF_Param_NO_DATA; \
    FSP_SHIM_ADD_DEF(NAME, ID) \
  } while (0)

#endif  // FSP_SHIM_AE_PARAMS_H_
