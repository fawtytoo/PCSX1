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
* SIO functions.
*/

#include "system.h"
#include "pad.h"

#include "sio.h"

// Status Flags
#define TX_RDY      0x0001
#define RX_RDY      0x0002
#define TX_EMPTY    0x0004
#define PARITY_ERR  0x0008
#define RX_OVERRUN  0x0010
#define FRAMING_ERR 0x0020
#define SYNC_DETECT 0x0040
#define DSR         0x0080
#define CTS         0x0100
#define IRQ         0x0200

// Control Flags
#define TX_PERM     0x0001
#define DTR         0x0002
#define RX_PERM     0x0004
#define BREAK       0x0008
#define RESET_ERR   0x0010
#define RTS         0x0020
#define SIO_RESET   0x0040

// MCD flags
#define MCDST_CHANGED 0x08

// *** FOR WORKS ON PADS AND MEMORY CARDS *****

typedef struct stat STATUS;

void LoadDongle( char *str );
void SaveDongle( char *str );

#define BUFFER_SIZE 0x1010

static u8       buf[ BUFFER_SIZE ];

//[0] -> dummy
//[1] -> memory card status flag
//[2] -> card 1 id, 0x5a->plugged, any other not plugged
//[3] -> card 2 id, 0x5d->plugged, any other not plugged
u8              cardh[4] = {0x00, 0x00, 0x00, 0x00};

// Transfer Ready and the Buffer is Empty
// static unsigned short StatReg = 0x002b;
static u16      StatReg = TX_RDY | TX_EMPTY;
static u16      ModeReg;
static u16      CtrlReg;
static u16      BaudReg;

static u32      bufcount;
static u32      parp;
static u32      mcdst, rdwr;
static u8       adrH, adrL;
static u32      padst;
static u32      gsdonglest;

char    MemCard[2][MCD_SIZE];
int     mcdAmended[2] = {0, 0};

#define DONGLE_SIZE 0x40 * 0x1000

u32         DongleBank;
u8          DongleData[ DONGLE_SIZE ];
static int DongleInit;

#if 0
// Breaks Twisted Metal 2 intro
#define SIO_INT(eCycle) { \
    if (!Config.SioIrq) { \
        psxRegs.interrupt |= (1 << PSXINT_SIO); \
        psxRegs.intCycle[PSXINT_SIO].cycle = eCycle; \
        psxRegs.intCycle[PSXINT_SIO].sCycle = psxRegs.cycle; \
    } \
    \
    StatReg &= ~RX_RDY; \
    StatReg &= ~TX_RDY; \
}
#endif

#define SIO_INT(eCycle) { \
    if (!Config.SioIrq) { \
        psxRegs.interrupt |= (1 << PSXINT_SIO); \
        psxRegs.intCycle[PSXINT_SIO].cycle = eCycle; \
        psxRegs.intCycle[PSXINT_SIO].sCycle = psxRegs.cycle; \
    } \
}

// clk cycle byte
// 4us * 8bits = (PSXCLK / 1000000) * 32; (linuzappz)
// TODO: add SioModePrescaler
#define SIO_CYCLES (BaudReg * 8)

// rely on this for now - someone's actual testing
//#define SIO_CYCLES (PSXCLK / 57600)
//PCSX 1.9.91
//#define SIO_CYCLES 200
//PCSX 1.9.91
//#define SIO_CYCLES 270
// ePSXe 1.6.0
//#define SIO_CYCLES        535
// ePSXe 1.7.0
//#define SIO_CYCLES 635

u8 reverse_8(u8 bits)
{
    u8      tmp;
    int lcv;

    tmp = 0;
    for( lcv = 0; lcv < 8; lcv++ )
    {
        tmp >>= 1;
        tmp |= (bits & 0x80);

        bits <<= 1;
    }

    return tmp;
}

