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

#include "psxhw.h"
#include "psxdma.h"
#include "system.h"
#include "gpu_externals.h"

#include "gpu.h"

#define GPUSTATUS_ODDLINES            0x80000000
#define GPUSTATUS_DMABITS             0x60000000 // Two bits
#define GPUSTATUS_READYFORCOMMANDS    0x10000000 // DMA block ready
#define GPUSTATUS_READYFORVRAM        0x08000000
#define GPUSTATUS_IDLE                0x04000000 // CMD ready
#define GPUSTATUS_MODE                0x02000000 // Data request mode

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

// Taken from PEOPS SOFTGPU
u32 lUsedAddr[3];

static inline bool CheckForEndlessLoop(u32 laddr) {
    if (laddr == lUsedAddr[1]) return true;
    if (laddr == lUsedAddr[2]) return true;

    if (laddr < lUsedAddr[0]) lUsedAddr[1] = laddr;
    else lUsedAddr[2] = laddr;

    lUsedAddr[0] = laddr;

    return false;
}

static u32 gpuDmaChainSize(u32 addr) {
    u32 size;
    u32 DMACommandCounter = 0;

    lUsedAddr[0] = lUsedAddr[1] = lUsedAddr[2] = 0xffffff;

    // initial linked list ptr (word)
    size = 1;

    do {
        addr &= 0x1ffffc;

        if (DMACommandCounter++ > 2000000) break;
        if (CheckForEndlessLoop(addr)) break;

        // # 32-bit blocks to transfer
        size += psxMu8( addr + 3 );

        // next 32-bit pointer
        addr = psxMu32( addr & ~0x3 ) & 0xffffff;
        size += 1;
    } while (addr != 0xffffff);

    return size;
}

int gpuReadStatus() {
    int hard;

    // GPU plugin
    hard = GPU_ReadStatus();

    // Gameshark Lite - wants to see VRAM busy
    // - Must enable GPU 'Fake Busy States' hack
    if( (hard & GPUSTATUS_IDLE) == 0 )
        hard &= ~GPUSTATUS_READYFORVRAM;

    return hard;
}

void psxDma2(u32 madr, u32 bcr, u32 chcr) { // GPU
    u32 *ptr;
    u32 size, bs;

    switch (chcr) {
        case 0x01000200: // vram2mem
            ptr = (u32 *)PSXM(madr);
            if (ptr == NULL) {
                break;
            }
            // BA blocks * BS words (word = 32-bits)
            size = (bcr >> 16) * (bcr & 0xffff);
            GPU_ReadDataMem(ptr, size);

#if 1
            // already 32-bit word size ((size * 4) / 4)
            GPUDMA_INT(size);
#else
            // Experimental burst dma transfer (0.333x max)
            GPUDMA_INT(size/3);
#endif
            return;

        case 0x01000201: // mem2vram
            bs=(bcr & 0xffff);
            size = (bcr >> 16) * bs; // BA blocks * BS words (word = 32-bits)
            ptr = (u32 *)PSXM(madr);
            if (ptr == NULL) {
                break;
            }
            GPU_WriteDataMem(ptr, size);

#if 0
            // already 32-bit word size ((size * 4) / 4)
            GPUDMA_INT(size);
#else
            // X-Files video interlace. Experimental delay depending of BS.
            GPUDMA_INT( (7 * size) / bs );
#endif
            return;

        case 0x00000401: // Vampire Hunter D: title screen linked list update (see psxhw.c)
        case 0x01000401: // dma chain
            size = gpuDmaChainSize(madr);
            GPU_DmaChain((u32 *)psxM, madr & 0x1fffff);

            // Tekken 3 = use 1.0 only (not 1.5x)

            // Einhander = parse linked list in pieces (todo)
            // Final Fantasy 4 = internal vram time (todo)
            // Rebel Assault 2 = parse linked list in pieces (todo)
            // Vampire Hunter D = allow edits to linked list (todo)
            GPUDMA_INT(size);
            return;

    }

    HW_DMA2_CHCR &= SWAP32(~0x01000000);
    DMA_INTERRUPT(2);
}

void gpuInterrupt() {
    HW_DMA2_CHCR &= SWAP32(~0x01000000);
    DMA_INTERRUPT(2);
}

// -----------------------------------------------------------------------------

#define GPUIsBusy   (lGPUstatusRet &= ~GPUSTATUS_IDLE)
#define GPUIsIdle   (lGPUstatusRet |= GPUSTATUS_IDLE)
#define GPUIsNotReadyForCommands    (lGPUstatusRet &= ~GPUSTATUS_READYFORCOMMANDS)
#define GPUIsReadyForCommands       (lGPUstatusRet |= GPUSTATUS_READYFORCOMMANDS)

////////////////////////////////////////////////////////////////////////
// memory image of the PSX vram
////////////////////////////////////////////////////////////////////////

// always alloc one extra MB for soft drawing funcs security
#define VRAMSIZE    (iGPUHeight * 2) * 1024 + (1024 * 1024)

u8                  psxVSecure[VRAMSIZE];
u8                  *psxVub;
u16                 *psxVuw;
u16                 *psxVuw_eom;

