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
* Handles all CD-ROM registers and functions.
*/

#include "cdrom.h"
#include "psxdma.h"
#include "spu.h"

cdrStruct cdr;

/* CD-ROM magic numbers */
#define CdlSync        0
#define CdlNop         1
#define CdlSetloc      2
#define CdlPlay        3
#define CdlForward     4
#define CdlBackward    5
#define CdlReadN       6
#define CdlStandby     7
#define CdlStop        8
#define CdlPause       9
#define CdlInit        10 // 0xa
#define CdlMute        11 // 0xb
#define CdlDemute      12 // 0xc
#define CdlSetfilter   13 // 0xd
#define CdlSetmode     14 // 0xe
#define CdlGetmode     15 // 0xf
#define CdlGetlocL     16 // 0x10
#define CdlGetlocP     17 // 0x11
#define CdlReadT       18 // 0x12
#define CdlGetTN       19 // 0x13
#define CdlGetTD       20 // 0x14
#define CdlSeekL       21 // 0x15
#define CdlSeekP       22 // 0x16
#define CdlSetclock    23 // 0x17
#define CdlGetclock    24 // 0x18
#define CdlTest        25 // 0x19
#define CdlID          26 // 0x1a
#define CdlReadS       27 // 0x1b
#define CdlReset       28 // 0x1c
#define CdlGetQ        29 // 0x1d
#define CdlReadToc     30 // 0x1e

char *CmdName[0x100]= {
    "CdlSync",     "CdlNop",       "CdlSetloc",  "CdlPlay",
    "CdlForward",  "CdlBackward",  "CdlReadN",   "CdlStandby",
    "CdlStop",     "CdlPause",     "CdlInit",    "CdlMute",
    "CdlDemute",   "CdlSetfilter", "CdlSetmode", "CdlGetmode",
    "CdlGetlocL",  "CdlGetlocP",   "CdlReadT",   "CdlGetTN",
    "CdlGetTD",    "CdlSeekL",     "CdlSeekP",   "CdlSetclock",
    "CdlGetclock", "CdlTest",      "CdlID",      "CdlReadS",
    "CdlReset",    NULL,           "CDlReadToc", NULL
};

u8      Test04[] = { 0 };
u8      Test05[] = { 0 };
u8      Test20[] = { 0x98, 0x06, 0x10, 0xC3 };
u8      Test22[] = { 0x66, 0x6F, 0x72, 0x20, 0x45, 0x75, 0x72, 0x6F };
u8      Test23[] = { 0x43, 0x58, 0x44, 0x32, 0x39 ,0x34, 0x30, 0x51 };

// cdr.Stat:
#define NoIntr      0
#define DataReady   1
#define Complete    2
#define Acknowledge 3
#define DataEnd     4
#define DiskError   5

/* Modes flags */
#define MODE_SPEED       (1<<7) // 0x80
#define MODE_STRSND      (1<<6) // 0x40 ADPCM on/off
#define MODE_SIZE_2340   (1<<5) // 0x20
#define MODE_SIZE_2328   (1<<4) // 0x10
#define MODE_SIZE_2048   (0<<4) // 0x00
#define MODE_SF          (1<<3) // 0x08 channel on/off
#define MODE_REPORT      (1<<2) // 0x04
#define MODE_AUTOPAUSE   (1<<1) // 0x02
#define MODE_CDDA        (1<<0) // 0x01

/* Status flags */
#define STATUS_PLAY      (1<<7) // 0x80
#define STATUS_SEEK      (1<<6) // 0x40
#define STATUS_READ      (1<<5) // 0x20
#define STATUS_SHELLOPEN (1<<4) // 0x10
#define STATUS_UNKNOWN3  (1<<3) // 0x08
#define STATUS_UNKNOWN2  (1<<2) // 0x04
#define STATUS_ROTATING  (1<<1) // 0x02
#define STATUS_ERROR     (1<<0) // 0x01

/* Errors */
#define ERROR_NOTREADY   (1<<7) // 0x80
#define ERROR_INVALIDCMD (1<<6) // 0x40
#define ERROR_INVALIDARG (1<<5) // 0x20

// 1x = 75 sectors per second
// PSXCLK = 1 sec in the ps
// so (PSXCLK / 75) = cdr read time (linuzappz)
#define cdReadTime (PSXCLK / 75)

enum drive_state {
    DRIVESTATE_STANDBY = 0,
    DRIVESTATE_LID_OPEN,
    DRIVESTATE_RESCAN_CD,
    DRIVESTATE_PREPARE_CD,
    DRIVESTATE_STOPPED,
};

// for cdr.Seeked
enum seeked_state {
    SEEK_PENDING = 0,
    SEEK_DONE = 1,
};

static CDRSTAT  stat;

// lookup table for crc calculation
static u16 crctab[256] =
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

u16 calcCrc(u8 *d, int len)
{
    u16 crc = 0;
    int i;

    for (i = 0; i < len; i++)
    {
        crc = crctab[(crc >> 8) ^ d[i]] ^ (crc << 8);
    }

    return ~crc;
}

// for that weird psemu API..
static u32 fsm2sec(char *msf)
{
    return ((msf[2] * 60 + msf[1]) * 75) + msf[0];
}

extern long ISOinit(void);

// cdrInterrupt
#define CDR_INT(eCycle) { \
    psxRegs.interrupt |= (1 << PSXINT_CDR); \
    psxRegs.intCycle[PSXINT_CDR].cycle = eCycle; \
    psxRegs.intCycle[PSXINT_CDR].sCycle = psxRegs.cycle; \
}

