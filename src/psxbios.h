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

#ifndef __PSXBIOS_H__
#define __PSXBIOS_H__

#include "psxcommon.h"
#include "r3000a.h"
#include "psxmem.h"
#include "misc.h"
#include "sio.h"

void psxBiosInit();
void psxBiosShutdown();
void psxBiosException();

extern void (*biosA0[256])(void);
extern void (*biosB0[256])(void);
extern void (*biosC0[256])(void);

extern bool hleSoftCall;

#endif
