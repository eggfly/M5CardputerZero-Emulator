#pragma once
#include "emu_compat.h"
#ifdef _WIN32
#include "compat/win32_stubs.h"
// MinGW doesn't properly link __attribute__((weak)) functions.
// Redefine it to nothing so weak functions become normal (strong) definitions.
// --allow-multiple-definition handles any resulting dupes.
#ifdef __MINGW32__
#undef __attribute__
#define __attribute__(x)
#endif
#endif