void sioWrite8(u8 value)
{
    switch (padst) {
        case 1: SIO_INT(SIO_CYCLES);
            /*
            $41-4F
            $41 = Find bits in poll respones
            $42 = Polling command
            $43 = Config mode (Dual shock?)
            $44 = Digital / Analog (after $F3)
            $45 = Get status info (Dual shock?)

            ID:
            $41 = Digital
            $73 = Analogue Red LED
            $53 = Analogue Green LED

            $23 = NegCon
            $12 = Mouse
            */

            if ((value & 0x40) == 0x40) {
                padst = 2; parp = 1;
                if (!Config.UseNet) {
                    switch (CtrlReg & 0x2002) {
                        case 0x0002:
                            buf[parp] = PAD_Poll(value);
                            break;
                        case 0x2002:
                            buf[parp] = PAD_Poll(value);
                            break;
                    }
                }/* else {
//                  SysPrintf("%x: %x, %x, %x, %x\n", CtrlReg&0x2002, buf[2], buf[3], buf[4], buf[5]);
                }*/

                if (!(buf[parp] & 0x0f)) {
                    bufcount = 2 + 32;
                } else {
                    bufcount = 2 + (buf[parp] & 0x0f) * 2;
                }

                // Digital / Dual Shock Controller
                if (buf[parp] == 0x41) {
                    switch (value) {
                        // enter config mode
                        case 0x43:
                            buf[1] = 0x43;
                            break;

                        // get status
                        case 0x45:
                            buf[1] = 0xf3;
                            break;
                    }
                }

                // NegCon - Wipeout 3
                if( buf[parp] == 0x23 ) {
                    switch (value) {
                        // enter config mode
                        case 0x43:
                            buf[1] = 0x79;
                            break;

                        // get status
                        case 0x45:
                            buf[1] = 0xf3;
                            break;
                    }
                }
            }
            else padst = 0;
            return;
        case 2:
            parp++;
/*          if (buf[1] == 0x45) {
                buf[parp] = 0;
                SIO_INT(SIO_CYCLES);
                return;
            }*/
            if (!Config.UseNet) {
                switch (CtrlReg & 0x2002) {
                    case 0x0002: buf[parp] = PAD_Poll(value); break;
                    case 0x2002: buf[parp] = PAD_Poll(value); break;
                }
            }

            if (parp == bufcount) { padst = 0; return; }
            SIO_INT(SIO_CYCLES);
            return;
    }

    switch (mcdst) {
        case 1:
            SIO_INT(SIO_CYCLES);
            if (rdwr) { parp++; return; }
            parp = 1;
            switch (value) {
                case 0x52: rdwr = 1; break;
                case 0x57: rdwr = 2; break;
                default: mcdst = 0;
            }
            return;
        case 2: // address H
            SIO_INT(SIO_CYCLES);
            adrH = value;
            *buf = 0;
            parp = 0;
            bufcount = 1;
            mcdst = 3;
            return;
        case 3: // address L
            SIO_INT(SIO_CYCLES);
            adrL = value;
            *buf = adrH;
            parp = 0;
            bufcount = 1;
            mcdst = 4;
            return;
        case 4:
            SIO_INT(SIO_CYCLES);
            parp = 0;
            switch (rdwr) {
                case 1: // read
                    buf[0] = 0x5c;
                    buf[1] = 0x5d;
                    buf[2] = adrH;
                    buf[3] = adrL;
                    switch (CtrlReg & 0x2002) {
                        case 0x0002:
                            memcpy(&buf[4], MemCard[0] + (adrL | (adrH << 8)) * 128, 128);
                            break;
                        case 0x2002:
                            memcpy(&buf[4], MemCard[1] + (adrL | (adrH << 8)) * 128, 128);
                            break;
                    }
                    {
                    char xorsum = 0;
                    int i;
                    for (i = 2; i < 128 + 4; i++)
                        xorsum ^= buf[i];
                    buf[132] = xorsum;
                    }
                    buf[133] = 0x47;
                    bufcount = 133;
                    break;
                case 2: // write
                    buf[0] = adrL;
                    buf[1] = value;
                    buf[129] = 0x5c;
                    buf[130] = 0x5d;
                    buf[131] = 0x47;
                    bufcount = 131;
                    cardh[1] &= ~MCDST_CHANGED;
                    break;
            }
            mcdst = 5;
            return;
        case 5:
            parp++;
            if (rdwr == 2) {
                if (parp < 128) buf[parp + 1] = value;
            }
            SIO_INT(SIO_CYCLES);
            return;
    }

    /*
    GameShark CDX

    ae - be - ef - 04 + [00]
    ae - be - ef - 01 + 00 + [00] * $1000
    ae - be - ef - 01 + 42 + [00] * $1000
    ae - be - ef - 03 + 01,01,1f,e3,85,ae,d1,28 + [00] * 4
    */
    switch (gsdonglest) {
        // main command loop
        case 1:
            SIO_INT( SIO_CYCLES );

            // GS CDX
            // - unknown output

            // reset device when fail?
            if( value == 0xae )
            {
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;
            }

            // GS CDX
            else if( value == 0xbe )
            {
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;

                buf[0] = reverse_8( 0xde );
            }

            // GS CDX
            else if( value == 0xef )
            {
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;

                buf[0] = reverse_8( 0xad );
            }

            // GS CDX [1 in + $1000 out + $1 out]
            else if( value == 0x01 )
            {
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;

                // $00 = 0000 0000
                // - (reverse) 0000 0000
                buf[0] = 0x00;
                gsdonglest = 2;
            }

            // GS CDX [1 in + $1000 in + $1 out]
            else if( value == 0x02 )
            {
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;

                // $00 = 0000 0000
                // - (reverse) 0000 0000
                buf[0] = 0x00;
                gsdonglest = 3;
            }

            // GS CDX [8 in, 4 out]
            else if( value == 0x03 )
            {
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;

                // $00 = 0000 0000
                // - (reverse) 0000 0000
                buf[0] = 0x00;

                gsdonglest = 4;
            }

            // GS CDX [out 1]
            else if( value == 0x04 )
            {
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;

                // $00 = 0000 0000
                // - (reverse) 0000 0000
                buf[0] = 0x00;
                gsdonglest = 5;
            }
            else
            {
                // ERROR!!
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;
                buf[0] = 0xff;

                gsdonglest = 0;
            }

            return;

        // be - ef - 01
        case 2: {
            u8      checksum;
            u32     lcv;

            SIO_INT( SIO_CYCLES );
            StatReg |= RX_RDY;

            // read 1 byte
            DongleBank = buf[ 0 ];

            // write data + checksum
            checksum = 0;
            for( lcv = 0; lcv < 0x1000; lcv++ )
            {
                u8      data;

                data = DongleData[ DongleBank * 0x1000 + lcv ];

                buf[ lcv+1 ] = reverse_8( data );
                checksum += data;
            }

            parp = 0;
            bufcount = 0x1001;
            buf[ 0x1001 ] = reverse_8( checksum );

            gsdonglest = 255;
            return;
        }

        // be - ef - 02
        case 3:
            SIO_INT( SIO_CYCLES );
            StatReg |= RX_RDY;

            // command start
            if( parp < 0x1000+1 )
            {
                // read 1 byte
                buf[ parp ] = value;
                parp++;
            }

            if( parp == 0x1001 )
            {
                u8      checksum;
                u32     lcv;

                DongleBank = buf[0];
                memcpy( DongleData + DongleBank * 0x1000, buf+1, 0x1000 );

                // save to file
                SaveDongle( "memcards/CDX_Dongle.bin" );

                // write 8-bit checksum
                checksum = 0;
                for( lcv = 1; lcv < 0x1001; lcv++ )
                {
                    checksum += buf[ lcv ];
                }

                parp = 0;
                bufcount = 1;
                buf[1] = reverse_8( checksum );

                // flush result
                gsdonglest = 255;
            }
            return;

        // be - ef - 03
        case 4:
            SIO_INT( SIO_CYCLES );
            StatReg |= RX_RDY;

            // command start
            if( parp < 8 )
            {
                // read 2 (?,?) + 4 (DATA?) + 2 (CRC?)
                buf[ parp ] = value;
                parp++;
            }

            if( parp == 8 )
            {
                // now write 4 bytes via -FOUR- $00 writes
                parp = 8;
                bufcount = 12;

                // TODO: Solve CDX algorithm

                // GS CDX [magic key]
                if( buf[2] == 0x12 && buf[3] == 0x34 &&
                        buf[4] == 0x56 && buf[5] == 0x78 )
                {
                    buf[9] = reverse_8( 0x3e );
                    buf[10] = reverse_8( 0xa0 );
                    buf[11] = reverse_8( 0x40 );
                    buf[12] = reverse_8( 0x29 );
                }

                // GS CDX [address key #2 = 6ec]
                else if( buf[2] == 0x1f && buf[3] == 0xe3 &&
                                 buf[4] == 0x45 && buf[5] == 0x60 )
                {
                    buf[9] = reverse_8( 0xee );
                    buf[10] = reverse_8( 0xdd );
                    buf[11] = reverse_8( 0x71 );
                    buf[12] = reverse_8( 0xa8 );
                }

                // GS CDX [address key #3 = ???]
                else if( buf[2] == 0x1f && buf[3] == 0xe3 &&
                                 buf[4] == 0x72 && buf[5] == 0xe3 )
                {
                    // unsolved!!

                    // Used here: 80090348 / 80090498

                    // dummy value - MSB
                    buf[9] = reverse_8( 0xfa );
                    buf[10] = reverse_8( 0xde );
                    buf[11] = reverse_8( 0x21 );
                    buf[12] = reverse_8( 0x97 );
                }

                // GS CDX [address key #4 = a00]
                else if( buf[2] == 0x1f && buf[3] == 0xe3 &&
                                 buf[4] == 0x85 && buf[5] == 0xae )
                {
                    buf[9] = reverse_8( 0xee );
                    buf[10] = reverse_8( 0xdd );
                    buf[11] = reverse_8( 0x7d );
                    buf[12] = reverse_8( 0x44 );
                }

                // GS CDX [address key #5 = 9ec]
                else if( buf[2] == 0x17 && buf[3] == 0xe3 &&
                                 buf[4] == 0xb5 && buf[5] == 0x60 )
                {
                    buf[9] = reverse_8( 0xee );
                    buf[10] = reverse_8( 0xdd );
                    buf[11] = reverse_8( 0x7e );
                    buf[12] = reverse_8( 0xa8 );
                }

                else
                {
                    // dummy value - MSB
                    buf[9] = reverse_8( 0xfa );
                    buf[10] = reverse_8( 0xde );
                    buf[11] = reverse_8( 0x21 );
                    buf[12] = reverse_8( 0x97 );
                }

                // flush bytes -> done
                gsdonglest = 255;
            }
            return;

        // be - ef - 04
        case 5:
            if( value == 0x00 )
            {
                SIO_INT( SIO_CYCLES );
                StatReg |= RX_RDY;

                // read 1 byte
                parp = 0;
                bufcount = parp;

                // size of dongle card?
                buf[ 0 ] = reverse_8( DONGLE_SIZE / 0x1000 );

                // done already
                gsdonglest = 0;
            }
            return;

        // flush bytes -> done
        case 255:
            if( value == 0x00 )
            {
                //SIO_INT( SIO_CYCLES );
                SIO_INT(1);
                StatReg |= RX_RDY;

                parp++;
                if( parp == bufcount )
                {
                    gsdonglest = 0;
                }
            }
            else
            {
                // ERROR!!
                StatReg |= RX_RDY;

                parp = 0;
                bufcount = parp;
                buf[0] = 0xff;

                gsdonglest = 0;
            }
            return;
    }

    switch (value) {
        case 0x01: // start pad
            StatReg |= RX_RDY;      // Transfer is Ready

            if (!Config.UseNet) {
                switch (CtrlReg & 0x2002) {
                    case 0x0002: buf[0] = PAD_StartPoll(0); break;
                    case 0x2002: buf[0] = PAD_StartPoll(1); break;
                }
            } else {
                if ((CtrlReg & 0x2002) == 0x0002) {
                    int i, j;

                    PAD_StartPoll(0);
                    buf[0] = 0;
                    buf[1] = PAD_Poll(0x42);
                    if (!(buf[1] & 0x0f)) {
                        bufcount = 32;
                    } else {
                        bufcount = (buf[1] & 0x0f) * 2;
                    }
                    buf[2] = PAD_Poll(0);
                    i = 3;
                    j = bufcount;
                    while (j--) {
                        buf[i++] = PAD_Poll(0);
                    }
                    bufcount+= 3;

#ifdef ENABLE_NET
                    if (NET_sendPadData(buf, bufcount) == -1)
                        netError();

                    if (NET_recvPadData(buf, 1) == -1)
                        netError();
                    if (NET_recvPadData(buf + 128, 2) == -1)
                        netError();
#endif
                } else {
                    memcpy(buf, buf + 128, 32);
                }
            }

            bufcount = 2;
            parp = 0;
            padst = 1;
            SIO_INT(SIO_CYCLES);
            return;
        case 0x81: // start memcard
        //case 0x82: case 0x83: case 0x84: // Multitap memcard access
            StatReg |= RX_RDY;
            memcpy(buf, cardh, 4);
            parp = 0;
            bufcount = 3;
            mcdst = 1;
            rdwr = 0;
            SIO_INT(SIO_CYCLES);
            return;
        case 0xae: // GameShark CDX - start dongle
            StatReg |= RX_RDY;
            gsdonglest = 1;

            parp = 0;
            bufcount = parp;

            if( !DongleInit )
            {
                LoadDongle( "memcards/CDX_Dongle.bin" );

                DongleInit = 1;
            }

            SIO_INT( SIO_CYCLES );
            return;

        default: // no hardware found
            StatReg |= RX_RDY;
            return;
    }
}