////////////////////////////////////////////////////////////////////////
// GPU globals
////////////////////////////////////////////////////////////////////////

static long         lGPUdataRet;
long                lGPUstatusRet;

static u32          gpuDataM[256];
static u8           gpuCommand = 0;
static long         gpuDataC = 0;
static long         gpuDataP = 0;

VRAMLoad_t          VRAMWrite;
VRAMLoad_t          VRAMRead;
DATAREGISTERMODES   DataWriteMode;
DATAREGISTERMODES   DataReadMode;

BOOL                bSkipNextFrame = FALSE;
short               sDispWidths[8] = {256, 320, 512, 640, 368, 384, 512, 640};
PSXDisplay_t        PSXDisplay;
PSXDisplay_t        PreviousPSXDisplay;
u32            lGPUInfoVals[16];
static int          iFakePrimBusy = 0;
u32            vBlank = 0;
BOOL                oddLines;
//---------------
int                 iGPUHeightMask = 511;
int                 GlobalTextIL = 0;

BOOL                bIsFirstFrame = TRUE;
BOOL                bCheckMask = FALSE;
u16                 sSetMask = 0;
u32                 lSetMask = 0;

////////////////////////////////////////////////////////////////////////
// INIT, will be called after lib load... well, just do some var init...
////////////////////////////////////////////////////////////////////////

void GPU_Open()                                // GPU INIT
{
    //!!! ATTENTION !!!
    psxVub = psxVSecure + 512 * 1024;                           // security offset into double sized psx vram!

    psxVuw = (u16 *)psxVub;

    psxVuw_eom = psxVuw + 1024 * iGPUHeight;                    // pre-calc of end of vram

    memset(psxVSecure, 0x00, VRAMSIZE);
    memset(lGPUInfoVals, 0x00, 16 * sizeof(u32));

    PSXDisplay.RGB24 = FALSE;                      // init some stuff
    PSXDisplay.Interlaced = FALSE;
    PSXDisplay.DrawOffset.x = 0;
    PSXDisplay.DrawOffset.y = 0;
    PSXDisplay.DisplayMode.x = 320;
    PSXDisplay.DisplayMode.y = 240;
    PreviousPSXDisplay.DisplayMode.x = 320;
    PreviousPSXDisplay.DisplayMode.y = 240;
    PSXDisplay.Disabled = FALSE;
    PreviousPSXDisplay.Range.x0 = 0;
    PreviousPSXDisplay.Range.y0 = 0;
    PSXDisplay.Range.x0 = 0;
    PSXDisplay.Range.x1 = 0;
    PreviousPSXDisplay.DisplayModeNew.y = 0;
    PSXDisplay.Double = 1;
    lGPUdataRet = 0x400;

    DataWriteMode = DR_NORMAL;

    // Reset transfer values, to prevent mis-transfer of data
    memset(&VRAMWrite, 0, sizeof(VRAMLoad_t));
    memset(&VRAMRead, 0, sizeof(VRAMLoad_t));

    // device initialised already !
    lGPUstatusRet = 0x14802000;
    GPUIsIdle;
    GPUIsReadyForCommands;
    bDoVSyncUpdate = TRUE;
    vBlank = 0;
    oddLines = FALSE;

    bIsFirstFrame  = TRUE;                                // we have to init later
    bDoVSyncUpdate = TRUE;
}

void GPU_Close()
{
}

////////////////////////////////////////////////////////////////////////
// Update display (swap buffers)
////////////////////////////////////////////////////////////////////////

void updateDisplay()                               // UPDATE DISPLAY
{
    if (PSXDisplay.Disabled)                               // disable?
    {
   //DoClearFrontBuffer();                               // -> clear frontbuffer
        return;                                             // -> and bye
    }

   //DoBufferSwap();                                     // -> swap
    UpdateVideo();
}

////////////////////////////////////////////////////////////////////////
// roughly emulated screen centering bits... not complete !!!
////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsX()                          // X CENTER
{
    long    lx, l;

    if (!PSXDisplay.Range.x1)
    {
        return;
    }

    l = PreviousPSXDisplay.DisplayMode.x;

    l *= (long)PSXDisplay.Range.x1;
    l /= 2560;
    lx = l;
    l &= 0xfffffff8;

    if (l == PreviousPSXDisplay.Range.y1)
    {
        return;            // abusing range.y1 for
    }
    PreviousPSXDisplay.Range.y1 = (short)l;                 // storing last x range and test

    if (lx >= PreviousPSXDisplay.DisplayMode.x)
    {
        PreviousPSXDisplay.Range.x1 = (short)PreviousPSXDisplay.DisplayMode.x;
        PreviousPSXDisplay.Range.x0 = 0;
    }
    else
    {
        PreviousPSXDisplay.Range.x1 = (short)l;

        PreviousPSXDisplay.Range.x0 = (PSXDisplay.Range.x0 - 500) / 8;

        if (PreviousPSXDisplay.Range.x0 < 0)
        {
            PreviousPSXDisplay.Range.x0 = 0;
        }

        if ((PreviousPSXDisplay.Range.x0 + lx) > PreviousPSXDisplay.DisplayMode.x)
        {
            PreviousPSXDisplay.Range.x0 = (short)(PreviousPSXDisplay.DisplayMode.x - lx);
            PreviousPSXDisplay.Range.x0 += 2; //???

            PreviousPSXDisplay.Range.x1 += (short)(lx - l);

            PreviousPSXDisplay.Range.x1 -= 2; // makes linux stretching easier
        }

        // some linux alignment security
        PreviousPSXDisplay.Range.x0 = PreviousPSXDisplay.Range.x0 >> 1;
        PreviousPSXDisplay.Range.x0 = PreviousPSXDisplay.Range.x0 << 1;
        PreviousPSXDisplay.Range.x1 = PreviousPSXDisplay.Range.x1 >> 1;
        PreviousPSXDisplay.Range.x1 = PreviousPSXDisplay.Range.x1 << 1;

   //DoClearScreenBuffer();
    }

    bDoVSyncUpdate = TRUE;
}

