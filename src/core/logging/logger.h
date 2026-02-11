#pragma once
#include <cstdarg>
#include <stdio.h>

#if defined(_DEBUG) || defined(BUILD_NOGUI)
inline void Log(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}
#else
#define Log(...) ((void)nullptr)
#endif