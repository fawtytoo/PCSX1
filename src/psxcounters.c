/***************************************************************************
 *   Copyright (C) 2010 by Blade_Arma                                      *
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
 * Internal PSX counters.
 */
#include "system.h"
#include "spu.h"

#include "psxcounters.h"

/******************************************************************************/

typedef struct
{
    u16     mode, target;
    u32     rate, irq, counterState, irqState;
    u32     cycle, cycleStart;
}
RCNT;

#define RC0GATE             0x0001  // 0    not implemented
#define RC1GATE             0x0001  // 0    not implemented
#define RC2DISABLE          0x0001  // 0    partially implemented
#define RcUnknown1          0x0002  // 1    ?
#define RcUnknown2          0x0004  // 2    ?
#define RCCOUNTTOTARGET     0x0008  // 3
#define RCIRQONTARGET       0x0010  // 4
#define RCIRQONOVERFLOW     0x0020  // 5
#define RCIRQREGENERATE     0x0040  // 6
#define RcUnknown7          0x0080  // 7    ?
#define RC0PIXELCLOCK       0x0100  // 8    fake implementation
#define RC1HSYNCCLOCK       0x0100  // 8
#define Rc2Unknown8         0x0100  // 8    ?
#define Rc0Unknown9         0x0200  // 9    ?
#define Rc1Unknown9         0x0200  // 9    ?
#define RC2ONEEIGHTHCLOCK   0x0200  // 9
#define RCIRQREQUEST        0x0400  // 10   Interrupt request flag (0 disabled or during int, 1 request)
#define RCCOUNTEQTARGET     0x0800  // 11
#define RCOVERFLOW          0x1000  // 12
#define RcUnknown13         0x2000  // 13   ? (always zero)
#define RcUnknown14         0x4000  // 14   ? (always zero)
#define RcUnknown15         0x8000  // 15   ? (always zero)

#define JITTER_FLAGS        (RC2ONEEIGHTHCLOCK | RCIRQREGENERATE | RCCOUNTTOTARGET)

#define COUNTERQUANTITY     4
//static const u32 CounterQuantity  = 4;

#define COUNTTOOVERFLOW     0
#define COUNTTOTARGET       1

static u32 FrameRate;
static u32 VBlankStart;
static u32 SpuUpdInterval;

/******************************************************************************/

static RCNT     rcnts[COUNTERQUANTITY];

static u32 hSyncCount = 0;
static u32 spuSyncCount = 0;

u32 HSyncTotal;
u32 psxNextCounter = 0, psxNextsCounter = 0;

/******************************************************************************/

static inline
void setIrq( u32 irq )
{   // psxHu32ref is a macro
    psxHu32ref(0x1070) |= SWAPu32(irq);
}

/******************************************************************************/

static inline
void _psxRcntWcount( u32 index, u32 value )
{
    if( value > 0xffff )
    {
        value &= 0xffff;
    }

    rcnts[index].cycleStart  = psxRegs.cycle;
    rcnts[index].cycleStart -= value * rcnts[index].rate;

    // TODO: <=.
    if( value < rcnts[index].target )
    {
        rcnts[index].cycle = rcnts[index].target * rcnts[index].rate;
        rcnts[index].counterState = COUNTTOTARGET;
    }
    else
    {
        rcnts[index].cycle = 0xffff * rcnts[index].rate;
        rcnts[index].counterState = COUNTTOOVERFLOW;
    }
}

static inline
u32 _psxRcntRcount( u32 index )
{
    u32 count;

    count  = psxRegs.cycle;
    count -= rcnts[index].cycleStart;
    count /= rcnts[index].rate;

    if( count > 0xffff )
    {
        count &= 0xffff;
    }

    return count;
}

/******************************************************************************/

static
void psxRcntSet()
{
    s32 countToUpdate;
    u32 i;

    psxNextsCounter = psxRegs.cycle;
    psxNextCounter  = 0x7fffffff;

    for (i = 0; i < COUNTERQUANTITY; ++i)
    {
        countToUpdate = rcnts[i].cycle - (psxNextsCounter - rcnts[i].cycleStart);

        if( countToUpdate < 0 )
        {
            psxNextCounter = 0;
            break;
        }

        if( countToUpdate < (s32)psxNextCounter )
        {
            psxNextCounter = countToUpdate;
        }
    }
}

/******************************************************************************/

static
void psxRcntReset( u32 index )
{
    u32 count;

    if (rcnts[index].counterState == COUNTTOTARGET)
    {
        if (rcnts[index].mode & RCCOUNTTOTARGET)
        {
            count  = psxRegs.cycle;
            count -= rcnts[index].cycleStart;
            count /= rcnts[index].rate;
            count -= rcnts[index].target;
        }
        else
        {
            count = _psxRcntRcount( index );
        }

        _psxRcntWcount( index, count );

        if (rcnts[index].mode & RCIRQONTARGET)
        {
            if ((rcnts[index].mode & RCIRQREGENERATE) || (!rcnts[index].irqState) )
            {
                setIrq( rcnts[index].irq );
                rcnts[index].irqState = true;
            }
        }

        rcnts[index].mode |= RCCOUNTEQTARGET;
    }
    else if (rcnts[index].counterState == COUNTTOOVERFLOW)
    {
        count  = psxRegs.cycle;
        count -= rcnts[index].cycleStart;
        count /= rcnts[index].rate;
        count -= 0xffff;

        _psxRcntWcount( index, count );

        if (rcnts[index].mode & RCIRQONOVERFLOW)
        {
            if ((rcnts[index].mode & RCIRQREGENERATE) || (!rcnts[index].irqState))
            {
                setIrq( rcnts[index].irq );
                rcnts[index].irqState = true;
            }
        }

        rcnts[index].mode |= RCOVERFLOW;
    }

    rcnts[index].mode |= RCIRQREQUEST;

    psxRcntSet();
}