void sioWriteStat16(u16 value) {
    (void)value;
}

void sioWriteMode16(u16 value) {
    ModeReg = value;
}

void sioWriteCtrl16(u16 value) {
    CtrlReg = value & ~RESET_ERR;
    if (value & RESET_ERR) StatReg &= ~IRQ;
    if ((CtrlReg & SIO_RESET) || (!CtrlReg)) {
        padst = 0; mcdst = 0; parp = 0;
        StatReg = TX_RDY | TX_EMPTY;
        psxRegs.interrupt &= ~(1 << PSXINT_SIO);
    }
}

void sioWriteBaud16(u16 value) {
    BaudReg = value;
}

u8 sioRead8()
{
    u8      ret = 0;

    if ((StatReg & RX_RDY)/* && (CtrlReg & RX_PERM)*/) {
//      StatReg &= ~RX_OVERRUN;
        ret = buf[parp];
        if (parp == bufcount) {
            StatReg &= ~RX_RDY;     // Receive is not Ready now
            if (mcdst == 5) {
                mcdst = 0;
                if (rdwr == 2) {
                    switch (CtrlReg & 0x2002) {
                        case 0x0002:
                            memcpy(MemCard[0] + (adrL | (adrH << 8)) * 128, &buf[1], 128);
                            mcdAmended[0] = 1;
                            break;
                        case 0x2002:
                            memcpy(MemCard[1] + (adrL | (adrH << 8)) * 128, &buf[1], 128);
                            mcdAmended[1] = 1;
                            break;
                    }
                }
            }
            if (padst == 2) padst = 0;
            if (mcdst == 1) {
                mcdst = 2;
                StatReg|= RX_RDY;
            }
        }
    }

    return ret;
}

