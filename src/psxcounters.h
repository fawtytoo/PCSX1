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

#ifndef __PSXCOUNTERS_H__
#define __PSXCOUNTERS_H__

#include "psxcommon.h"
#include "r3000a.h"
#include "psxmem.h"
#include "gpu.h"

extern u32 psxNextCounter, psxNextsCounter;

void psxRcntInit();
void psxRcntUpdate();

void psxRcntWcount(u32 index, u32 value);
void psxRcntWmode(u32 index, u32 value);
void psxRcntWtarget(u32 index, u32 value);

u32 psxRcntRcount(u32 index);
u32 psxRcntRmode(u32 index);
u32 psxRcntRtarget(u32 index);

#endif
