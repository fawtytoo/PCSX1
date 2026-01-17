/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

/*
* Functions for PSX hardware control.
*/

#include "psxhw.h"
#include "mdec.h"
#include "cdrom.h"
#include "gpu.h"
#include "spu.h"

// Vampire Hunter D hack
bool    dmaGpuListHackEn = false;

static inline
void setIrq( u32 irq )
{
    psxHu32ref(0x1070) |= SWAPu32(irq);
}

void psxHwReset() {
    if (Config.SioIrq) psxHu32ref(0x1070) |= SWAP32(0x80);
    if (Config.SpuIrq) psxHu32ref(0x1070) |= SWAP32(0x200);

    memset(psxH, 0, 0x10000);

    mdecInit(); // initialise mdec decoder
    cdrReset();
    psxRcntInit();
}

u8 psxHwRead8(u32 add) {
    u8      hard;

    switch (add) {
        case 0x1f801040: hard = sioRead8(); break; // memory cards
#ifdef ENABLE_NET
        case 0x1f801050: hard = SIO1_readData8(); break;
#endif
        case 0x1f801800: hard = cdrRead0(); break;
        case 0x1f801801: hard = cdrRead1(); break;
        case 0x1f801802: hard = cdrRead2(); break;
        case 0x1f801803: hard = cdrRead3(); break;
        default:
            hard = psxHu8(add);
            return hard;
    }

    return hard;
}

u16 psxHwRead16(u32 add) {
    u16     hard;

    switch (add) {
        case 0x1f801040:
            hard = sioRead8();
            hard|= sioRead8() << 8;
            return hard;
        case 0x1f801044:
            hard = sioReadStat16();
            return hard;
        case 0x1f801048:
            hard = sioReadMode16();
            return hard;
        case 0x1f80104a:
            hard = sioReadCtrl16();
            return hard;
        case 0x1f80104e:
            hard = sioReadBaud16();
            return hard;
#ifdef ENABLE_NET
        case 0x1f801050:
            hard = SIO1_readData16();
            return hard;
        case 0x1f801054:
            hard = SIO1_readStat16();
            return hard;
        case 0x1f801058:
            hard = SIO1_readMode16();
            return hard;
        case 0x1f80105a:
            hard = SIO1_readCtrl16();
            return hard;
        case 0x1f80105e:
            hard = SIO1_readBaud16();
            return hard;
#endif
        case 0x1f801100:
            hard = psxRcntRcount(0);
            return hard;
        case 0x1f801104:
            hard = psxRcntRmode(0);
            return hard;
        case 0x1f801108:
            hard = psxRcntRtarget(0);
            return hard;
        case 0x1f801110:
            hard = psxRcntRcount(1);
            return hard;
        case 0x1f801114:
            hard = psxRcntRmode(1);
            return hard;
        case 0x1f801118:
            hard = psxRcntRtarget(1);
            return hard;
        case 0x1f801120:
            hard = psxRcntRcount(2);
            return hard;
        case 0x1f801124:
            hard = psxRcntRmode(2);
            return hard;
        case 0x1f801128:
            hard = psxRcntRtarget(2);
            return hard;

        //case 0x1f802030: hard =   //int_2000????
        //case 0x1f802040: hard =//dip switches...??

        default:
            if (add >= 0x1f801c00 && add < 0x1f801e00) {
                hard = SPU_ReadRegister(add);
            } else {
                hard = psxHu16(add);
            }
            return hard;
    }

    return hard;
}

u32 psxHwRead32(u32 add) {
    u32 hard;

    switch (add) {
        case 0x1f801040:
            hard = sioRead8();
            hard |= sioRead8() << 8;
            hard |= sioRead8() << 16;
            hard |= sioRead8() << 24;
            return hard;
#ifdef ENABLE_NET
        case 0x1f801050:
            hard = SIO1_readData32();
            return hard;
#endif
        case 0x1f801810:
            hard = GPU_ReadData();
            return hard;
        case 0x1f801814:
            hard = gpuReadStatus();
            return hard;

        case 0x1f801820: hard = mdecRead0(); break;
        case 0x1f801824: hard = mdecRead1(); break;

        // time for rootcounters :)
        case 0x1f801100:
            hard = psxRcntRcount(0);
            return hard;
        case 0x1f801104:
            hard = psxRcntRmode(0);
            return hard;
        case 0x1f801108:
            hard = psxRcntRtarget(0);
            return hard;
        case 0x1f801110:
            hard = psxRcntRcount(1);
            return hard;
        case 0x1f801114:
            hard = psxRcntRmode(1);
            return hard;
        case 0x1f801118:
            hard = psxRcntRtarget(1);
            return hard;
        case 0x1f801120:
            hard = psxRcntRcount(2);
            return hard;
        case 0x1f801124:
            hard = psxRcntRmode(2);
            return hard;
        case 0x1f801128:
            hard = psxRcntRtarget(2);
            return hard;
        case 0x1f801014:
            hard = psxHu32(add);
            return hard;

        default:
            hard = psxHu32(add);
            return hard;
    }
    return hard;
}

