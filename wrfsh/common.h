#pragma once

#ifdef _MSC_VER
#define snprintf(buf, n, format, ...)  _snprintf_s(buf, n, n, format, __VA_ARGS__)
#endif