// cdrReadInterrupt
#define CDREAD_INT(eCycle) { \
    psxRegs.interrupt |= (1 << PSXINT_CDREAD); \
    psxRegs.intCycle[PSXINT_CDREAD].cycle = eCycle; \
    psxRegs.intCycle[PSXINT_CDREAD].sCycle = psxRegs.cycle; \
}

// cdrDecodedBufferInterrupt
#define CDRDBUF_INT(eCycle) { \
    psxRegs.interrupt |= (1 << PSXINT_CDRDBUF); \
    psxRegs.intCycle[PSXINT_CDRDBUF].cycle = eCycle; \
    psxRegs.intCycle[PSXINT_CDRDBUF].sCycle = psxRegs.cycle; \
}

// cdrLidSeekInterrupt
#define CDRLID_INT(eCycle) { \
    psxRegs.interrupt |= (1 << PSXINT_CDRLID); \
    psxRegs.intCycle[PSXINT_CDRLID].cycle = eCycle; \
    psxRegs.intCycle[PSXINT_CDRLID].sCycle = psxRegs.cycle; \
}

// cdrPlayInterrupt
#define CDRMISC_INT(eCycle) { \
    psxRegs.interrupt |= (1 << PSXINT_CDRPLAY); \
    psxRegs.intCycle[PSXINT_CDRPLAY].cycle = eCycle; \
    psxRegs.intCycle[PSXINT_CDRPLAY].sCycle = psxRegs.cycle; \
}

#define StopReading() { \
    if (cdr.Reading) { \
        cdr.Reading = 0; \
        psxRegs.interrupt &= ~(1 << PSXINT_CDREAD); \
    } \
    cdr.StatP &= ~(STATUS_READ|STATUS_SEEK);\
}

#define StopCdda() { \
    if (cdr.Play) { \
        cdr.StatP &= ~STATUS_PLAY; \
        cdr.Play = false; \
        cdr.FastForward = 0; \
        cdr.FastBackward = 0; \
        SPU_RegisterCallback(SPU_Irq); \
    } \
}

#define SetResultSize(size) { \
    cdr.ResultP = 0; \
    cdr.ResultC = size; \
    cdr.ResultReady = 1; \
}

static void setIrq(void)
{
    if (cdr.Stat & cdr.Reg2)
        psxHu32ref(0x1070) |= SWAP32((u32)0x4);
}

static void adjustTransferIndex(void)
{
    u32     bufSize = 0;

    switch (cdr.Mode & (MODE_SIZE_2340|MODE_SIZE_2328)) {
        case MODE_SIZE_2340: bufSize = 2340; break;
        case MODE_SIZE_2328: bufSize = 12 + 2328; break;
        default:
        case MODE_SIZE_2048: bufSize = 12 + 2048; break;
    }

    if (cdr.transferIndex >= bufSize)
        cdr.transferIndex -= bufSize;
}

// FIXME: do this in SPU instead
void cdrDecodedBufferInterrupt()
{
#if 0
    return;
#endif

    // ISO reader only
    //if( CDR_init != ISOinit ) return;

    // check dbuf IRQ still active
    if( cdr.Play == 0 ) return;
    if ((SPU_ReadRegister(0x1f801000 | H_SPUctrl) & 0x40) == 0) return;
    if ((SPU_ReadRegister(0x1f801000 | H_SPUirqAddr) * 8) >= 0x800) return;

    // turn off plugin SPU IRQ decoded buffer handling
    SPU_RegisterCallback( 0 );

    /*
    Vib Ribbon

    000-3FF = left CDDA
    400-7FF = right CDDA

    Assume IRQ every wrap
    */

    // signal CDDA data ready
    psxHu32ref(0x1070) |= SWAP32((u32)0x200);

    // time for next full buffer
    //CDRDBUF_INT( PSXCLK / 44100 * 0x200 );
    CDRDBUF_INT( PSXCLK / 44100 * 0x100 );
}

// timing used in this function was taken from tests on real hardware
// (yes it's slow, but you probably don't want to modify it)
void cdrLidSeekInterrupt()
{
    switch (cdr.DriveState) {
    default:
    case DRIVESTATE_STANDBY:
        cdr.StatP &= ~STATUS_SEEK;

        if (ISO_GetStatus(&stat) == -1)
            return;

        if (stat.Status & STATUS_SHELLOPEN)
        {
            StopCdda();
            cdr.DriveState = DRIVESTATE_LID_OPEN;
            CDRLID_INT(0x800);
        }
        break;

    case DRIVESTATE_LID_OPEN:
        if (ISO_GetStatus(&stat) == -1)
            stat.Status &= ~STATUS_SHELLOPEN;

        // 02, 12, 10
        if (!(cdr.StatP & STATUS_SHELLOPEN)) {
            StopReading();
            cdr.StatP |= STATUS_SHELLOPEN;

            // could generate error irq here, but real hardware
            // only sometimes does that
            // (not done when lots of commands are sent?)

            CDRLID_INT(cdReadTime * 30);
            break;
        }
        else if (cdr.StatP & STATUS_ROTATING) {
            cdr.StatP &= ~STATUS_ROTATING;
        }
        else if (!(stat.Status & STATUS_SHELLOPEN)) {
            // closed now
            CheckCdrom();

            // cdr.StatP STATUS_SHELLOPEN is "sticky"
            // and is only cleared by CdlNop

            cdr.DriveState = DRIVESTATE_RESCAN_CD;
            CDRLID_INT(cdReadTime * 105);
            break;
        }

        // recheck for close
        CDRLID_INT(cdReadTime * 3);
        break;

    case DRIVESTATE_RESCAN_CD:
        cdr.StatP |= STATUS_ROTATING;
        cdr.DriveState = DRIVESTATE_PREPARE_CD;

        // this is very long on real hardware, over 6 seconds
        // make it a bit faster here...
        CDRLID_INT(cdReadTime * 150);
        break;

    case DRIVESTATE_PREPARE_CD:
        cdr.StatP |= STATUS_SEEK;

        cdr.DriveState = DRIVESTATE_STANDBY;
        CDRLID_INT(cdReadTime * 26);
        break;
    }
}

