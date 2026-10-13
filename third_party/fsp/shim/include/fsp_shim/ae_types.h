// Snapper's stand-in for the After Effects effect API: the plain types.
//
// Written for Snapper from the way F's Plugins use the API (names and
// meanings only); no text of Adobe's SDK is copied. Field and type names
// match what plugin code reads, layouts do not need to match Adobe's
// binaries because nothing here talks to After Effects itself.

#ifndef FSP_SHIM_AE_TYPES_H_
#define FSP_SHIM_AE_TYPES_H_

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------
// Basic numbers. A_long is a 32 bit int, as in After Effects (plugin code
// passes int* where A_long* is asked for).
typedef std::int32_t A_long;
typedef std::uint32_t A_u_long;
typedef short A_short;
typedef unsigned short A_u_short;
typedef char A_char;
typedef unsigned char A_u_char;
typedef unsigned char A_Boolean;
typedef double A_FpLong;
typedef float A_FpShort;
typedef A_long A_Err;
typedef void* A_Handle;
typedef std::intptr_t A_intptr_t;

typedef struct { A_FpLong x, y; } A_FloatPoint;
typedef struct { A_FpLong x, y, z; } A_FloatPoint3;
typedef struct { A_FpLong left, top, right, bottom; } A_FloatRect;
typedef struct { A_FpLong mat[3][3]; } A_Matrix3;
typedef struct { A_FpLong mat[4][4]; } A_Matrix4;
typedef struct { A_long value; A_u_long scale; } A_Time;
typedef struct { A_long num; A_u_long den; } A_Ratio;
typedef struct { A_long left, top, right, bottom; } A_LRect;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef A_char PF_Boolean;
typedef A_long PF_Fixed;
typedef A_u_long PF_UFixed;
typedef A_char PF_Fract;
typedef A_long PF_ParamValue;
typedef A_long PF_ParamIndex;
typedef A_long PF_Err;
typedef double PF_FpLong;
typedef float PF_FpShort;
typedef void** PF_Handle;
typedef PF_Handle PF_ArbitraryH;
typedef A_u_long PF_PathID;
typedef struct FspShimPathOutline* PF_PathOutlinePtr;
typedef struct FspShimPathSegPrep* PF_PathSegPrepPtr;
typedef struct FspShimEffectRef* PF_ProgPtr;
typedef struct FspShimPluginData* PF_PluginDataPtr;
typedef A_long PF_Field;
typedef A_long PF_ChannelMask;
typedef A_long PF_Quality;
typedef A_long PF_ModeFlags;
typedef A_long PF_ChangeFlags;

typedef A_long AEGP_PluginID;
typedef struct FspShimAegpEffect* AEGP_EffectRefH;
typedef struct FspShimAegpStream* AEGP_StreamRefH;
typedef struct FspShimAegpLayer* AEGP_LayerH;
typedef struct FspShimAegpComp* AEGP_CompH;
typedef struct FspShimAegpItem* AEGP_ItemH;
typedef A_long AEGP_DynStreamFlags;
typedef A_long AEGP_RenderQueueState;
enum {
  AEGP_DynStreamFlag_ACTIVE_EYEBALL = 1 << 0,
  AEGP_DynStreamFlag_HIDDEN = 1 << 1,
  AEGP_DynStreamFlag_DISABLED = 1 << 2,
  AEGP_DynStreamFlag_ELIDED = 1 << 3,
  AEGP_DynStreamFlag_SHOWN_WHEN_EMPTY = 1 << 4,
  AEGP_DynStreamFlag_SKIP_REVEAL_WHEN_UNHIDDEN = 1 << 5
};
enum {
  AEGP_RenderQueueState_STOPPED = 0,
  AEGP_RenderQueueState_PAUSED,
  AEGP_RenderQueueState_RENDERING
};

// ---------------------------------------------------------------------
// Pixels. The byte order in memory is alpha, red, green, blue.
typedef struct { A_u_char alpha, red, green, blue; } PF_Pixel;
typedef PF_Pixel PF_Pixel8;
typedef PF_Pixel PF_UnionablePixel;
typedef struct { A_u_short alpha, red, green, blue; } PF_Pixel16;
typedef struct { PF_FpShort alpha, red, green, blue; } PF_PixelFloat;
typedef PF_PixelFloat PF_Pixel32;
typedef PF_Pixel* PF_PixelPtr;

enum {
  PF_MAX_CHAN8 = 255,
  PF_HALF_CHAN8 = 128,
  PF_MAX_CHAN16 = 32768,
  PF_HALF_CHAN16 = 16384
};

typedef A_long PF_PixelFormat;
enum {
  PF_PixelFormat_ARGB32 = 1,
  PF_PixelFormat_ARGB64,
  PF_PixelFormat_ARGB128,
  PF_PixelFormat_GPU_BGRA128,
  PF_PixelFormat_RESERVED,
  PF_PixelFormat_BGRA32,
  PF_PixelFormat_VUYA32,
  PF_PixelFormat_NTSCDV25,
  PF_PixelFormat_PALDV25,
  PF_PixelFormat_INVALID = -1
};

// ---------------------------------------------------------------------
// Geometry.
typedef struct { A_long left, top, right, bottom; } PF_Rect;
typedef PF_Rect PF_LRect;
typedef PF_Rect PF_UnionableRect;
typedef struct { A_FpLong left, top, right, bottom; } PF_FloatRect;
typedef struct {
  union { A_long h; A_long x; };
  union { A_long v; A_long y; };
} PF_Point;
typedef struct { PF_Fixed x, y; } PF_FixedPoint;
typedef struct { A_long num; A_u_long den; } PF_RationalScale;
typedef struct { PF_Fixed mat[3][3]; } PF_FixedMatrix;
typedef struct { PF_FpLong mat[3][3]; } PF_FloatMatrix;
typedef PF_FloatMatrix PF_CompositeMatrix;

// ---------------------------------------------------------------------
// Worlds (pictures). PF_LayerDef and PF_EffectWorld are the same thing.
typedef A_long PF_WorldFlags;
enum {
  PF_WorldFlag_DEEP = 1 << 0,
  PF_WorldFlag_WRITEABLE = 1 << 1,
  // Snapper's own mark for a 32 bit float world (After Effects keeps
  // the format elsewhere; plugins only ask through PF_GetPixelFormat).
  PF_WorldFlag_FSP_FLOAT = 1 << 28,
  PF_WorldFlag_RESERVED0 = 1 << 24,
  PF_WorldFlag_RESERVED1 = 1 << 25
};

typedef struct PF_LayerDef {
  void* reserved0;
  void* reserved1;
  PF_WorldFlags world_flags;
  PF_PixelPtr data;
  A_long rowbytes;
  A_long width;
  A_long height;
  PF_UnionableRect extent_hint;
  void* platform_ref;
  A_long reserved_long1;
  void* reserved_long4;
  PF_RationalScale pix_aspect_ratio;
  void* reserved_long2;
  A_long origin_x;
  A_long origin_y;
  A_long reserved_long3;
  A_long dephault;
} PF_LayerDef;
typedef PF_LayerDef PF_EffectWorld;
typedef PF_LayerDef PF_World;

typedef A_long PF_NewWorldFlags;
enum {
  PF_NewWorldFlag_NONE = 0,
  PF_NewWorldFlag_CLEAR_PIXELS = 1 << 0,
  PF_NewWorldFlag_DEEP_PIXELS = 1 << 1,
  PF_NewWorldFlag_RESERVED0 = 1 << 2,
  PF_NewWorldFlag_RESERVED1 = 1 << 3
};

#endif  // FSP_SHIM_AE_TYPES_H_
