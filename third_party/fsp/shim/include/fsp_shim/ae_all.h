// Everything of Snapper's After Effects stand-in. Each SDK header name a
// plugin includes (AE_Effect.h, Param_Utils.h, ...) is a one-line file in
// shim/include that includes this.

#ifndef FSP_SHIM_AE_ALL_H_
#define FSP_SHIM_AE_ALL_H_

// The plugins pick the newer suite helpers when these are set.
#define PF_AE100_PLUG_IN_VERSION 1
#define PF_AE110_PLUG_IN_VERSION 1
#define PF_PLUG_IN_VERSION 13
#define PF_PLUG_IN_SUBVERS 28

#include "fsp_shim/ae_aegp.h"
#include "fsp_shim/ae_callbacks.h"
#include "fsp_shim/ae_effect.h"
#include "fsp_shim/ae_macros.h"
#include "fsp_shim/ae_params.h"
#include "fsp_shim/ae_suite_helper.h"
#include "fsp_shim/ae_suites.h"
#include "fsp_shim/ae_types.h"
#include "fsp_shim/host_api.h"

#endif  // FSP_SHIM_AE_ALL_H_