u16 sioReadStat16() {
    u16 hard;

    hard = StatReg;

#if 0
    // wait for IRQ first
    if( psxRegs.interrupt & (1 << PSXINT_SIO) )
    {
        hard &= ~TX_RDY;
        hard &= ~RX_RDY;
        hard &= ~TX_EMPTY;
    }
#endif

    return hard;
}

u16 sioReadMode16() {
    return ModeReg;
}

u16 sioReadCtrl16() {
    return CtrlReg;
}

u16 sioReadBaud16() {
    return BaudReg;
}

void netError() {
    //ClosePlugins();
    SysPrintf("%s", "Connection closed!\n");

    CdromId[0] = '\0';
    CdromLabel[0] = '\0';

    //SysRunGui();
}

void sioInterrupt() {
//  SysPrintf("Sio Interrupt\n");
    StatReg |= IRQ;
    psxHu32ref(0x1070) |= SWAPu32(0x80);

#if 0
    // Rhapsody: fixes input problems
    // Twisted Metal 2: breaks intro
    StatReg |= TX_RDY;
    StatReg |= RX_RDY;
#endif
}

void MCD_Load(int mcd, char *filename)
{
    int     error;
    char    *data = MemCard[mcd];

    // flag indicating entries have not yet been read (i.e. new card plugged)
    if (mcd == 0)
    {
        cardh[2] = 0x5a;
    }
    else
    {
        cardh[3] = 0x5d;
    }
    cardh[1] |= MCDST_CHANGED;

    if ((error = File_Load(filename, &data, &(int){MCD_SIZE})) == 0)
    {
        SysPrintf("Loading memory card %i: %s\n", mcd + 1, filename);
    }
    else if (error == 1)
    {
        SysPrintf("Unable to open memory card - card %i is not plugged\n", mcd + 1);
    }

    mcdAmended[mcd] = 0;
}

