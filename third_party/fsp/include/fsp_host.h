/* F's Plugins (bryful, MIT licence) behind a plain C interface.
 *
 * Every plugin carried in third_party/fsp runs inside a stand-in for the
 * After Effects host; this header is all of it the outside sees. No After
 * Effects type crosses it. Plugins are numbered from 0; the list and each
 * plugin's parameters come from running the plugin's own setup once, the
 * first time any of these is called. Safe to call from several threads.
 */

#ifndef FSP_HOST_H_
#define FSP_HOST_H_

#ifdef __cplusplus
extern "C" {
#endif

enum FspHostParamType {
  FSP_HOST_PARAM_SLIDER = 0,   /* A whole number. */
  FSP_HOST_PARAM_FLOAT = 1,    /* A number with decimals. */
  FSP_HOST_PARAM_ANGLE = 2,    /* Degrees. */
  FSP_HOST_PARAM_CHECKBOX = 3, /* 0 or 1. */
  FSP_HOST_PARAM_POPUP = 4,    /* The choice's number, from 1. */
  FSP_HOST_PARAM_COLOR = 5,    /* 0xRRGGBB. */
  FSP_HOST_PARAM_POINT = 6,    /* Two slots, x then y, in percent of the
                                  picture's width and height. */
  FSP_HOST_PARAM_OTHER = 7     /* A layer picker, group, button or other
                                  part with no value here (hidden). */
};

typedef struct FspHostParam {
  const char* name;          /* As the plugin declares it. */
  int type;                  /* An FspHostParamType. */
  int is_hidden;             /* 1: nothing to show (FSP_HOST_PARAM_OTHER,
                                or a part the plugin hides itself). */
  double low, high;          /* The range a value may be typed in. */
  double slider_low;         /* The range the plugin's slider shows. */
  double slider_high;
  double neutral;            /* The plugin's default (x of a point). */
  double neutral_y;          /* A point's default y; else 0. */
  int choice_count;          /* Popup entries, in order. */
  const char* const* choices;
  int slot;                  /* Where its value starts in values[]. */
  int slot_count;            /* 2 for a point, else 1. */
} FspHostParam;

int fsp_host_plugin_count(void);
/* The plugin's folder name ("RimFill"); NULL when plugin is out of range. */
const char* fsp_host_plugin_id(int plugin);
/* Its name as After Effects showed it ("NF-RimFill"). */
const char* fsp_host_plugin_name(int plugin);
/* Its After Effects menu ("NF-Cell"). */
const char* fsp_host_plugin_category(int plugin);
int fsp_host_param_count(int plugin);
/* NULL when either number is out of range. */
const FspHostParam* fsp_host_param(int plugin, int param);
/* The length of values[] for fsp_host_render. */
int fsp_host_slot_count(int plugin);

/* Runs plugin over a width x height picture. argb_in and argb_out hold
 * 4 bytes a pixel in After Effects' 8 bit order (alpha, red, green, blue),
 * straight alpha, rows top first with no padding; they may not overlap.
 * values holds value_count == fsp_host_slot_count(plugin) values, clamped
 * to each parameter's range. scale is how much smaller than full size the
 * picture is (1 full, 0.5 half), as After Effects' downsampling.
 * Gives back 0 when it ran; otherwise argb_out is untouched and
 * fsp_host_last_error says why. */
int fsp_host_render(int plugin, int width, int height,
                    const unsigned char* argb_in, unsigned char* argb_out,
                    const double* values, int value_count, double scale);

/* The last failure on this thread, "" when none. */
const char* fsp_host_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* FSP_HOST_H_ */
