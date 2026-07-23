#ifndef PLATFORM_WINDOWS_INTERNAL_H
#define PLATFORM_WINDOWS_INTERNAL_H

#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define STOIN_WINDOWS_EXTRA_INFO ((ULONG_PTR)0x73746f696eULL)

bool windows_vk_from_logical(uint16_t logical_keycode, UINT *out_vk);

#endif
