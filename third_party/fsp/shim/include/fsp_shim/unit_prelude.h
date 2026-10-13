// Included first by every plugin unit (units/<plugin>_unit.cpp), outside
// the plugin's namespace. A plugin's sources are pulled in inside a
// namespace of their own; any header they include that was not already
// included here would land inside that namespace and break. So this
// pulls in the stand-in host and every standard header F's code uses
// (their include guards then keep them out of the namespace).

#ifndef FSP_SHIM_UNIT_PRELUDE_H_
#define FSP_SHIM_UNIT_PRELUDE_H_

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <chrono>
#include <climits>
#include <clocale>
#include <cmath>
#include <compare>
#include <complex>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <deque>
#include <exception>
#include <filesystem>
#include <forward_list>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <numeric>
#include <optional>
#include <ostream>
#include <queue>
#include <random>
#include <ranges>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>
#include <version>

#include "fsp_shim/ae_all.h"
#include "fsp_shim/win_compat.h"
#include "_NFLib/AE_SDK.h"

// F's preset files (NF_Settings: JSON through file dialogs) must sit at
// global scope: json.hpp names ::nlohmann. Their calls are stubs that
// fail loudly (shim/src/nf_settings.cpp); presets are After Effects UI.
#if __has_include("_NFLib/NF_Settings.h")
#include "_NFLib/json.hpp"
#include "_NFLib/NF_Settings.h"
#endif

#endif  // FSP_SHIM_UNIT_PRELUDE_H_