////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsY()                          // Y CENTER
{
    int iT, iO = PreviousPSXDisplay.Range.y0;
    int iOldYOffset = PreviousPSXDisplay.DisplayModeNew.y;

// new

    if ((PreviousPSXDisplay.DisplayModeNew.x + PSXDisplay.DisplayModeNew.y) > iGPUHeight)
    {
        int dy1 = iGPUHeight - PreviousPSXDisplay.DisplayModeNew.x;
        int dy2 = (PreviousPSXDisplay.DisplayModeNew.x + PSXDisplay.DisplayModeNew.y) - iGPUHeight;

        if (dy1 >= dy2)
        {
            PreviousPSXDisplay.DisplayModeNew.y =- dy2;
        }
        else
        {
            PSXDisplay.DisplayPosition.y = 0;
            PreviousPSXDisplay.DisplayModeNew.y =- dy1;
        }
    }
    else
    {
        PreviousPSXDisplay.DisplayModeNew.y = 0;
    }

// eon

    if (PreviousPSXDisplay.DisplayModeNew.y != iOldYOffset) // if old offset!=new offset: recalc height
    {
        PSXDisplay.Height = PSXDisplay.Range.y1 - PSXDisplay.Range.y0 + PreviousPSXDisplay.DisplayModeNew.y;
        PSXDisplay.DisplayModeNew.y = PSXDisplay.Height * PSXDisplay.Double;
    }

//

    if (PSXDisplay.PAL)
    {
        iT = 48;
    }
    else
    {
        iT = 28;
    }

    if (PSXDisplay.Range.y0 >= iT)
    {
        PreviousPSXDisplay.Range.y0 = (short)((PSXDisplay.Range.y0 - iT - 4) * PSXDisplay.Double);
        if (PreviousPSXDisplay.Range.y0 < 0)
        {
            PreviousPSXDisplay.Range.y0 = 0;
        }
        PSXDisplay.DisplayModeNew.y += PreviousPSXDisplay.Range.y0;
    }
    else
    {
        PreviousPSXDisplay.Range.y0 = 0;
    }

    if (iO != PreviousPSXDisplay.Range.y0)
    {
   //DoClearScreenBuffer();
    }
}

////////////////////////////////////////////////////////////////////////
// check if update needed
////////////////////////////////////////////////////////////////////////

void updateDisplayIfChanged()                      // UPDATE DISPLAY IF CHANGED
{
    if ((PSXDisplay.DisplayMode.y == PSXDisplay.DisplayModeNew.y) && (PSXDisplay.DisplayMode.x == PSXDisplay.DisplayModeNew.x))
    {
        if ((PSXDisplay.RGB24 == PSXDisplay.RGB24New) && (PSXDisplay.Interlaced == PSXDisplay.InterlacedNew))
        {
            return;
        }
    }

    PSXDisplay.RGB24 = PSXDisplay.RGB24New;       // get new infos

    PSXDisplay.DisplayMode.y = PSXDisplay.DisplayModeNew.y;
    PSXDisplay.DisplayMode.x = PSXDisplay.DisplayModeNew.x;
    PreviousPSXDisplay.DisplayMode.x = min(640, PSXDisplay.DisplayMode.x); // previous will hold max 640x512... that's
    PreviousPSXDisplay.DisplayMode.y = min(512, PSXDisplay.DisplayMode.y); // the size of my back buffer surface
    PSXDisplay.Interlaced = PSXDisplay.InterlacedNew;

    ChangeDispOffsetsX();

    SysSetFrameRate(PSXDisplay.PAL ? 5000 : /*5994*/6000);
}

////////////////////////////////////////////////////////////////////////
// update lace is called evry VSync
////////////////////////////////////////////////////////////////////////

void GPU_UpdateLace()                      // VSYNC
{
    if (!(dwCfgFixes & 1))
    {
        lGPUstatusRet ^= 0x80000000;                           // odd/even bit
    }

    SysWaitTime();

    if (bDoVSyncUpdate)               // some primitives drawn?
    {
        updateDisplay();                                 // -> update display
    }

    bDoVSyncUpdate = FALSE;                                 // vsync done
}

////////////////////////////////////////////////////////////////////////
// process read request from GPU status register
////////////////////////////////////////////////////////////////////////

