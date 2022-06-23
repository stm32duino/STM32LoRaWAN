#include "mw_log_conf.h"

#include <stdarg.h>
#include <core_debug.h>

void MW_LOG([[gnu::unused]] MwLogTimestamp_t ts, [[gnu::unused]] MwLogLevel_t level, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vcore_debug(fmt, ap);
    va_end(ap);
}