void psxRcntUpdate()
{
    u32 cycle;

    cycle = psxRegs.cycle;

    // rcnt 0.
    if( cycle - rcnts[0].cycleStart >= rcnts[0].cycle )
    {
        psxRcntReset( 0 );
    }

    // rcnt 1.
    if( cycle - rcnts[1].cycleStart >= rcnts[1].cycle )
    {
        psxRcntReset( 1 );
    }

    // rcnt 2.
    if( cycle - rcnts[2].cycleStart >= rcnts[2].cycle )
    {
        psxRcntReset( 2 );
    }

    // rcnt base.
    if( cycle - rcnts[3].cycleStart >= rcnts[3].cycle )
    {
        psxRcntReset( 3 );

        GPU_HSync(hSyncCount);

        spuSyncCount++;
        hSyncCount++;

        // Update spu.
        if (spuSyncCount >= SpuUpdInterval)
        {
            spuSyncCount = 0;

            SPU_Async(SpuUpdInterval * rcnts[3].target);
        }

#ifdef ENABLE_NET
        SIO1_update( 0 );
#endif

        // VSync irq.
        if (hSyncCount == VBlankStart)
        {
            GPU_VBlank( 1 );

            // For the best times. :D
            //setIrq( 0x01 );
        }

        // Update lace. (calculated at psxHsyncCalculate() on init/defreeze)
        if (hSyncCount >= HSyncTotal)
        {
            hSyncCount = 0;

            GPU_VBlank( 0 );
            setIrq( 0x01 );

            GPU_UpdateLace();
            SysUpdate();
        }
    }
}

/******************************************************************************/

void psxRcntWcount( u32 index, u32 value )
{
    psxRcntUpdate();

    _psxRcntWcount( index, value );
    psxRcntSet();
}

void psxRcntWmode( u32 index, u32 value )
{
    psxRcntUpdate();

    rcnts[index].mode = value;
    rcnts[index].irqState = false;

    switch( index )
    {
        case 0:
            if (value & RC0PIXELCLOCK)
            {
                rcnts[index].rate = 5;
            }
            else
            {
                rcnts[index].rate = 1;
            }
        break;
        case 1:
            if (value & RC1HSYNCCLOCK)
            {
                rcnts[index].rate = PSXCLK / (FrameRate * HSyncTotal);
            }
            else
            {
                rcnts[index].rate = 1;
            }
        break;
        case 2:
            if (value & RC2ONEEIGHTHCLOCK)
            {
                rcnts[index].rate = 8;
            }
            else
            {
                rcnts[index].rate = 1;
            }

            // TODO: wcount must work.
            if (value & RC2DISABLE)
            {
                rcnts[index].rate = 0xffffffff;
            }
        break;
    }

    _psxRcntWcount( index, 0 );
    psxRcntSet();
}

void psxRcntWtarget( u32 index, u32 value )
{
    psxRcntUpdate();

    rcnts[index].target = value; // TODO: only upper 16bit used

    _psxRcntWcount( index, _psxRcntRcount( index ) );
    psxRcntSet();
}

/******************************************************************************/

u32 psxRcntRcount( u32 index )
{
    u32 count;

    psxRcntUpdate();

    count = _psxRcntRcount( index );

    // Parasite Eve 2 fix - artificial clock jitter based on BIAS
    // TODO: any other games depend on getting excepted value from RCNT?
    if (Config.HackFix && index == 2 && rcnts[index].counterState == COUNTTOTARGET && (Config.RCntFix || ((rcnts[index].mode & 0x2FF) == JITTER_FLAGS)))
    {
        /*
        *The problem is that...
        *
        *We generate too many cycles during PSX HW hardware operations.
        *
        *OR
        *
        *We simply count too many cycles here for RCNTs.
        *
        *OR
        *
        *RCNT implementation here is only 99% compatible. Assumed this since easities to fix (only PE2 known to be affected).
        */
    }

    return count;
}

u32 psxRcntRmode( u32 index )
{
    u16 mode;

    psxRcntUpdate();

    mode = rcnts[index].mode;
    rcnts[index].mode &= 0xe7ff;

    return mode;
}

u32 psxRcntRtarget( u32 index )
{
    return rcnts[index].target;
}

/******************************************************************************/

void psxHsyncCalculate()
{
    HSyncTotal = Config.Region == PSX_TYPE_NTSC ? 263 : 313;
    if (Config.VSyncWA)
    {
        HSyncTotal /= BIAS;
    }
    else if (Config.HackFix)
    {
        HSyncTotal += 1;
    }
}

void psxRcntInit()
{
    s32 i;

    FrameRate = Config.Region == PSX_TYPE_NTSC ? 60 : 50;
    VBlankStart = Config.Region == PSX_TYPE_NTSC ? 243 : 256;
    SpuUpdInterval = Config.Region == PSX_TYPE_NTSC ? 23 : 22;

    psxHsyncCalculate();

    // rcnt 0.
    rcnts[0].rate   = 1;
    rcnts[0].irq    = 0x10;

    // rcnt 1.
    rcnts[1].rate   = 1;
    rcnts[1].irq    = 0x20;

    // rcnt 2.
    rcnts[2].rate   = 1;
    rcnts[2].irq    = 0x40;

    // rcnt base.
    rcnts[3].rate   = 1;
    rcnts[3].mode   = RCCOUNTTOTARGET;
    rcnts[3].target = (PSXCLK / (FrameRate * HSyncTotal));

    for (i = 0; i < COUNTERQUANTITY; ++i)
    {
        _psxRcntWcount( i, 0 );
    }

    hSyncCount = 0;
    spuSyncCount = 0;

    psxRcntSet();
}
