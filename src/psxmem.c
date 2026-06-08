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
* PSX memory functions.
*/

#include "psxmem.h"
#include "r3000a.h"
#include "psxhw.h"
#include "system.h"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

s8  psxM[0x00220000]; // Kernel & User Memory (2 Meg)
s8 *psxP = NULL; // Parallel Port (64K)
s8  psxR[0x00080000]; // BIOS ROM (512K)
s8 *psxH = NULL; // Scratch Pad (1K) & Hardware Registers (8K)

u8  *psxMemWLUT[0x10000];
u8  *psxMemRLUT[0x10000];

/*  Playstation Memory Map (from Playstation doc by Joshua Walker)
0x0000_0000-0x0000_ffff     Kernel (64K)
0x0001_0000-0x001f_ffff     User Memory (1.9 Meg)

0x1f00_0000-0x1f00_ffff     Parallel Port (64K)

0x1f80_0000-0x1f80_03ff     Scratch Pad (1024 bytes)

0x1f80_1000-0x1f80_2fff     Hardware Registers (8K)

0x1fc0_0000-0x1fc7_ffff     BIOS (512K)

0x8000_0000-0x801f_ffff     Kernel and User Memory Mirror (2 Meg) Cached
0x9fc0_0000-0x9fc7_ffff     BIOS Mirror (512K) Cached

0xa000_0000-0xa01f_ffff     Kernel and User Memory Mirror (2 Meg) Uncached
0xbfc0_0000-0xbfc7_ffff     BIOS Mirror (512K) Uncached
*/

void Psx_MemInit()
{
    int     size = 0x80000;
    char    *bios = (char *)&psxR;
    int     i;

    memset(psxMemRLUT, 0, 0x10000 * sizeof(u8 *));
    memset(psxMemWLUT, 0, 0x10000 * sizeof(u8 *));

    psxP = &psxM[0x200000];
    psxH = &psxM[0x210000];

// MemR
    for (i = 0; i < 0x80; i++) psxMemRLUT[i + 0x0000] = (u8 *)&psxM[(i & 0x1f) << 16];

    memcpy(psxMemRLUT + 0x8000, psxMemRLUT, 0x80 * sizeof(void *));
    memcpy(psxMemRLUT + 0xa000, psxMemRLUT, 0x80 * sizeof(void *));

    psxMemRLUT[0x1f00] = (u8 *)psxP;
    psxMemRLUT[0x1f80] = (u8 *)psxH;

    for (i = 0; i < 0x08; i++) psxMemRLUT[i + 0x1fc0] = (u8 *)&psxR[i << 16];

    memcpy(psxMemRLUT + 0x9fc0, psxMemRLUT + 0x1fc0, 0x08 * sizeof(void *));
    memcpy(psxMemRLUT + 0xbfc0, psxMemRLUT + 0x1fc0, 0x08 * sizeof(void *));

// MemW
    for (i = 0; i < 0x80; i++) psxMemWLUT[i + 0x0000] = (u8 *)&psxM[(i & 0x1f) << 16];

    memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void *));
    memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void *));

    psxMemWLUT[0x1f00] = (u8 *)psxP;
    psxMemWLUT[0x1f80] = (u8 *)psxH;

    // Load BIOS
    if (File_Load(Config.Bios, &bios, &size) == 0)
    {
        Config.HLE = false;
        SysPrintf("Loaded BIOS: %s\n", Config.Bios);

        // System ROM Version %version %data %region
        // this is found in the BIOS image at 0x7ff32
        // the region ID is at 0x7ff52
        // NOTE: not all bios have a version string
        switch (psxR[0x7ff52])
        {
          case 'J':
            SysPrintf("BIOS detected as NTSC (Japan)\n");
            break;

          case 'A':
            SysPrintf("BIOS detected as NTSC (America)\n");
            break;

          case 'E':
            SysPrintf("BIOS detected as PAL (Europe)\n");
            break;
        }
    }
    else
    {
        SysPrintf("Could not open BIOS:\"%s\". Enabling HLE Bios!\n", Config.Bios);
        memset(psxR, 0, 0x80000);
    }
}

void psxMemReset()
{
    memset(psxM, 0, 0x00200000);
    memset(psxP, 0, 0x00010000);
}

void psxMemShutdown() {
}

static int writeok = 1;