static void Find_CurTrack(char *time)
{
    int current, sect;

    current = msf2sec(time);

    for (cdr.CurTrack = 1; cdr.CurTrack < cdr.ResultTN[1]; cdr.CurTrack++) {
        ISO_GetTD(cdr.CurTrack + 1, cdr.ResultTD);
        sect = fsm2sec(cdr.ResultTD);
        if (sect - current >= 150)
            break;
    }
}

static void generate_subq(char *time)
{
    char    start[3], next[3];
    u32     this_s, start_s, next_s, pregap;
    int relative_s;

    ISO_GetTD(cdr.CurTrack, start);
    if (cdr.CurTrack + 1 <= cdr.ResultTN[1]) {
        pregap = 150;
        ISO_GetTD(cdr.CurTrack + 1, next);
    }
    else {
        // last track - cd size
        pregap = 0;
        next[0] = cdr.SetSectorEnd[2];
        next[1] = cdr.SetSectorEnd[1];
        next[2] = cdr.SetSectorEnd[0];
    }

    this_s = msf2sec(time);
    start_s = fsm2sec(start);
    next_s = fsm2sec(next);

    cdr.TrackChanged = false;

    if (next_s - this_s < pregap) {
        cdr.TrackChanged = true;
        cdr.CurTrack++;
        start_s = next_s;
    }

    cdr.subq.Index = 1;

    relative_s = this_s - start_s;
    if (relative_s < 0) {
        cdr.subq.Index = 0;
        relative_s = -relative_s;
    }
    sec2msf(relative_s, cdr.subq.Relative);

    cdr.subq.Track = itob(cdr.CurTrack);
    cdr.subq.Relative[0] = itob(cdr.subq.Relative[0]);
    cdr.subq.Relative[1] = itob(cdr.subq.Relative[1]);
    cdr.subq.Relative[2] = itob(cdr.subq.Relative[2]);
    cdr.subq.Absolute[0] = itob(time[0]);
    cdr.subq.Absolute[1] = itob(time[1]);
    cdr.subq.Absolute[2] = itob(time[2]);
}

static void ReadTrack(char *time)
{
    u8      tmp[3];
    SUBQ    *subq;
    u16     crc;

    tmp[0] = itob(time[0]);
    tmp[1] = itob(time[1]);
    tmp[2] = itob(time[2]);

    if (memcmp(cdr.Prev, tmp, 3) == 0)
        return;

    cdr.RErr = ISO_ReadTrack(tmp);
    memcpy(cdr.Prev, tmp, 3);

    subq = (SUBQ *)ISO_GetBufferSub();
    if (subq != NULL && cdr.CurTrack == 1) {
        crc = calcCrc((u8 *)subq + 12, 10);
        if (crc == (((u16)subq->CRC[0] << 8) | subq->CRC[1])) {
            cdr.subq.Track = subq->TrackNumber;
            cdr.subq.Index = subq->IndexNumber;
            memcpy(cdr.subq.Relative, subq->TrackRelativeAddress, 3);
            memcpy(cdr.subq.Absolute, subq->AbsoluteAddress, 3);
        }
    }
    else {
        generate_subq(time);
    }
}

static void AddIrqQueue(u16 irq, u32 ecycle) {
    if (cdr.Irq != 0) {
        if (irq == cdr.Irq || irq + 0x100 == cdr.Irq) {
            cdr.IrqRepeated = 1;
            CDR_INT(ecycle);
            return;
        }
    }

    cdr.Irq = irq;
    cdr.eCycle = ecycle;

    CDR_INT(ecycle);
}

static void cdrPlayInterrupt_Autopause()
{
    if ((cdr.Mode & MODE_AUTOPAUSE) && cdr.TrackChanged) {
        // Magic the Gathering
        // - looping territory cdda

        // ...?
        //cdr.ResultReady = 1;
        //cdr.Stat = DataReady;
        cdr.Stat = DataEnd;
        setIrq();

        StopCdda();
    }
    else if (cdr.Mode & MODE_REPORT) {

        cdr.Result[0] = cdr.StatP;
        cdr.Result[1] = cdr.subq.Track;
        cdr.Result[2] = cdr.subq.Index;

        if (cdr.subq.Absolute[2] & 0x10) {
            cdr.Result[3] = cdr.subq.Relative[0];
            cdr.Result[4] = cdr.subq.Relative[1] | 0x80;
            cdr.Result[5] = cdr.subq.Relative[2];
        }
        else {
            cdr.Result[3] = cdr.subq.Absolute[0];
            cdr.Result[4] = cdr.subq.Absolute[1];
            cdr.Result[5] = cdr.subq.Absolute[2];
        }

        cdr.Result[6] = 0;
        cdr.Result[7] = 0;

        // Rayman: Logo freeze (resultready + dataready)
        cdr.ResultReady = 1;
        cdr.Stat = DataReady;

        SetResultSize(8);
        setIrq();
    }
}