u32 GPU_ReadStatus()             // READ STATUS
{
    if (vBlank || oddLines == FALSE)
    { // vblank or even lines
        lGPUstatusRet &= ~(0x80000000);
    }
    else
    { // Oddlines and not vblank
        lGPUstatusRet |= 0x80000000;
    }

    if (dwCfgFixes & 1)
    {
        static int  iNumRead = 0;                         // odd/even hack

        if ((iNumRead++) == 2)
        {
            iNumRead = 0;
            lGPUstatusRet ^= 0x80000000;                   // interlaced bit toggle... we do it on every 3 read status... needed by some games (like ChronoCross) with old epsxe versions (1.5.2 and older)
        }
    }

    if (iFakePrimBusy)                                // 27.10.2007 - PETE : emulating some 'busy' while drawing... pfff
    {
        iFakePrimBusy--;

        if (iFakePrimBusy & 1)                            // we do a busy-idle-busy-idle sequence after/while drawing prims
        {
            GPUIsBusy;
            GPUIsNotReadyForCommands;
        }
        else
        {
            GPUIsIdle;
            GPUIsReadyForCommands;
        }
    }
    return lGPUstatusRet;
}

////////////////////////////////////////////////////////////////////////
// processes data send to GPU status register
// these are always single packet commands.
////////////////////////////////////////////////////////////////////////