u8 psxMemRead8(u32 mem) {
    char *p;
    u32 t;

    if (!Config.MemHack) {
        psxRegs.cycle += 0;
    }

    t = mem >> 16;
    if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
        if ((mem & 0xffff) < 0x400)
            return psxHu8(mem);
        else
            return psxHwRead8(mem);
    } else {
        p = (char *)(psxMemRLUT[t]);
        if (p != NULL) {
            return *(u8 *)(p + (mem & 0xffff));
        } else {
            return 0;
        }
    }
}

u16 psxMemRead16(u32 mem) {
    char *p;
    u32 t;

    if (!Config.MemHack) {
        psxRegs.cycle += 1;
    }

    t = mem >> 16;
    if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
        if ((mem & 0xffff) < 0x400)
            return psxHu16(mem);
        else
            return psxHwRead16(mem);
    } else {
        p = (char *)(psxMemRLUT[t]);
        if (p != NULL) {
            return SWAPu16(*(u16 *)(p + (mem & 0xffff)));
        } else {
            return 0;
        }
    }
}

u32 psxMemRead32(u32 mem) {
    char *p;
    u32 t;

    if (!Config.MemHack) {
        psxRegs.cycle += 1;
    }

    t = mem >> 16;
    if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
        if ((mem & 0xffff) < 0x400)
            return psxHu32(mem);
        else
            return psxHwRead32(mem);
    } else {
        p = (char *)(psxMemRLUT[t]);
        if (p != NULL) {
            return SWAPu32(*(u32 *)(p + (mem & 0xffff)));
        } else {
            return 0;
        }
    }
}

void psxMemWrite8(u32 mem, u8 value) {
    char *p;
    u32 t;

    if (!Config.MemHack) {
        psxRegs.cycle += 1;
    }

    t = mem >> 16;
    if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
        if ((mem & 0xffff) < 0x400)
            psxHu8(mem) = value;
        else
            psxHwWrite8(mem, value);
    } else {
        p = (char *)(psxMemWLUT[t]);
        if (p != NULL) {
            *(u8 *)(p + (mem & 0xffff)) = value;
        }
    }
}

void psxMemWrite16(u32 mem, u16 value) {
    char *p;
    u32 t;

    if (!Config.MemHack) {
        psxRegs.cycle += 1;
    }

    t = mem >> 16;
    if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
        if ((mem & 0xffff) < 0x400)
            psxHu16ref(mem) = SWAPu16(value);
        else
            psxHwWrite16(mem, value);
    } else {
        p = (char *)(psxMemWLUT[t]);
        if (p != NULL) {
            *(u16 *)(p + (mem & 0xffff)) = SWAPu16(value);
        }
    }
}

void psxMemWrite32(u32 mem, u32 value) {
    char *p;
    u32 t;

    if (!Config.MemHack) {
        psxRegs.cycle += 1;
    }

    //  if ((mem&0x1fffff) == 0x71E18 || value == 0x48088800) SysPrintf("t2fix!!\n");
    t = mem >> 16;
    if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
        if ((mem & 0xffff) < 0x400)
            psxHu32ref(mem) = SWAPu32(value);
        else
            psxHwWrite32(mem, value);
    } else {
        p = (char *)(psxMemWLUT[t]);
        if (p != NULL) {
            *(u32 *)(p + (mem & 0xffff)) = SWAPu32(value);
        } else {
            if (mem != 0xfffe0130) {
                // FIXME was a _LOG entry
            } else {
                int i;

                // a0-44: used for cache flushing
                switch (value) {
                    case 0x800: case 0x804:
                        if (writeok == 0) break;
                        writeok = 0;
                        memset(psxMemWLUT + 0x0000, 0, 0x80 * sizeof(void *));
                        memset(psxMemWLUT + 0x8000, 0, 0x80 * sizeof(void *));
                        memset(psxMemWLUT + 0xa000, 0, 0x80 * sizeof(void *));

                        psxRegs.ICache_valid = false;
                        break;
                    case 0x00: case 0x1e988:
                        if (writeok == 1) break;
                        writeok = 1;
                        for (i = 0; i < 0x80; i++) psxMemWLUT[i + 0x0000] = (void *)&psxM[(i & 0x1f) << 16];
                        memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void *));
                        memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void *));
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

void *psxMemPointer(u32 mem) {
    char *p;
    u32 t;

    t = mem >> 16;
    if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
        if ((mem & 0xffff) < 0x400)
            return (void *)&psxH[mem];
        else
            return NULL;
    } else {
        p = (char *)(psxMemWLUT[t]);
        if (p != NULL) {
            return (void *)(p + (mem & 0xffff));
        }
        return NULL;
    }
}