// also handles seek
void cdrPlayInterrupt()
{
    if (cdr.Seeked == SEEK_PENDING) {
        if (cdr.Stat) {
            CDRMISC_INT( 0x100 );
            return;
        }
        SetResultSize(1);
        cdr.StatP |= STATUS_ROTATING;
        cdr.StatP &= ~STATUS_SEEK;
        cdr.Result[0] = cdr.StatP;
        cdr.Seeked = SEEK_DONE;
        if (cdr.Irq == 0) {
            cdr.Stat = Complete;
            setIrq();
        }

        if (cdr.SetlocPending) {
            memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
            cdr.SetlocPending = 0;
        }
        Find_CurTrack(cdr.SetSectorPlay);
        ReadTrack(cdr.SetSectorPlay);
        cdr.TrackChanged = false;
    }

    if (!cdr.Play) return;

    if (memcmp(cdr.SetSectorPlay, cdr.SetSectorEnd, 3) == 0) {
        StopCdda();
        cdr.TrackChanged = true;
    }

    if (!cdr.Irq && !cdr.Stat && (cdr.Mode & (MODE_AUTOPAUSE|MODE_REPORT)))
        cdrPlayInterrupt_Autopause();

    if (!cdr.Play) return;

    if (!cdr.Muted) {
        ISO_ReadCDDA(cdr.SetSectorPlay[0], cdr.SetSectorPlay[1],
            cdr.SetSectorPlay[2], cdr.Transfer);

        cdrAttenuate((s16 *)cdr.Transfer, CD_FRAMESIZE_RAW / 4, 1);
        SPU_PlayCDDA_Channel((short *)cdr.Transfer, CD_FRAMESIZE_RAW);
    }

    cdr.SetSectorPlay[2]++;
    if (cdr.SetSectorPlay[2] == 75) {
        cdr.SetSectorPlay[2] = 0;
        cdr.SetSectorPlay[1]++;
        if (cdr.SetSectorPlay[1] == 60) {
            cdr.SetSectorPlay[1] = 0;
            cdr.SetSectorPlay[0]++;
        }
    }

    CDRMISC_INT(cdReadTime);

    // update for CdlGetlocP/autopause
    generate_subq(cdr.SetSectorPlay);
}

