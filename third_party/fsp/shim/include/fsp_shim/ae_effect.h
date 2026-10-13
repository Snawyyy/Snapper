// Snapper's stand-in for the After Effects effect API: commands, errors,
// flags, parameter definitions and the in/out data a plugin is handed.
// Written for Snapper; see ae_types.h.

#ifndef FSP_SHIM_AE_EFFECT_H_
#define FSP_SHIM_AE_EFFECT_H_

#include "fsp_shim/ae_types.h"

// ---------------------------------------------------------------------
// Commands. Snapper only ever sends GLOBAL_SETUP, PARAMS_SETUP and
// RENDER; the rest exist so the plugins' switch statements compile.
typedef A_long PF_Cmd;
enum {
  PF_Cmd_ABOUT = 0,
  PF_Cmd_GLOBAL_SETUP,
  PF_Cmd_UNUSED_0,
  PF_Cmd_GLOBAL_SETDOWN,
  PF_Cmd_PARAMS_SETUP,
  PF_Cmd_SEQUENCE_SETUP,
  PF_Cmd_SEQUENCE_RESETUP,
  PF_Cmd_SEQUENCE_FLATTEN,
  PF_Cmd_SEQUENCE_SETDOWN,
  PF_Cmd_DO_DIALOG,
  PF_Cmd_FRAME_SETUP,
  PF_Cmd_RENDER,
  PF_Cmd_FRAME_SETDOWN,
  PF_Cmd_USER_CHANGED_PARAM,
  PF_Cmd_UPDATE_PARAMS_UI,
  PF_Cmd_EVENT,
  PF_Cmd_GET_EXTERNAL_DEPENDENCIES,
  PF_Cmd_COMPLETELY_GENERAL,
  PF_Cmd_QUERY_DYNAMIC_FLAGS,
  PF_Cmd_AUDIO_RENDER,
  PF_Cmd_AUDIO_SETUP,
  PF_Cmd_AUDIO_SETDOWN,
  PF_Cmd_ARBITRARY_CALLBACK,
  PF_Cmd_SMART_PRE_RENDER,
  PF_Cmd_SMART_RENDER,
  PF_Cmd_RESERVED1,
  PF_Cmd_RESERVED2,
  PF_Cmd_RESERVED3,
  PF_Cmd_GET_FLATTENED_SEQUENCE_DATA,
  PF_Cmd_TRANSLATE_PARAMS_TO_PREFS,
  PF_Cmd_RESERVED4,
  PF_Cmd_SMART_RENDER_GPU,
  PF_Cmd_GPU_DEVICE_SETUP,
  PF_Cmd_GPU_DEVICE_SETDOWN,
  PF_Cmd_NUM_CMDS
};

// ---------------------------------------------------------------------
// Errors.
enum {
  PF_Err_NONE = 0,
  PF_Err_OUT_OF_MEMORY = 4,
  PF_Err_INTERNAL_STRUCT_DAMAGED = 512,
  PF_Err_INVALID_INDEX,
  PF_Err_UNRECOGNIZED_PARAM_TYPE,
  PF_Err_INVALID_CALLBACK,
  PF_Err_BAD_CALLBACK_PARAM,
  PF_Interrupt_CANCEL,
  PF_Err_CANNOT_PARSE_KEYFRAME_TEXT
};

