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

#ifndef __CDROM_H__
#define __CDROM_H__

#include "psxcommon.h"
#include "decode_xa.h"
#include "r3000a.h"
#include "cdriso.h"
#include "psxmem.h"
#include "psxhw.h"
#include "psxcommon.h"

#define btoi(b)     ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define itob(i)     ((i) / 10 * 16 + (i) % 10) /* u_char to BCD */

#define MSF2SECT(m, s, f)       (((m) * 60 + (s) - 2) * 75 + (f))

#define CD_FRAMESIZE_RAW        2352
#define DATA_SIZE               (CD_FRAMESIZE_RAW - 12)

#define SUB_FRAMESIZE           96

typedef struct {
    u8      OCUP;
    u8      Reg1Mode;
    u8      Reg2;
    u8      CmdProcess;
    u8      Ctrl;
    u8      Stat;

    u8      StatP;

    u8      Transfer[CD_FRAMESIZE_RAW];
    u32     transferIndex;

    u8      Prev[4];
    u8      Param[8];
    u8      Result[16];

    u8      ParamC;
    u8      ParamP;
    u8      ResultC;
    u8      ResultP;
    u8      ResultReady;
    u8      Cmd;
    u8      Readed;
    u8      SetlocPending;
    u32 Reading;

    u8      ResultTN[6];
    char    ResultTD[4];
    char    SetSectorPlay[4];
    char    SetSectorEnd[4];
    u8      SetSector[4];
    u8      Track;
    bool    Play, Muted;
    int CurTrack;
    int Mode, File, Channel;
    int Reset;
    int RErr;
    int FirstSector;

    xa_decode_t Xa;

    int Init;

    u16 Irq;
    u8 IrqRepeated;
    u32 eCycle;

    u8 Seeked;
    u8 ReadRescheduled;

    u8 DriveState;
    u8 FastForward;
    u8 FastBackward;

    u8 AttenuatorLeftToLeft, AttenuatorLeftToRight;
    u8 AttenuatorRightToRight, AttenuatorRightToLeft;
    u8 AttenuatorLeftToLeftT, AttenuatorLeftToRightT;
    u8 AttenuatorRightToRightT, AttenuatorRightToLeftT;

    struct {
        u8      Track;
        u8      Index;
        char Relative[3];
        u8      Absolute[3];
    } subq;
    u8      TrackChanged;
} cdrStruct;

extern cdrStruct cdr;

void cdrReset();
void cdrAttenuate(s16 *buf, int samples, int stereo);

void cdrInterrupt();
void cdrReadInterrupt();
void cdrDecodedBufferInterrupt();
void cdrLidSeekInterrupt();
void cdrPlayInterrupt();
void cdrDmaInterrupt();

u8      cdrRead0(void);
u8      cdrRead1(void);
u8      cdrRead2(void);
u8      cdrRead3(void);
void cdrWrite0(u8      rt);
void cdrWrite1(u8      rt);
void cdrWrite2(u8      rt);
void cdrWrite3(u8      rt);

void CDR_LidInterrupt(void);

#endif