void cdrInterrupt() {
    u16 Irq = cdr.Irq;
    int no_busy_error = 0;
    int start_rotating = 0;
    int error = 0;
    int delay;

    // Reschedule IRQ
    if (cdr.Stat) {
        CDR_INT( 0x100 );
        return;
    }

    cdr.Ctrl &= ~0x80;

    // default response
    SetResultSize(1);
    cdr.Result[0] = cdr.StatP;
    cdr.Stat = Acknowledge;

    if (cdr.IrqRepeated) {
        cdr.IrqRepeated = 0;
        if (cdr.eCycle > psxRegs.cycle) {
            CDR_INT(cdr.eCycle);
            goto finish;
        }
    }

    cdr.Irq = 0;

    switch (Irq) {
        case CdlSync:
            // TOOD: sometimes/always return error?
            break;

        case CdlNop:
            if (cdr.DriveState != DRIVESTATE_LID_OPEN)
                cdr.StatP &= ~STATUS_SHELLOPEN;
            no_busy_error = 1;
            break;

        case CdlSetloc:
            break;

        do_CdlPlay:
        case CdlPlay:
            StopCdda();
            if (cdr.Seeked == SEEK_PENDING) {
                // XXX: wrong, should seek instead..
                cdr.Seeked = SEEK_DONE;
            }
            if (cdr.SetlocPending) {
                memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
                cdr.SetlocPending = 0;
            }

            // BIOS CD Player
            // - Pause player, hit Track 01/02/../xx (Setloc issued!!)

            if (cdr.ParamC == 0 || cdr.Param[0] == 0) {
                // FIXME was a log entry
            }
            else
            {
                int track = btoi( cdr.Param[0] );

                if (track <= cdr.ResultTN[1])
                    cdr.CurTrack = track;

                ISO_GetTD((u8)cdr.CurTrack, cdr.ResultTD); // TODO
                {
                    cdr.SetSectorPlay[0] = cdr.ResultTD[2];
                    cdr.SetSectorPlay[1] = cdr.ResultTD[1];
                    cdr.SetSectorPlay[2] = cdr.ResultTD[0];
                }
            }

            /*
            Rayman: detect track changes
            - fixes logo freeze

            Twisted Metal 2: skip PREGAP + starting accurate SubQ
            - plays tracks without retry play

            Wild 9: skip PREGAP + starting accurate SubQ
            - plays tracks without retry play
            */
            Find_CurTrack(cdr.SetSectorPlay);
            ReadTrack(cdr.SetSectorPlay);
            cdr.TrackChanged = false;

            ISO_Play(cdr.SetSectorPlay);

            // Vib Ribbon: gameplay checks flag
            cdr.StatP &= ~STATUS_SEEK;
            cdr.Result[0] = cdr.StatP;

            cdr.StatP |= STATUS_PLAY;

            // BIOS player - set flag again
            cdr.Play = true;

            CDRMISC_INT( cdReadTime );
            start_rotating = 1;
            break;

        case CdlForward:
            // TODO: error 80 if stopped
            cdr.Stat = Complete;

            // GameShark CD Player: Calls 2x + Play 2x
            if( cdr.FastForward == 0 ) cdr.FastForward = 2;
            else cdr.FastForward++;

            cdr.FastBackward = 0;
            break;

        case CdlBackward:
            cdr.Stat = Complete;

            // GameShark CD Player: Calls 2x + Play 2x
            if( cdr.FastBackward == 0 ) cdr.FastBackward = 2;
            else cdr.FastBackward++;

            cdr.FastForward = 0;
            break;

        case CdlStandby:
            if (cdr.DriveState != DRIVESTATE_STOPPED) {
                error = ERROR_INVALIDARG;
                goto set_error;
            }
            AddIrqQueue(CdlStandby + 0x100, cdReadTime * 125 / 2);
            start_rotating = 1;
            break;

        case CdlStandby + 0x100:
            cdr.Stat = Complete;
            break;

        case CdlStop:
            if (cdr.Play) {
                // grab time for current track
                ISO_GetTD((u8)(cdr.CurTrack), cdr.ResultTD);

                cdr.SetSectorPlay[0] = cdr.ResultTD[2];
                cdr.SetSectorPlay[1] = cdr.ResultTD[1];
                cdr.SetSectorPlay[2] = cdr.ResultTD[0];
            }

            StopCdda();
            StopReading();

            delay = 0x800;
            if (cdr.DriveState == DRIVESTATE_STANDBY)
                delay = cdReadTime * 30 / 2;

            cdr.DriveState = DRIVESTATE_STOPPED;
            AddIrqQueue(CdlStop + 0x100, delay);
            break;

        case CdlStop + 0x100:
            cdr.StatP &= ~STATUS_ROTATING;
            cdr.Result[0] = cdr.StatP;
            cdr.Stat = Complete;
            break;

        case CdlPause:
            /*
            Gundam Battle Assault 2: much slower (*)
            - Fixes boot, gameplay

            Hokuto no Ken 2: slower
            - Fixes intro + subtitles

            InuYasha - Feudal Fairy Tale: slower
            - Fixes battles
            */
            AddIrqQueue(CdlPause + 0x100, cdReadTime * 3);
            cdr.Ctrl |= 0x80;
            break;

        case CdlPause + 0x100:
            cdr.StatP &= ~STATUS_READ;
            cdr.Result[0] = cdr.StatP;
            cdr.Stat = Complete;
            break;

        case CdlInit:
            AddIrqQueue(CdlInit + 0x100, cdReadTime * 6);
            no_busy_error = 1;
            start_rotating = 1;
            break;

        case CdlInit + 0x100:
            cdr.Stat = Complete;
            break;

        case CdlMute:
            cdr.Muted = true;
            break;

        case CdlDemute:
            cdr.Muted = false;
            break;

        case CdlSetfilter:
            cdr.File = cdr.Param[0];
            cdr.Channel = cdr.Param[1];
            break;

        case CdlSetmode:
            no_busy_error = 1;
            break;

        case CdlGetmode:
            SetResultSize(6);
            cdr.Result[1] = cdr.Mode;
            cdr.Result[2] = cdr.File;
            cdr.Result[3] = cdr.Channel;
            cdr.Result[4] = 0;
            cdr.Result[5] = 0;
            no_busy_error = 1;
            break;

        case CdlGetlocL:
            SetResultSize(8);
            memcpy(cdr.Result, cdr.Transfer, 8);
            break;

        case CdlGetlocP:
            SetResultSize(8);
            memcpy(&cdr.Result, &cdr.subq, 8);

            if (!cdr.Play && !cdr.Reading)
                cdr.Result[1] = 0; // HACK?
            break;

        case CdlReadT: // SetSession?
            // really long
            AddIrqQueue(CdlReadT + 0x100, cdReadTime * 290 / 4);
            start_rotating = 1;
            break;

        case CdlReadT + 0x100:
            cdr.Stat = Complete;
            break;

        case CdlGetTN:
            SetResultSize(3);
            ISO_GetTN(cdr.ResultTN);
#if 0
            cdr.Stat = DiskError;
            cdr.Result[0] |= STATUS_ERROR;
#else
            cdr.Stat = Acknowledge;
            cdr.Result[1] = itob(cdr.ResultTN[0]);
            cdr.Result[2] = itob(cdr.ResultTN[1]);
#endif
            break;

        case CdlGetTD:
            cdr.Track = btoi(cdr.Param[0]);
            SetResultSize(4);
            ISO_GetTD(cdr.Track, cdr.ResultTD);
#if 0
            cdr.Stat = DiskError;
            cdr.Result[0] |= STATUS_ERROR;
#else
            cdr.Stat = Acknowledge;
            cdr.Result[0] = cdr.StatP;
            cdr.Result[1] = itob(cdr.ResultTD[2]);
            cdr.Result[2] = itob(cdr.ResultTD[1]);
            cdr.Result[3] = itob(cdr.ResultTD[0]);
#endif
            break;

        case CdlSeekL:
        case CdlSeekP:
            StopCdda();
            StopReading();
            cdr.StatP |= STATUS_SEEK;

            /*
            Crusaders of Might and Magic = 0.5x-4x
            - fix cutscene speech start

            Eggs of Steel = 2x-?
            - fix new game

            Medievil = ?-4x
            - fix cutscene speech

            Rockman X5 = 0.5-4x
            - fix capcom logo
            */
            CDRMISC_INT(cdr.Seeked == SEEK_DONE ? 0x800 : cdReadTime * 4);
            cdr.Seeked = SEEK_PENDING;
            start_rotating = 1;
            break;

        case CdlTest:
            switch (cdr.Param[0]) {
                case 0x20: // System Controller ROM Version
                    SetResultSize(4);
                    memcpy(cdr.Result, Test20, 4);
                    break;
                case 0x22:
                    SetResultSize(8);
                    memcpy(cdr.Result, Test22, 4);
                    break;
                case 0x23: case 0x24:
                    SetResultSize(8);
                    memcpy(cdr.Result, Test23, 4);
                    break;
            }
            no_busy_error = 1;
            break;

        case CdlID:
            AddIrqQueue(CdlID + 0x100, 20480);
            break;

        case CdlID + 0x100:
            SetResultSize(8);
            cdr.Result[0] = cdr.StatP;
            cdr.Result[1] = 0;
            cdr.Result[2] = 0;
            cdr.Result[3] = 0;

            // 0x10 - audio | 0x40 - disk missing | 0x80 - unlicensed
            if (ISO_GetStatus(&stat) == -1 || stat.Type == 0 || stat.Type == 0xff) {
                cdr.Result[1] = 0xc0;
            }
            else {
                if (stat.Type == 2)
                    cdr.Result[1] |= 0x10;
                if (CdromId[0] == '\0')
                    cdr.Result[1] |= 0x80;
            }
            cdr.Result[0] |= (cdr.Result[1] >> 4) & 0x08;

            cdr.Result[4] = 'S';
            cdr.Result[5] = 'C';
            cdr.Result[6] = 'E';
            cdr.Result[7] = Config.CdromId;

            cdr.Stat = Complete;
            break;

        case CdlReset:
            // yes, it really sets STATUS_SHELLOPEN
            cdr.StatP |= STATUS_SHELLOPEN;
            cdr.DriveState = DRIVESTATE_RESCAN_CD;
            CDRLID_INT(20480);
            no_busy_error = 1;
            start_rotating = 1;
            break;

        case CdlGetQ:
            // TODO?
            break;

        case CdlReadToc:
            AddIrqQueue(CdlReadToc + 0x100, cdReadTime * 180 / 4);
            no_busy_error = 1;
            start_rotating = 1;
            break;

        case CdlReadToc + 0x100:
            cdr.Stat = Complete;
            no_busy_error = 1;
            break;

        case CdlReadN:
        case CdlReadS:
            if (cdr.SetlocPending) {
                memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
                cdr.SetlocPending = 0;
            }
            Find_CurTrack(cdr.SetSectorPlay);

            if ((cdr.Mode & MODE_CDDA) && cdr.CurTrack > 1)
                // Read* acts as play for cdda tracks in cdda mode
                goto do_CdlPlay;

            cdr.Reading = 1;
            cdr.FirstSector = 1;

            // Fighting Force 2 - update subq time immediately
            // - fixes new game
            ReadTrack(cdr.SetSectorPlay);

            // Crusaders of Might and Magic - update getlocl now
            // - fixes cutscene speech
            {
                u8 *buf = ISO_GetBuffer();
                if (buf != NULL)
                    memcpy(cdr.Transfer, buf, 8);
            }

            /*
            Duke Nukem: Land of the Babes - seek then delay read for one frame
            - fixes cutscenes
            C-12 - Final Resistance - doesn't like seek
            */

            if (cdr.Seeked != SEEK_DONE) {
                cdr.StatP |= STATUS_SEEK;
                cdr.StatP &= ~STATUS_READ;

                // Crusaders of Might and Magic - use short time
                // - fix cutscene speech (startup)

                // ??? - use more accurate seek time later
                CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime) : cdReadTime * 2);
            } else {
                cdr.StatP |= STATUS_READ;
                cdr.StatP &= ~STATUS_SEEK;

                CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime) : cdReadTime * 2);
            }

            cdr.Result[0] = cdr.StatP;
            start_rotating = 1;
            break;

        default:
            error = ERROR_INVALIDCMD;
            // FALLTHROUGH

        set_error:
            SetResultSize(2);
            cdr.Result[0] = cdr.StatP | STATUS_ERROR;
            cdr.Result[1] = error;
            cdr.Stat = DiskError;
            break;
    }

    if (cdr.DriveState == DRIVESTATE_STOPPED && start_rotating) {
        cdr.DriveState = DRIVESTATE_STANDBY;
        cdr.StatP |= STATUS_ROTATING;
    }

    if (!no_busy_error) {
        switch (cdr.DriveState) {
        case DRIVESTATE_LID_OPEN:
        case DRIVESTATE_RESCAN_CD:
        case DRIVESTATE_PREPARE_CD:
            SetResultSize(2);
            cdr.Result[0] = cdr.StatP | STATUS_ERROR;
            cdr.Result[1] = ERROR_NOTREADY;
            cdr.Stat = DiskError;
            break;
        }
    }

