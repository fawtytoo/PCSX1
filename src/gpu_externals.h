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

#ifndef _EXTERNALS_H_
#define _EXTERNALS_H_

#define INFO_TW        0
#define INFO_DRAWSTART 1
#define INFO_DRAWEND   2
#define INFO_DRAWOFF   3

#define SHADETEXBIT(x)  ((x>>24) & 0x1)
#define SEMITRANSBIT(x) ((x>>25) & 0x1)
#define PSXRGB(r,g,b)   ((g<<10)|(b<<5)|r)

#define DATAREGISTERMODES u16

#define DR_NORMAL        0
#define DR_VRAMTRANSFER  1

#define GPUSTATUS_ODDLINES            0x80000000
#define GPUSTATUS_DMABITS             0x60000000 // Two bits
#define GPUSTATUS_READYFORCOMMANDS    0x10000000
#define GPUSTATUS_READYFORVRAM        0x08000000
#define GPUSTATUS_IDLE                0x04000000
#define GPUSTATUS_DISPLAYDISABLED     0x00800000
#define GPUSTATUS_INTERLACED          0x00400000
#define GPUSTATUS_RGB24               0x00200000
#define GPUSTATUS_PAL                 0x00100000
#define GPUSTATUS_DOUBLEHEIGHT        0x00080000
#define GPUSTATUS_WIDTHBITS           0x00070000 // Three bits
#define GPUSTATUS_MASKENABLED         0x00001000
#define GPUSTATUS_MASKDRAWN           0x00000800
#define GPUSTATUS_DRAWINGALLOWED      0x00000400
#define GPUSTATUS_DITHER              0x00000200

#include "psxcommon.h"

#include "gpu_prim.h"
#include "gpu_swap.h"
#include "gpu_soft.h"

/////////////////////////////////////////////////////////////////////////////

typedef struct VRAMLOADTTAG
{
    short           x;
    short           y;
    short           Width;
    short           Height;
    short           RowsRemaining;
    short           ColsRemaining;
    u16     *ImagePtr;
}
VRAMLoad_t;

/////////////////////////////////////////////////////////////////////////////

typedef struct PSXPOINTTAG
{
    s32 x;
    s32 y;
}
PSXPoint_t;

typedef struct PSXSPOINTTAG
{
    short   x;
    short   y;
}
PSXSPoint_t;

typedef struct PSXRECTTAG
{
    short   x0;
    short   x1;
    short   y0;
    short   y1;
}
PSXRect_t;

// linux defines for some windows stuff

#define FALSE   0
#define TRUE    1
#define BOOL    u16
#define LOWORD(l)           ((u16)(l))
#define HIWORD(l)           ((u16)(((u32)(l) >> 16) & 0xFFFF))
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#define DWORD u32
#define __int64 long long int

typedef struct RECTTAG
{
    int left;
    int top;
    int right;
    int bottom;
}
RECT;

/////////////////////////////////////////////////////////////////////////////

typedef struct TWINTAG
{
    PSXRect_t   Position;
}
TWin_t;

/////////////////////////////////////////////////////////////////////////////

typedef struct
{
    PSXPoint_t  DisplayModeNew;
    PSXPoint_t  DisplayMode;
    PSXPoint_t  DisplayPosition;

    s32     Double;
    s32     Height;
    s32     PAL;
    s32     InterlacedNew;
    s32     Interlaced;
    s32     RGB24New;
    s32     RGB24;
    PSXSPoint_t DrawOffset;
    s32     Disabled;
    PSXRect_t   Range;
}
PSXDisplay_t;

/////////////////////////////////////////////////////////////////////////////

extern s32          GlobalTextAddrX, GlobalTextAddrY, GlobalTextTP;
extern s32          GlobalTextREST, GlobalTextABR, GlobalTextPAGE;
extern short            ly0, lx0, ly1, lx1, ly2, lx2, ly3, lx3;
extern BOOL             bCheckMask;
extern u16              sSetMask;
extern u32              lSetMask;
extern BOOL             bDeviceOK;
extern short            g_m1;
extern short            g_m2;
extern short            g_m3;
extern short            DrawSemiTrans;
extern int              iUseGammaVal;

// prim.c

extern BOOL             bUsingTWin;
extern TWin_t           TWin;
extern void (*primTableJ[256])(u8 *);
extern void (*primTableSkip[256])(u8 *);
extern u16              usMirror;
extern int              iDither;
extern u32         dwCfgFixes;
extern BOOL             bDoVSyncUpdate;
extern s32          drawX;
extern s32          drawY;
extern s32          drawW;
extern s32          drawH;

// gpu.c

extern VRAMLoad_t           VRAMWrite;
extern VRAMLoad_t           VRAMRead;
extern DATAREGISTERMODES    DataWriteMode;
extern DATAREGISTERMODES    DataReadMode;
extern short                sDispWidths[];
//extern unsigned int   iMaxDMACommandCounter;
//extern unsigned long  dwDMAChainStop;
extern PSXDisplay_t         PSXDisplay;
extern PSXDisplay_t         PreviousPSXDisplay;
extern long                 lGPUstatusRet;
//extern long           drawingLines;
extern u8                   psxVSecure[];
extern u8                   *psxVub;
extern u16                  *psxVuw;
extern u16                  *psxVuw_eom;
extern u32             lGPUInfoVals[];

// zn.c

#define iGPUHeight  512

extern int              iGPUHeightMask;
extern int              GlobalTextIL;

#endif