void GPU_WriteStatus(u32 gdata)      // WRITE STATUS
{
    u32    lCommand = (gdata >> 24) & 0xff;

    switch (lCommand)
    {
    // reset gpu
      case 0x00:
        memset(lGPUInfoVals, 0x00, 16 * sizeof(u32));
        lGPUstatusRet = 0x14802000;
        PSXDisplay.Disabled = 1;
        DataWriteMode = DataReadMode = DR_NORMAL;
        PSXDisplay.DrawOffset.x = PSXDisplay.DrawOffset.y = 0;
        drawX = drawY = 0;
        drawW = drawH = 0;
        sSetMask = 0;
        lSetMask = 0;
        bCheckMask = FALSE;
        usMirror = 0;
        GlobalTextAddrX = 0;
        GlobalTextAddrY = 0;
        GlobalTextTP = 0;
        GlobalTextABR = 0;
        PSXDisplay.RGB24 = FALSE;
        PSXDisplay.Interlaced = FALSE;
        bUsingTWin = FALSE;
        return;

    // dis/enable display
      case 0x03:
        PSXDisplay.Disabled = (gdata & 1);

        if (PSXDisplay.Disabled)
        {
            lGPUstatusRet |= GPUSTATUS_DISPLAYDISABLED;
        }
        else
        {
            lGPUstatusRet &= ~GPUSTATUS_DISPLAYDISABLED;
        }
        return;

    // setting transfer mode
      case 0x04:
        gdata &= 0x03;                                     // Only want the lower two bits

        DataWriteMode = DataReadMode = DR_NORMAL;
        if (gdata == 0x02)
        {
            DataWriteMode = DR_VRAMTRANSFER;
        }
        if (gdata == 0x03)
        {
            DataReadMode = DR_VRAMTRANSFER;
        }
        lGPUstatusRet &= ~GPUSTATUS_DMABITS;                 // Clear the current settings of the DMA bits
        lGPUstatusRet |= (gdata << 29);                      // Set the DMA bits according to the received data
        return;

    // setting display position
      case 0x05:
        PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
        PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;

/*
     PSXDisplay.DisplayPosition.y = (short)((gdata>>10)&0x3ff);
     if (PSXDisplay.DisplayPosition.y & 0x200)
      PSXDisplay.DisplayPosition.y |= 0xfffffc00;
     if(PSXDisplay.DisplayPosition.y<0)
      {
       PreviousPSXDisplay.DisplayModeNew.y=PSXDisplay.DisplayPosition.y/PSXDisplay.Double;
       PSXDisplay.DisplayPosition.y=0;
      }
     else PreviousPSXDisplay.DisplayModeNew.y=0;
*/

// new
#if 0 // iGPUHeight == 1024
            //PSXDisplay.DisplayPosition.y = (short)((gdata >> 12) & 0x3ff); // gpu version 2
            PSXDisplay.DisplayPosition.y = (short)((gdata >> 10) & 0x3ff);
#endif
            PSXDisplay.DisplayPosition.y = (short)((gdata >> 10) & 0x1ff);

        // store the same val in some helper var, we need it on later compares
        PreviousPSXDisplay.DisplayModeNew.x = PSXDisplay.DisplayPosition.y;

        if ((PSXDisplay.DisplayPosition.y + PSXDisplay.DisplayMode.y) > iGPUHeight)
        {
            int dy1 = iGPUHeight - PSXDisplay.DisplayPosition.y;
            int dy2 = (PSXDisplay.DisplayPosition.y + PSXDisplay.DisplayMode.y) - iGPUHeight;

            if (dy1 >= dy2)
            {
                PreviousPSXDisplay.DisplayModeNew.y = -dy2;
            }
            else
            {
                PSXDisplay.DisplayPosition.y = 0;
                PreviousPSXDisplay.DisplayModeNew.y = -dy1;
            }
        }
        else
        {
            PreviousPSXDisplay.DisplayModeNew.y = 0;
        }
// eon

        PSXDisplay.DisplayPosition.x = (short)(gdata & 0x3ff);

        bDoVSyncUpdate = TRUE;

        return;

    // setting width
      case 0x06:
        PSXDisplay.Range.x0 = (short)(gdata & 0x7ff);
        PSXDisplay.Range.x1 = (short)((gdata >> 12) & 0xfff);

        PSXDisplay.Range.x1 -= PSXDisplay.Range.x0;

        ChangeDispOffsetsX();
        return;

    // setting height
      case 0x07:
        PSXDisplay.Range.y0 = (short)(gdata & 0x3ff);
        PSXDisplay.Range.y1 = (short)((gdata >> 10) & 0x3ff);

        PreviousPSXDisplay.Height = PSXDisplay.Height;

        PSXDisplay.Height = PSXDisplay.Range.y1 - PSXDisplay.Range.y0 + PreviousPSXDisplay.DisplayModeNew.y;

        if (PreviousPSXDisplay.Height != PSXDisplay.Height)
        {
            PSXDisplay.DisplayModeNew.y = PSXDisplay.Height * PSXDisplay.Double;

            ChangeDispOffsetsY();

            updateDisplayIfChanged();
        }
        return;

    // setting display infos
      case 0x08:
        PSXDisplay.DisplayModeNew.x = sDispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];

        if (gdata & 0x04)
        {
            PSXDisplay.Double = 2;
        }
        else
        {
            PSXDisplay.Double = 1;
        }

        PSXDisplay.DisplayModeNew.y = PSXDisplay.Height * PSXDisplay.Double;

        ChangeDispOffsetsY();

        PSXDisplay.PAL = (gdata & 0x08) ? TRUE : FALSE; // if 1 - PAL mode, else NTSC
        PSXDisplay.RGB24New = (gdata & 0x10) ? TRUE : FALSE; // if 1 - TrueColor
        PSXDisplay.InterlacedNew = (gdata & 0x20) ? TRUE : FALSE; // if 1 - Interlace

        lGPUstatusRet &= ~GPUSTATUS_WIDTHBITS;                   // Clear the width bits
        lGPUstatusRet |= (((gdata & 0x03) << 17) | ((gdata & 0x40) << 10));                // Set the width bits

        if (PSXDisplay.InterlacedNew)
        {
            if (!PSXDisplay.Interlaced)
            {
                PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
                PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
            }
            lGPUstatusRet |= GPUSTATUS_INTERLACED;
        }
        else
        {
            lGPUstatusRet &= ~GPUSTATUS_INTERLACED;
        }

        if (PSXDisplay.PAL)
        {
            lGPUstatusRet |= GPUSTATUS_PAL;
        }
        else
        {
            lGPUstatusRet &= ~GPUSTATUS_PAL;
        }

        if (PSXDisplay.Double == 2)
        {
            lGPUstatusRet |= GPUSTATUS_DOUBLEHEIGHT;
        }
        else
        {
            lGPUstatusRet &= ~GPUSTATUS_DOUBLEHEIGHT;
        }

        if (PSXDisplay.RGB24New)
        {
            lGPUstatusRet |= GPUSTATUS_RGB24;
        }
        else
        {
            lGPUstatusRet &= ~GPUSTATUS_RGB24;
        }

        updateDisplayIfChanged();

        WindowSize(PSXDisplay.DisplayMode.x, PSXDisplay.DisplayMode.y);
        return;

    // ask about GPU version and other stuff
      case 0x10:
        gdata &= 0xff;

        switch (gdata)
        {
          case 0x02:
            lGPUdataRet = lGPUInfoVals[INFO_TW];              // tw infos
            return;

          case 0x03:
            lGPUdataRet = lGPUInfoVals[INFO_DRAWSTART];       // draw start
            return;

          case 0x04:
            lGPUdataRet = lGPUInfoVals[INFO_DRAWEND];         // draw end
            return;

          case 0x05:
          case 0x06:
            lGPUdataRet = lGPUInfoVals[INFO_DRAWOFF];         // draw offset
            return;

          case 0x07:
            //lGPUdataRet = 0x01; // gpu version 2
            lGPUdataRet = 0x02;                          // gpu type
            return;

          case 0x08:
          case 0x0F:                                       // some bios addr?
            lGPUdataRet = 0xBFC03720;
            return;
        }
        return;
    }
}

////////////////////////////////////////////////////////////////////////
// vram read/write helpers, needed by LEWPY's optimized vram read/write :)
////////////////////////////////////////////////////////////////////////

static void FinishedVRAMWrite()
{
    // Set register to NORMAL operation
    DataWriteMode = DR_NORMAL;
    // Reset transfer values, to prevent mis-transfer of data
    VRAMWrite.x = 0;
    VRAMWrite.y = 0;
    VRAMWrite.Width = 0;
    VRAMWrite.Height = 0;
    VRAMWrite.ColsRemaining = 0;
    VRAMWrite.RowsRemaining = 0;
}

