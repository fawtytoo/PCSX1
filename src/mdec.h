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

#ifndef __MDEC_H__
#define __MDEC_H__

#include "psxcommon.h"
#include "r3000a.h"
#include "psxhw.h"
#include "psxdma.h"

void mdecInit();
void mdecWrite0(u32 data);
void mdecWrite1(u32 data);
u32 mdecRead0();
u32 mdecRead1();
void psxDma0(u32 madr, u32 bcr, u32 chcr);
void psxDma1(u32 madr, u32 bcr, u32 chcr);
void mdec1Interrupt();

#endif