void MCD_Save(int mcd, char *filename)
{
    if (mcdAmended[mcd])
    {
        File_Save(filename, MemCard[mcd], MCD_SIZE);
        SysPrintf("Saving memory card %i: %s\n", mcd + 1, filename);
    }
}

void MCD_Format(int mcd)
{
    char    *memcard = MemCard[mcd];
    int     i, j, b = 0;

    memcard[b++] = 'M';
    memcard[b++] = 'C';

    for (i = 0; i < 125; i++)
    {
        memcard[b++] = '\x00';
    }

    memcard[b++] = '\x0e'; // checksum

    for (i = 0; i < 15; i++) // 15 blocks
    {
        memcard[b++] = '\xa0';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\xff';
        memcard[b++] = '\xff';

        for (j = 0; j < 117; j++)
        {
            memcard[b++] = '\x00';
        }

        memcard[b++] = '\xa0'; // checksum
    }

    for (i = 0; i < 20; i++)
    {
        memcard[b++] = '\xff';
        memcard[b++] = '\xff';
        memcard[b++] = '\xff';
        memcard[b++] = '\xff';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\x00';
        memcard[b++] = '\xff';
        memcard[b++] = '\xff';

        for (j = 0; j < 118; j++)
        {
            memcard[b++] = '\x00';
        }
    }

    while (b < MCD_SIZE)
    {
        memcard[b++] = '\x00';
    }

    mcdAmended[mcd] = 1;
}

void LoadDongle( char *str )
{
    if (File_Load(str, (char **)DongleData, &(int){DONGLE_SIZE}) == 0)
    {
        return;
    }

    u32 *ptr, lcv;

    ptr = (u32 *) DongleData;

    // create temp data
    ptr[0] = (u32) 0x02015447;
    ptr[1] = (u32) 7;
    ptr[2] = (u32) 1;
    ptr[3] = (u32) 0;

    for( lcv=4; lcv<0x6c / 4; lcv++ )
    {
        ptr[ lcv ] = 0;
    }

    ptr[ lcv ] = (u32) 0x02000100;
    lcv++;

    while( lcv < 0x1000/4 )
    {
        ptr[ lcv ] = (u32) 0xffffffff;
        lcv++;
    }
}

void SaveDongle( char *str )
{
    File_Save(str, (char *)DongleData, DONGLE_SIZE);
}

void SIO1irq()
{
    psxHu32ref(0x1070) |= SWAPu32(0x100);
}