// ---------------------------------------------------------------------
// Flags a plugin reports in out_data. Their values only have to be
// distinct bits: Snapper reads none of them.
enum {
  PF_OutFlag_NONE = 0,
  PF_OutFlag_KEEP_RESOURCE_OPEN = 1 << 0,
  PF_OutFlag_WIDE_TIME_INPUT = 1 << 1,
  PF_OutFlag_NON_PARAM_VARY = 1 << 2,
  PF_OutFlag_RESERVED6 = 1 << 3,
  PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING = 1 << 4,
  PF_OutFlag_I_DO_DIALOG = 1 << 5,
  PF_OutFlag_USE_OUTPUT_EXTENT = 1 << 6,
  PF_OutFlag_SEND_DO_DIALOG = 1 << 7,
  PF_OutFlag_DISPLAY_ERROR_MESSAGE = 1 << 8,
  PF_OutFlag_I_EXPAND_BUFFER = 1 << 9,
  PF_OutFlag_PIX_INDEPENDENT = 1 << 10,
  PF_OutFlag_I_WRITE_INPUT_BUFFER = 1 << 11,
  PF_OutFlag_I_SHRINK_BUFFER = 1 << 12,
  PF_OutFlag_WORKS_IN_PLACE = 1 << 13,
  PF_OutFlag_RESERVED8 = 1 << 14,
  PF_OutFlag_CUSTOM_UI = 1 << 15,
  PF_OutFlag_RESERVED7 = 1 << 16,
  PF_OutFlag_REFRESH_UI = 1 << 17,
  PF_OutFlag_NOP_RENDER = 1 << 18,
  PF_OutFlag_I_USE_SHUTTER_ANGLE = 1 << 19,
  PF_OutFlag_I_USE_AUDIO = 1 << 20,
  PF_OutFlag_I_AM_OBSOLETE = 1 << 21,
  PF_OutFlag_FORCE_RERENDER = 1 << 22,
  PF_OutFlag_PiPL_OVERRIDES_OUTDATA_OUTFLAGS = 1 << 23,
  PF_OutFlag_I_HAVE_EXTERNAL_DEPENDENCIES = 1 << 24,
  PF_OutFlag_DEEP_COLOR_AWARE = 1 << 25,
  PF_OutFlag_SEND_UPDATE_PARAMS_UI = 1 << 26,
  PF_OutFlag_AUDIO_FLOAT_ONLY = 1 << 27,
  PF_OutFlag_AUDIO_IIR = 1 << 28,
  PF_OutFlag_I_SYNTHESIZE_AUDIO = 1 << 29,
  PF_OutFlag_AUDIO_EFFECT_TOO = 1 << 30
};
enum {
  PF_OutFlag2_NONE = 0,
  PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS = 1 << 0,
  PF_OutFlag2_I_USE_3D_CAMERA = 1 << 1,
  PF_OutFlag2_I_USE_3D_LIGHTS = 1 << 2,
  PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG = 1 << 3,
  PF_OutFlag2_I_AM_THREADSAFE = 1 << 4,
  PF_OutFlag2_CAN_COMBINE_WITH_DESTINATION = 1 << 5,
  PF_OutFlag2_DOESNT_NEED_EMPTY_PIXELS = 1 << 6,
  PF_OutFlag2_REVEALS_ZERO_ALPHA = 1 << 7,
  PF_OutFlag2_PRESERVES_FULLY_OPAQUE_PIXELS = 1 << 8,
  PF_OutFlag2_SUPPORTS_SMART_RENDER = 1 << 10,
  PF_OutFlag2_FLOAT_COLOR_AWARE = 1 << 12,
  PF_OutFlag2_I_USE_COLORSPACE_ENUMERATION = 1 << 13,
  PF_OutFlag2_I_AM_DEPRECATED = 1 << 14,
  PF_OutFlag2_PPRO_DO_NOT_CLONE_SEQUENCE_DATA_FOR_RENDER = 1 << 15,
  PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT = 1 << 17,
  PF_OutFlag2_I_USE_TIMECODE = 1 << 18,
  PF_OutFlag2_DEPENDS_ON_UNREFERENCED_MASKS = 1 << 19,
  PF_OutFlag2_OUTPUT_IS_WATERMARKED = 1 << 20,
  PF_OutFlag2_I_MIX_GUID_DEPENDENCIES = 1 << 21,
  PF_OutFlag2_SUPPORTS_THREADED_RENDERING = 1 << 27,
  PF_OutFlag2_MUTABLE_RENDER_SEQUENCE_DATA_SLOWER = 1 << 28,
  PF_OutFlag2_SUPPORTS_GPU_RENDER_F32 = 1 << 25
};
enum {
  PF_InFlag_NONE = 0,
  PF_InFlag_PROJECT_IS_RENDER_ONLY = 1 << 0
};
enum {
  PF_Quality_DRAWING_AUDIO = -1,
  PF_Quality_LO = 0,
  PF_Quality_HI = 1
};
enum {
  PF_Field_FRAME = 0,
  PF_Field_UPPER = 1,
  PF_Field_LOWER = 2
};
enum {
  PF_Stage_DEVELOP = 0,
  PF_Stage_ALPHA,
  PF_Stage_BETA,
  PF_Stage_RELEASE
};

