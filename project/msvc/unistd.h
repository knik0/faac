#pragma once

#include <io.h>

#define W_OK    2

#define access _access

#define __builtin_bswap32 _byteswap_ulong
#define __builtin_bswap16 _byteswap_ushort
