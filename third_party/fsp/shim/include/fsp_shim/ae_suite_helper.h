// Snapper's stand-in for the After Effects effect API: the AEFX helpers
// that acquire a suite by name or hold one for a scope.
// Written for Snapper; see ae_types.h.

#ifndef FSP_SHIM_AE_SUITE_HELPER_H_
#define FSP_SHIM_AE_SUITE_HELPER_H_

#include "fsp_shim/ae_aegp.h"
#include "fsp_shim/host_api.h"

PF_Err AEFX_AcquireSuite(PF_InData* in_data, PF_OutData* out_data,
                         const char* name, A_long version,
                         const char* error_string, void** suite);
PF_Err AEFX_ReleaseSuite(PF_InData* in_data, PF_OutData* out_data,
                         const char* name, A_long version,
                         const char* error_string);

// Like After Effects' own, a suite that cannot be had throws a PF_Err,
// which the plugins' EffectMain catches: plugin code is written for that.
template <typename SuiteType>
class AEFX_SuiteScoper {
 public:
  AEFX_SuiteScoper(const PF_InData* in_data, const char* name,
                   A_long version, PF_OutData* out_data = nullptr,
                   const char* error_string = nullptr)
      : basic_(in_data->pica_basicP), name_(name), version_(version) {
    (void)out_data;
    const void* suite = nullptr;
    const SPErr err = basic_->AcquireSuite(name, version, &suite);
    const bool is_missing = err != 0 || suite == nullptr;
    if (is_missing) {
      fsp_shim_fail(error_string != nullptr ? error_string : name);
      throw static_cast<PF_Err>(PF_Err_BAD_CALLBACK_PARAM);
    }
    suite_ = static_cast<const SuiteType*>(suite);
  }
  ~AEFX_SuiteScoper() { basic_->ReleaseSuite(name_, version_); }

  const SuiteType* operator->() const { return suite_; }
  const SuiteType* get() const { return suite_; }

 private:
  const SPBasicSuite* basic_;
  const char* name_;
  A_long version_;
  const SuiteType* suite_ = nullptr;
};

#endif  // FSP_SHIM_AE_SUITE_HELPER_H_