// iterate_generic's iteration count meaning "once per thread".
enum { PF_Iterations_ONCE_PER_PROCESSOR = -1 };

// ---------------------------------------------------------------------
// Parameters.
typedef A_long PF_ParamType;
enum {
  PF_Param_RESERVED = -1,
  PF_Param_LAYER = 0,
  PF_Param_SLIDER,
  PF_Param_FIX_SLIDER,
  PF_Param_ANGLE,
  PF_Param_CHECKBOX,
  PF_Param_COLOR,
  PF_Param_POINT,
  PF_Param_POPUP,
  PF_Param_CUSTOM,
  PF_Param_NO_DATA,
  PF_Param_FLOAT_SLIDER,
  PF_Param_ARBITRARY_DATA,
  PF_Param_PATH,
  PF_Param_GROUP_START,
  PF_Param_GROUP_END,
  PF_Param_BUTTON,
  PF_Param_RESERVED2,
  PF_Param_RESERVED3,
  PF_Param_POINT_3D
};

typedef A_long PF_ParamFlags;
enum {
  PF_ParamFlag_NONE = 0,
  PF_ParamFlag_RESERVED1 = 1 << 0,
  PF_ParamFlag_CANNOT_TIME_VARY = 1 << 1,
  PF_ParamFlag_CANNOT_INTERP = 1 << 2,
  PF_ParamFlag_RESERVED2 = 1 << 3,
  PF_ParamFlag_RESERVED3 = 1 << 4,
  PF_ParamFlag_COLLAPSE_TWIRLY = 1 << 5,
  PF_ParamFlag_SUPERVISE = 1 << 6,
  PF_ParamFlag_START_COLLAPSED = PF_ParamFlag_COLLAPSE_TWIRLY,
  PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS = 1 << 7,
  PF_ParamFlag_LAYER_PARAM_IS_TRACKMATTE = 1 << 7,
  PF_ParamFlag_EXCLUDE_FROM_HAVE_INPUTS_CHANGED = 1 << 8,
  PF_ParamFlag_SKIP_REVEAL_WHEN_UNHIDDEN = 1 << 9
};

typedef A_long PF_ParamUIFlags;
enum {
  PF_PUI_NONE = 0,
  PF_PUI_TOPIC = 1 << 0,
  PF_PUI_CONTROL = 1 << 1,
  PF_PUI_STD_CONTROL_ONLY = 1 << 2,
  PF_PUI_NO_ECW_UI = 1 << 3,
  PF_PUI_ECW_SEPARATOR = 1 << 4,
  PF_PUI_DISABLED = 1 << 5,
  PF_PUI_DONT_ERASE_TOPIC = 1 << 6,
  PF_PUI_DONT_ERASE_CONTROL = 1 << 7,
  PF_PUI_RADIO_BUTTON = 1 << 8,
  PF_PUI_INVISIBLE = 1 << 9
};

enum {
  PF_ChangeFlag_NONE = 0,
  PF_ChangeFlag_CHANGED_VALUE = 1 << 0,
  PF_ChangeFlag_RESERVED = 1 << 1,
  PF_ChangeFlag_SET_TO_VARY = 1 << 2,
  PF_ChangeFlag_SET_TO_CONSTANT = 1 << 3
};

typedef A_long PF_ValueDisplayFlags;
enum {
  PF_ValueDisplayFlag_NONE = 0,
  PF_ValueDisplayFlag_PERCENT = 1 << 0,
  PF_ValueDisplayFlag_PIXEL = 1 << 1,
  PF_ValueDisplayFlag_RESERVED1 = 1 << 2,
  PF_ValueDisplayFlag_REVERSE = 1 << 3
};
enum {
  PF_Precision_INTEGER = 0,
  PF_Precision_TENTHS,
  PF_Precision_HUNDREDTHS,
  PF_Precision_THOUSANDTHS,
  PF_Precision_TEN_THOUSANDTHS
};
typedef A_long PF_FSliderFlags;
enum {
  PF_FSliderFlag_NONE = 0,
  PF_FSliderFlag_WANT_PHASE = 1 << 0
};
enum {
  PF_LayerDefault_MYSELF = -1,
  PF_LayerDefault_NONE = 0
};

#define PF_MAX_EFFECT_PARAM_NAME_LEN 31

