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

#ifndef __GPU_H__
#define __GPU_H__

#define PSXWIDTH    640
#define PSXHEIGHT   512

int gpuReadStatus();

void psxDma2(u32 madr, u32 bcr, u32 chcr);
void gpuInterrupt();

void GPU_Open(void);
void GPU_Close(void);
void GPU_WriteStatus(u32);
void GPU_WriteData(u32);
void GPU_WriteDataMem(u32 *, int);
u32 GPU_ReadStatus(void);
u32 GPU_ReadData(void);
void GPU_ReadDataMem(u32 *, int);
long GPU_DmaChain(u32 *, u32);
void GPU_UpdateLace(void);
void GPU_HSync(int);
void GPU_VBlank(int);

#endif
