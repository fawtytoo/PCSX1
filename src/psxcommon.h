/*
    Copyright (C) 1999-2003 Pcsx Team
    Copyright (C) 2007      PCSX-df Team
    Copyright (C) 2009      PCSX-Reloaded Authors/Contributors
    Copyright (C) 2026      PCSX1 - Steve Clark

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
                                                                              */

/*
* This file contains common definitions and includes for all parts of the
* emulator core.
*/

#ifndef __PSXCOMMON_H__
#define __PSXCOMMON_H__

// System includes
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
//#include <unistd.h>
#include <limits.h>
//#include <time.h>
#include <ctype.h>
//#include <sys/types.h>
#include <assert.h>

// Define types
typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef uintptr_t   uptr; // psxbios.c

typedef enum
{
    false,
    true
}
bool;

typedef struct
{
    char    *MemCard[2];

    bool    HLE;
    char    *Bios;
    bool    SlowBoot;

    char    CdromId;
    bool    Region; // NTSC or PAL
    bool    RegionAuto;

    bool    Widescreen;

    bool    SioIrq;
    bool    PsxOut;
    bool    SpuIrq;
    bool    RCntFix;
    bool    UseNet;
    bool    VSyncWA;
    bool    HackFix;
    bool    MemHack;
}
CONFIG;

extern CONFIG   Config;

// Make the timing events trigger faster as we are currently assuming everything
// takes one cycle, which is not the case on real hardware.
// FIXME: Count the proper cycle and get rid of this
#define BIAS    2
#define PSXCLK  33868800    /* 33.8688 MHz */

#define PSX_TYPE_NTSC   0
#define PSX_TYPE_PAL    1

void Psx_MemInit(void);

#endif