typedef struct {
  PF_ParamValue value;
  A_char value_str[PF_MAX_EFFECT_PARAM_NAME_LEN + 1];
  A_char value_desc[PF_MAX_EFFECT_PARAM_NAME_LEN + 1];
  PF_ParamValue valid_min, valid_max;
  PF_ParamValue slider_min, slider_max;
  PF_ParamValue dephault;
} PF_SliderDef;

typedef struct {
  PF_Fixed value;
  A_char value_str[PF_MAX_EFFECT_PARAM_NAME_LEN + 1];
  A_char value_desc[PF_MAX_EFFECT_PARAM_NAME_LEN + 1];
  PF_Fixed valid_min, valid_max;
  PF_Fixed slider_min, slider_max;
  PF_Fixed dephault;
  A_short precision;
  A_short display_flags;
} PF_FixedSliderDef;

typedef struct {
  PF_FpLong value;
  PF_FpLong phase;
  A_char value_desc[PF_MAX_EFFECT_PARAM_NAME_LEN + 1];
  PF_FpShort valid_min, valid_max;
  PF_FpShort slider_min, slider_max;
  PF_FpShort dephault;
  A_short precision;
  A_short display_flags;
  PF_FSliderFlags fs_flags;
  PF_FpShort curve_tolerance;
  PF_Boolean useExponent;
  PF_FpShort exponent;
} PF_FloatSliderDef;

typedef struct {
  PF_Fixed value;
  PF_Fixed dephault;
  PF_Fixed valid_min, valid_max;
} PF_AngleDef;

typedef struct {
  PF_ParamValue value;
  PF_Boolean dephault;
  A_char reserved;
  A_short reserved1;
  union { const A_char* nameptr; } u;
} PF_CheckBoxDef;

typedef struct {
  PF_UnionablePixel value;
  PF_UnionablePixel dephault;
} PF_ColorDef;

typedef struct {
  PF_Fixed x_value;
  PF_Fixed y_value;
  A_char reserved[3];
  PF_Boolean restrict_bounds;
  PF_Fixed x_dephault;
  PF_Fixed y_dephault;
} PF_PointDef;

typedef struct {
  PF_ParamValue value;
  A_short num_choices;
  A_short dephault;
  union { const A_char* namesptr; } u;
} PF_PopupDef;

typedef struct {
  PF_ParamValue value;
  union { const A_char* namesptr; } u;
} PF_ButtonDef;

typedef struct {
  PF_PathID path_id;
  A_long reserved0;
  A_long dephault;
} PF_PathDef;

typedef struct {
  A_short id;
  A_short pad;
  PF_ArbitraryH dephault;
  PF_ArbitraryH value;
  void* refconPV;
} PF_ArbitraryDef;

typedef struct {
  PF_FpLong x_value, y_value, z_value;
  PF_FpLong x_dephault, y_dephault, z_dephault;
} PF_Point3DDef;

typedef union {
  PF_LayerDef ld;
  PF_SliderDef sd;
  PF_FixedSliderDef fd;
  PF_AngleDef ad;
  PF_CheckBoxDef bd;
  PF_ColorDef cd;
  PF_PointDef td;
  PF_PopupDef pd;
  PF_FloatSliderDef fs_d;
  PF_ArbitraryDef arb_d;
  PF_PathDef path_d;
  PF_ButtonDef button_d;
  PF_Point3DDef point3d_d;
} PF_ParamDefUnion;

typedef struct PF_ParamDef {
  union {
    A_long id;
    PF_ChangeFlags change_flags;
  } uu;
  PF_ParamUIFlags ui_flags;
  A_short ui_width;
  A_short ui_height;
  PF_ParamType param_type;
  A_char name[PF_MAX_EFFECT_PARAM_NAME_LEN + 1];
  PF_ParamFlags flags;
  A_long unused;
  PF_ParamDefUnion u;
} PF_ParamDef, *PF_ParamDefPtr, **PF_ParamDefH;
typedef PF_ParamDef** PF_ParamList;

// Plugin code names the parameter's name field through this.
#define PF_DEF_NAME name

typedef struct {
  PF_ParamIndex param_index;
} PF_UserChangedParamExtra;

#endif  // FSP_SHIM_AE_EFFECT_H_