void psxHwWrite8(u32 add, u8 value) {
    switch (add) {
        case 0x1f801040: sioWrite8(value); break;
#ifdef ENABLE_NET
        case 0x1f801050: SIO1_writeData8(value); break;
#endif
        case 0x1f801800: cdrWrite0(value); break;
        case 0x1f801801: cdrWrite1(value); break;
        case 0x1f801802: cdrWrite2(value); break;
        case 0x1f801803: cdrWrite3(value); break;

        default:
            psxHu8ref(add) = value;
            return;
    }
    psxHu8ref(add) = value;
}

void psxHwWrite16(u32 add, u16 value) {
    switch (add) {
        case 0x1f801040:
            sioWrite8((u8)value);
            sioWrite8((u8)(value>>8));
            return;
        case 0x1f801044:
            sioWriteStat16(value);
            return;
        case 0x1f801048:
            sioWriteMode16(value);
            return;
        case 0x1f80104a: // control register
            sioWriteCtrl16(value);
            return;
        case 0x1f80104e: // baudrate register
            sioWriteBaud16(value);
            return;
#ifdef ENABLE_NET
        case 0x1f801050:
            SIO1_writeData16(value);
            return;
        case 0x1f801054:
            SIO1_writeStat16(value);
            return;
        case 0x1f801058:
            SIO1_writeMode16(value);
            return;
        case 0x1f80105a:
            SIO1_writeCtrl16(value);
            return;
        case 0x1f80105e:
            SIO1_writeBaud16(value);
            return;
#endif
        case 0x1f801070:
            if (Config.SioIrq) psxHu16ref(0x1070) |= SWAPu16(0x80);
            if (Config.SpuIrq) psxHu16ref(0x1070) |= SWAPu16(0x200);
            psxHu16ref(0x1070) &= SWAPu16(value);
            return;

        case 0x1f801074:
            psxHu16ref(0x1074) = SWAPu16(value);
            return;

        case 0x1f801100:
            psxRcntWcount(0, value); return;
        case 0x1f801104:
            psxRcntWmode(0, value); return;
        case 0x1f801108:
            psxRcntWtarget(0, value); return;

        case 0x1f801110:
            psxRcntWcount(1, value); return;
        case 0x1f801114:
            psxRcntWmode(1, value); return;
        case 0x1f801118:
            psxRcntWtarget(1, value); return;

        case 0x1f801120:
            psxRcntWcount(2, value); return;
        case 0x1f801124:
            psxRcntWmode(2, value); return;
        case 0x1f801128:
            psxRcntWtarget(2, value); return;

        default:
            if (add>=0x1f801c00 && add<0x1f801e00) {
                SPU_WriteRegister(add, value);
                return;
            }

            psxHu16ref(add) = SWAPu16(value);
            return;
    }
    psxHu16ref(add) = SWAPu16(value);
}

#define DmaExec(n) { \
    HW_DMA##n##_CHCR = SWAPu32(value); \
\
    if (SWAPu32(HW_DMA##n##_CHCR) & 0x01000000 && SWAPu32(HW_DMA_PCR) & (8 << (n * 4))) { \
        psxDma##n(SWAPu32(HW_DMA##n##_MADR), SWAPu32(HW_DMA##n##_BCR), SWAPu32(HW_DMA##n##_CHCR)); \
    } \
}