static void FinishedVRAMRead()
{
    // Set register to NORMAL operation
    DataReadMode = DR_NORMAL;
    // Reset transfer values, to prevent mis-transfer of data
    VRAMRead.x = 0;
    VRAMRead.y = 0;
    VRAMRead.Width = 0;
    VRAMRead.Height = 0;
    VRAMRead.ColsRemaining = 0;
    VRAMRead.RowsRemaining = 0;

    // Indicate GPU is no longer ready for VRAM data in the STATUS REGISTER
    lGPUstatusRet &= ~GPUSTATUS_READYFORVRAM;
}

////////////////////////////////////////////////////////////////////////
// core read from vram
////////////////////////////////////////////////////////////////////////

void GPU_ReadDataMem(u32 *pMem, int iSize)
{
    int i;

    if (DataReadMode != DR_VRAMTRANSFER)
    {
        return;
    }

    GPUIsBusy;

    // adjust read ptr, if necessary
    while (VRAMRead.ImagePtr >= psxVuw_eom)
    {
        VRAMRead.ImagePtr -= iGPUHeight * 1024;
    }
    while (VRAMRead.ImagePtr < psxVuw)
    {
        VRAMRead.ImagePtr += iGPUHeight * 1024;
    }

    for (i = 0; i < iSize; i++)
    {
        // do 2 seperate 16bit reads for compatibility (wrap issues)
        if ((VRAMRead.ColsRemaining > 0) && (VRAMRead.RowsRemaining > 0))
        {
            // lower 16 bit
            lGPUdataRet = (u32)GETLE16(VRAMRead.ImagePtr);

            VRAMRead.ImagePtr++;
            if (VRAMRead.ImagePtr >= psxVuw_eom)
            {
                VRAMRead.ImagePtr -= iGPUHeight * 1024;
            }
            VRAMRead.RowsRemaining--;

            if (VRAMRead.RowsRemaining <= 0)
            {
                VRAMRead.RowsRemaining = VRAMRead.Width;
                VRAMRead.ColsRemaining--;
                VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
                if (VRAMRead.ImagePtr >= psxVuw_eom)
                {
                    VRAMRead.ImagePtr -= iGPUHeight * 1024;
                }
            }

            // higher 16 bit (always, even if it's an odd width)
            lGPUdataRet |= (u32)GETLE16(VRAMRead.ImagePtr) << 16;
            PUTLE32(pMem, lGPUdataRet);
            pMem++;

            if (VRAMRead.ColsRemaining <= 0)
            {
                FinishedVRAMRead();
                goto ENDREAD;
            }

            VRAMRead.ImagePtr++;
            if (VRAMRead.ImagePtr >= psxVuw_eom)
            {
                VRAMRead.ImagePtr -= iGPUHeight * 1024;
            }
            VRAMRead.RowsRemaining--;
            if (VRAMRead.RowsRemaining <= 0)
            {
                VRAMRead.RowsRemaining = VRAMRead.Width;
                VRAMRead.ColsRemaining--;
                VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
                if (VRAMRead.ImagePtr >= psxVuw_eom)
                {
                    VRAMRead.ImagePtr -= iGPUHeight * 1024;
                }
            }
            if (VRAMRead.ColsRemaining <= 0)
            {
                FinishedVRAMRead();
                goto ENDREAD;
            }
        }
        else
        {
            FinishedVRAMRead();
            goto ENDREAD;
        }
    }

ENDREAD:
    GPUIsIdle;
}

////////////////////////////////////////////////////////////////////////

u32 GPU_ReadData()
{
    u32    l;

    GPU_ReadDataMem(&l, 1);
    return lGPUdataRet;
}

////////////////////////////////////////////////////////////////////////
// processes data send to GPU data register
// extra table entries for fixing polyline troubles
////////////////////////////////////////////////////////////////////////

const u8    primTableCX[256] =
{
    // 00
    0,0,3,0,0,0,0,0,
    // 08
    0,0,0,0,0,0,0,0,
    // 10
    0,0,0,0,0,0,0,0,
    // 18
    0,0,0,0,0,0,0,0,
    // 20
    4,4,4,4,7,7,7,7,
    // 28
    5,5,5,5,9,9,9,9,
    // 30
    6,6,6,6,9,9,9,9,
    // 38
    8,8,8,8,12,12,12,12,
    // 40
    3,3,3,3,0,0,0,0,
    // 48
//  5,5,5,5,6,6,6,6,    // FLINE
    254,254,254,254,254,254,254,254,
    // 50
    4,4,4,4,0,0,0,0,
    // 58
//  7,7,7,7,9,9,9,9,    // GLINE
    255,255,255,255,255,255,255,255,
    // 60
    3,3,3,3,4,4,4,4,
    // 68
    2,2,2,2,3,3,3,3,    // 3=SPRITE1???
    // 70
    2,2,2,2,3,3,3,3,
    // 78
    2,2,2,2,3,3,3,3,
    // 80
    4,0,0,0,0,0,0,0,
    // 88
    0,0,0,0,0,0,0,0,
    // 90
    0,0,0,0,0,0,0,0,
    // 98
    0,0,0,0,0,0,0,0,
    // a0
    3,0,0,0,0,0,0,0,
    // a8
    0,0,0,0,0,0,0,0,
    // b0
    0,0,0,0,0,0,0,0,
    // b8
    0,0,0,0,0,0,0,0,
    // c0
    3,0,0,0,0,0,0,0,
    // c8
    0,0,0,0,0,0,0,0,
    // d0
    0,0,0,0,0,0,0,0,
    // d8
    0,0,0,0,0,0,0,0,
    // e0
    0,1,1,1,1,1,1,0,
    // e8
    0,0,0,0,0,0,0,0,
    // f0
    0,0,0,0,0,0,0,0,
    // f8
    0,0,0,0,0,0,0,0
};

