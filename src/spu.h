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

#ifndef __SPU_H__
#define __SPU_H__

//#include "psxcommon.h"
//#include "plugins.h"
//#include "r3000a.h"
#include "psxmem.h"
#include "decode_xa.h"

#include "spu_registers.h"

#include "spu.h"

#define SAMPLERATE  44100
#define BUFFERSIZE  65535

extern short    pSndBuffer[BUFFERSIZE + 1];
extern int      iWritePos;

extern int      SSumR, SSumL;

void SPU_Open(void);
void SPU_Close(void);
void SPU_PlaySample(u8);
void SPU_WriteRegister(u32, u16);
u16 SPU_ReadRegister(u32);
void SPU_WriteDMAMem(u16 *, int);
void SPU_ReadDMAMem(u16 *, int);
void SPU_PlayADPCM_Channel(xa_decode_t *);
void SPU_RegisterCallback(void (*callback)(void));
void SPU_Async(u32);
void SPU_PlayCDDA_Channel(short *, int);
void SPU_Irq(void);

void SPU_Cycle(void);

#endif