finish:
    setIrq();
    cdr.ParamC = 0;
}

#define ssat32_to_16(v) do { \
    if (v < -32768) v = -32768; \
    else if (v > 32767) v = 32767; \
} while (0)

void cdrAttenuate(s16 *buf, int samples, int stereo)
{
    int i, l, r;
    int ll = cdr.AttenuatorLeftToLeft;
    int lr = cdr.AttenuatorLeftToRight;
    int rl = cdr.AttenuatorRightToLeft;
    int rr = cdr.AttenuatorRightToRight;

    if (lr == 0 && rl == 0 && 0x78 <= ll && ll <= 0x88 && 0x78 <= rr && rr <= 0x88)
        return;

    if (!stereo && ll == 0x40 && lr == 0x40 && rl == 0x40 && rr == 0x40)
        return;

    if (stereo) {
        for (i = 0; i < samples; i++) {
            l = buf[i * 2];
            r = buf[i * 2 + 1];
            l = (l * ll + r * rl) >> 7;
            r = (r * rr + l * lr) >> 7;
            ssat32_to_16(l);
            ssat32_to_16(r);
            buf[i * 2] = l;
            buf[i * 2 + 1] = r;
        }
    }
    else {
        for (i = 0; i < samples; i++) {
            l = buf[i];
            l = l * (ll + rl) >> 7;
            //r = r * (rr + lr) >> 7;
            ssat32_to_16(l);
            //ssat32_to_16(r);
            buf[i] = l;
        }
    }
}