void GPU_WriteDataMem(u32 *pMem, int iSize)
{
    u8      command;
    u32        gdata = 0;
    int             i = 0;

    GPUIsBusy;
    GPUIsNotReadyForCommands;

STARTVRAM:

    if (DataWriteMode == DR_VRAMTRANSFER)
    {
        BOOL    bFinished = FALSE;

        // make sure we are in vram
        while (VRAMWrite.ImagePtr >= psxVuw_eom)
        {
            VRAMWrite.ImagePtr -= iGPUHeight * 1024;
        }
        while (VRAMWrite.ImagePtr < psxVuw)
        {
            VRAMWrite.ImagePtr += iGPUHeight * 1024;
        }

        // now do the loop
        while (VRAMWrite.ColsRemaining > 0)
        {
            while (VRAMWrite.RowsRemaining > 0)
            {
                if (i >= iSize)
                {
                    goto ENDVRAM;
                }
                i++;

                gdata = GETLE32(pMem);
                pMem++;

                // Write odd pixel - Wrap from beginning to next index if going past GPU width
                if (VRAMWrite.Width + VRAMWrite.x - VRAMWrite.RowsRemaining >= 1024)
                {
                    PUTLE16(VRAMWrite.ImagePtr - 1024, (u16)gdata);
                    VRAMWrite.ImagePtr++;
                }
                else
                {
                    PUTLE16(VRAMWrite.ImagePtr, (u16)gdata);
                    VRAMWrite.ImagePtr++;
                }
                if (VRAMWrite.ImagePtr >= psxVuw_eom)
                {
                    VRAMWrite.ImagePtr -= iGPUHeight * 1024; // Check if went past framebuffer
                }
                VRAMWrite.RowsRemaining--;

                // Check if end at odd pixel drawn
                if (VRAMWrite.RowsRemaining <= 0)
                {
                    VRAMWrite.ColsRemaining--;
                    if (VRAMWrite.ColsRemaining <= 0) // last pixel is odd width
                    {
                        gdata = (gdata & 0xFFFF) | (((u32)GETLE16(VRAMWrite.ImagePtr)) << 16);
                        FinishedVRAMWrite();
                        bDoVSyncUpdate = TRUE;
                        goto ENDVRAM;
                    }
                    VRAMWrite.RowsRemaining = VRAMWrite.Width;
                    VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
                }

                // Write even pixel - Wrap from beginning to next index if going past GPU width
                if (VRAMWrite.Width + VRAMWrite.x - VRAMWrite.RowsRemaining >= 1024)
                {
                    PUTLE16(VRAMWrite.ImagePtr - 1024, (u16)(gdata >> 16));
                    VRAMWrite.ImagePtr++;
                }
                else
                {
                    PUTLE16(VRAMWrite.ImagePtr, (u16)(gdata >> 16));
                    VRAMWrite.ImagePtr++;
                }
                if (VRAMWrite.ImagePtr >= psxVuw_eom)
                {
                    VRAMWrite.ImagePtr -= iGPUHeight * 1024; // Check if went past framebuffer
                }
                VRAMWrite.RowsRemaining--;
            }

            VRAMWrite.RowsRemaining = VRAMWrite.Width;
            VRAMWrite.ColsRemaining--;
            VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
            bFinished = TRUE;
        }

        FinishedVRAMWrite();
        if (bFinished)
        {
            bDoVSyncUpdate = TRUE;
        }
    }

ENDVRAM:

    if (DataWriteMode == DR_NORMAL)
    {
        void (* *primFunc)(u8 *);

        if (bSkipNextFrame)
        {
            primFunc = primTableSkip;
        }
        else
        {
            primFunc = primTableJ;
        }

        for ( ; i < iSize;)
        {
            if (DataWriteMode == DR_VRAMTRANSFER)
            {
                goto STARTVRAM;
            }

            gdata = GETLE32(pMem);
            pMem++;
            i++;

            if (gpuDataC == 0)
            {
                command = (u8)((gdata >> 24) & 0xff);

//if(command>=0xb0 && command<0xc0) auxprintf("b0 %x!!!!!!!!!\n",command);

                if (primTableCX[command])
                {
                    gpuDataC = primTableCX[command];
                    gpuCommand = command;
                    PUTLE32(&gpuDataM[0], gdata);
                    gpuDataP = 1;
                }
                else
                {
                    continue;
                }
            }
            else
            {
                PUTLE32(&gpuDataM[gpuDataP], gdata);
                if (gpuDataC > 128)
                {
                    if ((gpuDataC == 254 && gpuDataP >= 3) || (gpuDataC == 255 && gpuDataP >= 4 && !(gpuDataP & 1)))
                    {
                        if ((gpuDataM[gpuDataP] & 0xF000F000) == 0x50005000)
                        {
                            gpuDataP = gpuDataC - 1;
                        }
                    }
                }
                gpuDataP++;
            }

            if (gpuDataP == gpuDataC)
            {
                gpuDataC = gpuDataP = 0;
                primFunc[gpuCommand]((u8 *)gpuDataM);
                if (dwCfgFixes & 0x0400) // hack for emulating "gpu busy" in some games
                {
                    iFakePrimBusy = 4;
                }
            }
        }
    }

    lGPUdataRet = gdata;

    GPUIsReadyForCommands;
    GPUIsIdle;
}