void psxHwWrite32(u32 add, u32 value) {
    switch (add) {
        case 0x1f801040:
            sioWrite8((u8)value);
            sioWrite8((u8)((value&0xff) >>  8));
            sioWrite8((u8)((value&0xff) >> 16));
            sioWrite8((u8)((value&0xff) >> 24));
            return;
#ifdef ENABLE_NET
        case 0x1f801050:
            SIO1_writeData32(value);
            return;
#endif

        case 0x1f801070:
            if (Config.SioIrq) psxHu32ref(0x1070) |= SWAPu32(0x80);
            if (Config.SpuIrq) psxHu32ref(0x1070) |= SWAPu32(0x200);
            psxHu32ref(0x1070) &= SWAPu32(value);
            return;
        case 0x1f801074:
            psxHu32ref(0x1074) = SWAPu32(value);
            return;

        case 0x1f801088:
            DmaExec(0);                  // DMA0 chcr (MDEC in DMA)
            return;

        case 0x1f801098:
            DmaExec(1);                  // DMA1 chcr (MDEC out DMA)
            return;

        case 0x1f8010a8:
            /* A hack that makes Vampire Hunter D title screen visible,
             * but makes Tomb Raider II water effect to stay opaque
             * Root cause for this problem is that when DMA2 is issued
             * it is incompletele and still beign built by the game.
             * Maybe it is ready when some signal comes in or within given delay?
             */
            if (dmaGpuListHackEn && value == 0x00000401 && HW_DMA2_BCR == 0x0) {
                psxDma2(SWAPu32(HW_DMA2_MADR), SWAPu32(HW_DMA2_BCR), SWAPu32(value));
                return;
            }
            DmaExec(2);                  // DMA2 chcr (GPU DMA)
            if (Config.HackFix && HW_DMA2_CHCR == 0x1000401)
                dmaGpuListHackEn = true;
            return;

        case 0x1f8010b8:
            DmaExec(3);                  // DMA3 chcr (CDROM DMA)

            return;

        case 0x1f8010c8:
            DmaExec(4);                  // DMA4 chcr (SPU DMA)
            return;

#if 0
        case 0x1f8010d0: break; //DMA5write_madr();
        case 0x1f8010d4: break; //DMA5write_bcr();
        case 0x1f8010d8: break; //DMA5write_chcr(); // Not needed
#endif

        case 0x1f8010e8:
            DmaExec(6);                   // DMA6 chcr (OT clear)
            return;

        case 0x1f8010f4:
        {
            u32 tmp = (~value) & SWAPu32(HW_DMA_ICR);
            HW_DMA_ICR = SWAPu32(((tmp ^ value) & 0xffffff) ^ tmp);
            return;
        }

        case 0x1f801014:
            psxHu32ref(add) = SWAPu32(value);
            return;
        case 0x1f801810:
            // 0x1F means irq request, so fulfill it here because plugin can't and won't
            // Probably no need to send this to plugin in first place...
            // MML/Tronbonne is known to use this.
            // TODO FIFO is not implemented properly so commands are not exact
            // and thus we rely on hack that counter/cdrom irqs are enabled at same time
            if (Config.HackFix && SWAPu32(value) == 0x1f00000 && (psxHu32ref(0x1070) & 0x44)) {
                setIrq( 0x01 );
            }
            GPU_WriteData(value); return;
        case 0x1f801814:
            if (value & 0x8000000)
                dmaGpuListHackEn = false;
            GPU_WriteStatus(value); return;

        case 0x1f801820:
            mdecWrite0(value); break;
        case 0x1f801824:
            mdecWrite1(value); break;

        case 0x1f801100:
            psxRcntWcount(0, value & 0xffff); return;
        case 0x1f801104:
            psxRcntWmode(0, value); return;
        case 0x1f801108:
            psxRcntWtarget(0, value & 0xffff); return; //  HW_DMA_ICR&= SWAP32((~value)&0xff000000);

        case 0x1f801110:
            psxRcntWcount(1, value & 0xffff); return;
        case 0x1f801114:
            psxRcntWmode(1, value); return;
        case 0x1f801118:
            psxRcntWtarget(1, value & 0xffff); return;

        case 0x1f801120:
            psxRcntWcount(2, value & 0xffff); return;
        case 0x1f801124:
            psxRcntWmode(2, value); return;
        case 0x1f801128:
            psxRcntWtarget(2, value & 0xffff); return;

        default:
            // Dukes of Hazard 2 - car engine noise
            if (add>=0x1f801c00 && add<0x1f801e00) {
                SPU_WriteRegister(add, value&0xffff);
                add += 2;
                value >>= 16;

                if (add>=0x1f801c00 && add<0x1f801e00)
                    SPU_WriteRegister(add, value&0xffff);
                return;
            }

            psxHu32ref(add) = SWAPu32(value);
            return;
    }
    psxHu32ref(add) = SWAPu32(value);
}