void cdrReadInterrupt() {
    u8 *buf;

    if (!cdr.Reading)
        return;

    if (cdr.Irq || cdr.Stat) {
        CDREAD_INT(0x100);
        return;
    }

    if ((psxHu32ref(0x1070) & psxHu32ref(0x1074) & SWAP32((u32)0x4)) && !cdr.ReadRescheduled) {
        // HACK: with BIAS 2, emulated CPU is often slower than real thing,
        // game may be unfinished with prev data read, so reschedule
        // (Brave Fencer Musashi)
        CDREAD_INT(cdReadTime / 2);
        cdr.ReadRescheduled = 1;
        return;
    }

    cdr.OCUP = 1;
    SetResultSize(1);
    cdr.StatP |= STATUS_READ|STATUS_ROTATING;
    cdr.StatP &= ~STATUS_SEEK;
    cdr.Result[0] = cdr.StatP;
    cdr.Seeked = SEEK_DONE;

    ReadTrack(cdr.SetSectorPlay);

    buf = ISO_GetBuffer();
    if (buf == NULL)
        cdr.RErr = 0;

    if (cdr.RErr == 0) {
        memset(cdr.Transfer, 0, DATA_SIZE);
        cdr.Stat = DiskError;
        cdr.Result[0] |= STATUS_ERROR;
        CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);
        return;
    }

    memcpy(cdr.Transfer, buf, DATA_SIZE);

    if ((!cdr.Muted) && (cdr.Mode & MODE_STRSND) && (cdr.FirstSector != -1)) { // CD-XA
        // Firemen 2: Multi-XA files - briefings, cutscenes
        if( cdr.FirstSector == 1 && (cdr.Mode & MODE_SF)==0 ) {
            cdr.File = cdr.Transfer[4 + 0];
            cdr.Channel = cdr.Transfer[4 + 1];
        }

        if((cdr.Transfer[4 + 2] & 0x4) &&
             (cdr.Transfer[4 + 1] == cdr.Channel) &&
             (cdr.Transfer[4 + 0] == cdr.File)) {
            int ret = xa_decode_sector(&cdr.Xa, cdr.Transfer+4, cdr.FirstSector);
            if (!ret) {
                cdrAttenuate(cdr.Xa.pcm, cdr.Xa.nsamples, cdr.Xa.stereo);
                SPU_PlayADPCM_Channel(&cdr.Xa);
                cdr.FirstSector = 0;
            }
            else cdr.FirstSector = -1;
        }
    }

    cdr.SetSectorPlay[2]++;
    if (cdr.SetSectorPlay[2] == 75) {
        cdr.SetSectorPlay[2] = 0;
        cdr.SetSectorPlay[1]++;
        if (cdr.SetSectorPlay[1] == 60) {
            cdr.SetSectorPlay[1] = 0;
            cdr.SetSectorPlay[0]++;
        }
    }

    cdr.Readed = 0;
    cdr.ReadRescheduled = 0;

    CDREAD_INT((cdr.Mode & MODE_SPEED) ? (cdReadTime / 2) : cdReadTime);

    /*
    Croc 2: $40 - only FORM1 (*)
    Judge Dredd: $C8 - only FORM1 (*)
    Sim Theme Park - no adpcm at all (zero)
    */

    if (!(cdr.Mode & MODE_STRSND) || !(cdr.Transfer[4+2] & 0x4)) {
        cdr.Stat = DataReady;
        setIrq();
    }

    // update for CdlGetlocP
    ReadTrack(cdr.SetSectorPlay);
}

/*
cdrRead0:
    bit 0,1 - mode
    bit 2 - unknown
    bit 3 - unknown
    bit 4 - unknown
    bit 5 - 1 result ready
    bit 6 - 1 dma ready
    bit 7 - 1 command being processed
*/

u8 cdrRead0(void) {
    if (cdr.ResultReady)
        cdr.Ctrl |= 0x20;
    else
        cdr.Ctrl &= ~0x20;

    if (cdr.OCUP)
        cdr.Ctrl |= 0x40;
//  else
//      cdr.Ctrl &= ~0x40;

    // What means the 0x10 and the 0x08 bits? I only saw it used by the bios
    cdr.Ctrl |= 0x18;

    return psxHu8(0x1800) = cdr.Ctrl;
}

void cdrWrite0(u8 rt) {
    cdr.Ctrl = (rt & 3) | (cdr.Ctrl & ~3);
}

u8 cdrRead1(void) {
    if ((cdr.ResultP & 0xf) < cdr.ResultC)
        psxHu8(0x1801) = cdr.Result[cdr.ResultP & 0xf];
    else
        psxHu8(0x1801) = 0;
    cdr.ResultP++;
    if (cdr.ResultP == cdr.ResultC)
        cdr.ResultReady = 0;

    return psxHu8(0x1801);
}

void cdrWrite1(u8 rt) {
    char    set_loc[3];
    int     i;

    switch (cdr.Ctrl & 3) {
    case 0:
        break;
    case 3:
        cdr.AttenuatorRightToRightT = rt;
        return;
    default:
        return;
    }

    cdr.Cmd = rt;
    cdr.OCUP = 0;

    cdr.ResultReady = 0;
    cdr.Ctrl |= 0x80;
    // cdr.Stat = NoIntr;
    AddIrqQueue(cdr.Cmd, 0x800);

    switch (cdr.Cmd) {
    case CdlSetloc:
        for (i = 0; i < 3; i++)
            set_loc[i] = btoi(cdr.Param[i]);

        i = msf2sec(cdr.SetSectorPlay);
        i = abs(i - (int)msf2sec(set_loc));
        if (i > 16)
            cdr.Seeked = SEEK_PENDING;

        memcpy(cdr.SetSector, set_loc, 3);
        cdr.SetSector[3] = 0;
        cdr.SetlocPending = 1;
        break;

    case CdlReadN:
    case CdlReadS:
    case CdlPause:
        StopCdda();
        StopReading();
        break;

    case CdlReset:
    case CdlInit:
        cdr.Seeked = SEEK_DONE;
        StopCdda();
        StopReading();
        break;

    case CdlSetmode:
        cdr.Mode = cdr.Param[0];

        // Squaresoft on PlayStation 1998 Collector's CD Vol. 1
        // - fixes choppy movie sound
        if( cdr.Play && (cdr.Mode & MODE_CDDA) == 0 )
            StopCdda();
        break;
    }
}