////////////////////////////////////////////////////////////////////////

void GPU_WriteData(u32 gdata)
{
    PUTLE32(&gdata, gdata);
    GPU_WriteDataMem(&gdata, 1);
}

////////////////////////////////////////////////////////////////////////
// process gpu commands
////////////////////////////////////////////////////////////////////////
/*
extern unsigned long lUsedAddr[3];

static BOOL CheckForEndlessLoop(unsigned long laddr)
{
    if (laddr == lUsedAddr[1])
    {
        return TRUE;
    }
    if (laddr == lUsedAddr[2])
    {
        return TRUE;
    }

    if (laddr < lUsedAddr[0])
    {
        lUsedAddr[1] = laddr;
    }
    else
    {
        lUsedAddr[2] = laddr;
    }
    lUsedAddr[0] = laddr;
    return FALSE;
}
*/
long GPU_DmaChain(u32 *baseAddrL, u32 addr)
{
    u32        dmaMem;
    u8      *baseAddrB;
    short           count;
    u32     DMACommandCounter = 0;

    GPUIsBusy;

    lUsedAddr[0] = lUsedAddr[1] = lUsedAddr[2] = 0xffffff;

    baseAddrB = (u8 *)baseAddrL;

    do
    {
        if (iGPUHeight == 512)
        {
            addr&=0x1FFFFC;
        }
    if (DMACommandCounter++ > 2000000)
    {
        break;
    }
    if (CheckForEndlessLoop(addr))
    {
        break;
    }

    count = baseAddrB[addr + 3];

    dmaMem = addr + 4;

    if (count > 0)
    {
        GPU_WriteDataMem(&baseAddrL[dmaMem >> 2], count);
    }

        addr = GETLE32(&baseAddrL[addr >> 2]) & 0xffffff;
    }
    while (addr != 0xffffff);

    GPUIsIdle;

    return 0;
}

void GPU_VBlank(int val)
{
    vBlank = val;
    oddLines = oddLines ? FALSE : TRUE; // bit changes per frame when not interlaced
 //printf("VB %x (%x)\n", oddLines, vBlank);
}

void GPU_HSync(int val)
{
    // Interlaced mode - update bit every scanline
    if (PSXDisplay.Interlaced)
    {
        oddLines = (val % 2 ? FALSE : TRUE);
    }
 //printf("HS %x (%x)\n", oddLines, vBlank);
}

void DoBufferSwap(u32 *pixels)
{
    int     x = PSXDisplay.DisplayPosition.x;
    int     y = PSXDisplay.DisplayPosition.y;
    u16     dx = PreviousPSXDisplay.Range.x1;
    u16     dy = PreviousPSXDisplay.DisplayMode.y;
    int     row, column;
    int     startxy;
    u16     *v16;
    u8      *vRGB;
    u16     s;

    // is centering needed on an emulator? we'll assume not ...

#if 0
    if (PreviousPSXDisplay.Range.y0) // centering needed?
    {
        memset(surf, 0, (PreviousPSXDisplay.Range.y0 >> 1) * lPitch);

        dy -= PreviousPSXDisplay.Range.y0;
        surf += (PreviousPSXDisplay.Range.y0 >> 1) * lPitch;

        memset(surf + dy * lPitch, 0, ((PreviousPSXDisplay.Range.y0 + 1) >> 1) * lPitch);
    }

    if (PreviousPSXDisplay.Range.x0)
    {
        for (column = 0; column < dy; column++)
        {
            destpix = (u32 *)(surf + (column * lPitch));
            memset(destpix, 0, PreviousPSXDisplay.Range.x0 << 2);
        }
        surf += PreviousPSXDisplay.Range.x0 << 2;
    }
#endif

    if (PSXDisplay.RGB24)
    {
        startxy = 1024 * y + x;
        for (column = 0; column < dy; column++, startxy += 1024, pixels += PSXWIDTH)
        {
            vRGB = (u8 *)(psxVuw + startxy);
            for (row = 0; row < dx; row++, vRGB += 3)
            {
                *(pixels + row) = *(u32 *)vRGB;
            }
        }
    }
    else
    {
        startxy = 1024 * y + x;
        for (column = 0; column < dy; column++, startxy += 1024, pixels += PSXWIDTH)
        {
            v16 = psxVuw + startxy;
            for (row = 0; row < dx; row++, v16++)
            {
                s = GETLE16(v16);
                *(pixels + row) = (((s << 3) & 0xf8) | ((s << 6) & 0xf800) | ((s << 9) & 0xf80000));
            }
        }
    }
}
