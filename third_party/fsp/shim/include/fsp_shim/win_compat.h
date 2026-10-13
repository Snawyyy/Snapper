// The few Windows calls F's code makes, for Linux. windows.h, Windows.h
// and shlobj.h in shim/include include this.

#ifndef FSP_SHIM_WIN_COMPAT_H_
#define FSP_SHIM_WIN_COMPAT_H_

#include <cstdarg>
#include <cstddef>
#include <cstdio>

// Debug output goes to stderr.
inline void OutputDebugStringA(const char* text) {
  std::fputs(text != nullptr ? text : "", stderr);
}

inline int sprintf_s(char* buffer, std::size_t size, const char* format,
                     ...) {
  va_list args;
  va_start(args, format);
  const int written = std::vsnprintf(buffer, size, format, args);
  va_end(args);
  return written;
}

template <std::size_t kSize>
int sprintf_s(char (&buffer)[kSize], const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int written = std::vsnprintf(buffer, kSize, format, args);
  va_end(args);
  return written;
}

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#endif  // FSP_SHIM_WIN_COMPAT_H_
