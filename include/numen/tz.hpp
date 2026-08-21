#pragma once
#include <chrono>

// Apple's libc++ ships no tzdb; HowardHinnant/date is the reference implementation of the same API
#if NUMEN_USE_DATE_TZ
#include <date/tz.h>
namespace numen {
namespace tz = date;
}
#else
namespace numen {
namespace tz = std::chrono;
}
#endif
