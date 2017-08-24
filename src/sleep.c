#include "sleep.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif

void sleep(uint32_t millisec) {
#ifdef _WIN32
	Sleep(millisec);
#else
	usleep(millisec * 1000);
#endif
}