u8 cdrRead2()
{
    u8      ret;

    if (cdr.Readed == 0) {
        ret = 0;
    } else {
        ret = cdr.Transfer[cdr.transferIndex];
        cdr.transferIndex++;
        adjustTransferIndex();
    }

    return ret;
}

void cdrWrite2(u8 rt) {
    switch (cdr.Ctrl & 3) {
    case 0:
        if (cdr.ParamC < 8) // FIXME: size and wrapping
            cdr.Param[cdr.ParamC++] = rt;
        return;
    case 1:
        cdr.Reg2 = rt;
        setIrq();
        return;
    case 2:
        cdr.AttenuatorLeftToLeftT = rt;
        return;
    case 3:
        cdr.AttenuatorRightToLeftT = rt;
        return;
    }
}

u8 cdrRead3(void)
{
    if (cdr.Ctrl & 0x1)
        psxHu8(0x1803) = cdr.Stat | 0xE0;
    else
        psxHu8(0x1803) = cdr.Reg2 | 0xE0;

    return psxHu8(0x1803);
}

void cdrWrite3(u8 rt)
{

    switch (cdr.Ctrl & 3) {
    case 0:
        break; // transfer
    case 1:
        cdr.Stat &= ~rt;

        if (rt & 0x40)
            cdr.ParamC = 0;
        return;
    case 2:
        cdr.AttenuatorLeftToRightT = rt;
        return;
    case 3:
        if (rt & 0x20) {
            memcpy(&cdr.AttenuatorLeftToLeft, &cdr.AttenuatorLeftToLeftT, 4);
        }
        return;
    }

    if ((rt & 0x80) && cdr.Readed == 0) {
        cdr.Readed = 1;
        cdr.transferIndex = 0;

        switch (cdr.Mode & (MODE_SIZE_2340|MODE_SIZE_2328)) {
            case MODE_SIZE_2328:
            case MODE_SIZE_2048:
                cdr.transferIndex += 12;
                break;

            case MODE_SIZE_2340:
                cdr.transferIndex += 0;
                break;

            default:
                break;
        }
    }
}

void psxDma3(u32 madr, u32 bcr, u32 chcr) {
    u32 cdsize;
    u32 i;
    u8 *ptr;

    switch (chcr) {
        case 0x11000000:
        case 0x11400100:
            if (cdr.Readed == 0) {
                break;
            }

            cdsize = (bcr & 0xffff) * 4;

            // Ape Escape: bcr = 0001 / 0000
            // - fix boot
            if( cdsize == 0 )
            {
                switch (cdr.Mode & (MODE_SIZE_2340|MODE_SIZE_2328)) {
                    case MODE_SIZE_2340: cdsize = 2340; break;
                    case MODE_SIZE_2328: cdsize = 2328; break;
                    default:
                    case MODE_SIZE_2048: cdsize = 2048; break;
                }
            }

            ptr = (u8 *)PSXM(madr);
            if (ptr == NULL) {
                break;
            }

            /*
            GS CDX: Enhancement CD crash
            - Setloc 0:0:0
            - CdlPlay
            - Spams DMA3 and gets buffer overrun
            */
            for(i = 0; i < cdsize; ++i) {
                ptr[i] = cdr.Transfer[cdr.transferIndex];
                cdr.transferIndex++;
                adjustTransferIndex();
            }

            // burst vs normal
            if( chcr == 0x11400100 ) {
                CDRDMA_INT( (cdsize/4) / 4 );
            }
            else if( chcr == 0x11000000 ) {
                CDRDMA_INT( (cdsize/4) * 1 );
            }
            return;

        default:
            break;
    }

    HW_DMA3_CHCR &= SWAP32(~0x01000000);
    DMA_INTERRUPT(3);
}

void cdrDmaInterrupt()
{
    if (HW_DMA3_CHCR & SWAP32(0x01000000))
    {
        HW_DMA3_CHCR &= SWAP32(~0x01000000);
        DMA_INTERRUPT(3);
    }
}

static void getCdInfo(void)
{
    u8 tmp;

    ISO_GetTN(cdr.ResultTN);
    ISO_GetTD(0, cdr.SetSectorEnd);
    tmp = cdr.SetSectorEnd[0];
    cdr.SetSectorEnd[0] = cdr.SetSectorEnd[2];
    cdr.SetSectorEnd[2] = tmp;
}

void cdrReset() {
    memset(&cdr, 0, sizeof(cdr));
    cdr.CurTrack = 1;
    cdr.File = 1;
    cdr.Channel = 1;
    cdr.transferIndex = 0;
    cdr.Reg2 = 0x1f;
    cdr.Stat = NoIntr;
    cdr.DriveState = DRIVESTATE_STANDBY;
    cdr.StatP = STATUS_ROTATING;

    // BIOS player - default values
    cdr.AttenuatorLeftToLeft = 0x80;
    cdr.AttenuatorLeftToRight = 0x00;
    cdr.AttenuatorRightToLeft = 0x00;
    cdr.AttenuatorRightToRight = 0x80;

    getCdInfo();
}

void CDR_LidInterrupt()
{
    getCdInfo();
    StopCdda();
    cdrLidSeekInterrupt();
}
