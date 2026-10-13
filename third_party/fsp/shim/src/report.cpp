// Loud failures and the per-thread error message of the stand-in host.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "fsp_shim/host_api.h"
#include "fsp_shim/session.h"

namespace {

std::string& LastError() {
  thread_local std::string message;
  return message;
}

}  // namespace

PF_Err fsp_shim_fail(const char* what) {
  const char* text = what != nullptr ? what : "(no message)";
  std::fprintf(stderr, "fsp: %s\n", text);
  std::string& last = LastError();
  const bool is_first = last.empty();
  if (is_first) {
    last = text;
  }
  return PF_Err_BAD_CALLBACK_PARAM;
}

PF_Err fsp_shim_unsupported(const char* call) {
  assert(call != nullptr);
  const std::string text =
      std::string(call) +
      " is not supported by Snapper's After Effects stand-in yet";
  return fsp_shim_fail(text.c_str());
}

void fsp_shim_strnncpy(A_char* dst, const A_char* src, std::size_t size) {
  const bool has_room = dst != nullptr && size > 0;
  if (!has_room) {
    return;
  }
  const bool has_src = src != nullptr;
  if (!has_src) {
    dst[0] = '\0';
    return;
  }
  const std::size_t length = std::strlen(src);
  const std::size_t count = length < size ? length : size - 1;
  std::memcpy(dst, src, count);
  dst[count] = '\0';
}

namespace fsp_shim {

void ClearLastError() { LastError().clear(); }

const char* LastErrorText() { return LastError().c_str(); }

void SetLastError(const std::string& text) {
  assert(!text.empty());
  std::fprintf(stderr, "fsp: %s\n", text.c_str());
  LastError() = text;
}

}  // namespace fsp_shim
