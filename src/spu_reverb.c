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

#include "spu_externals.h"

int iUseReverb = 0;

REVERB  rvb;

// REVERB info and timing vars...
int sRVBStart[88200 * 2];
int *sRVBPlay = sRVBStart;
int *sRVBEnd = sRVBStart;
int iReverbOff = -1; // some delay factor for reverb
int iReverbRepeat = 0;
int iReverbNum = 1;

void SetREVERB(u16 val)
{
    switch (val)
    {
      case 0x0000: // off
        iReverbOff = -1;
        break;

      case 0x007D: // ok room
        iReverbOff = 32;
        iReverbNum = 2;
        iReverbRepeat = 128;
        break;

      case 0x0033: // studio small
        iReverbOff = 32;
        iReverbNum = 2;
        iReverbRepeat = 64;
        break;

      case 0x00B1: // ok studio medium
        iReverbOff = 48;
        iReverbNum = 2;
        iReverbRepeat = 96;
        break;

      case 0x00E3: // ok studio large ok
        iReverbOff = 64;
        iReverbNum = 2;
        iReverbRepeat = 128;
        break;

      case 0x01A5: // ok hall
        iReverbOff = 128;
        iReverbNum = 4;
        iReverbRepeat = 32;
        break;

      case 0x033D: // space echo
        iReverbOff = 256;
        iReverbNum = 4;
        iReverbRepeat = 64;
        break;

      case 0x0001: // echo/delay
        iReverbOff = 184;
        iReverbNum = 3;
        iReverbRepeat = 128;
        break;

      case 0x0017: // half echo
        iReverbOff = 128;
        iReverbNum = 2;
        iReverbRepeat = 128;
        break;

      default:
        iReverbOff = 32;
        iReverbNum = 1;
        iReverbRepeat =0 ;
        break;
    }
}

void StartREVERB(int ch)
{
    if (s_chan[ch].bReverb && (spuCtrl & 0x80)) // reverb possible?
    {
        if (iUseReverb == 2)
            s_chan[ch].bRVBActive = 1;
        else if (iUseReverb == 1 && iReverbOff > 0) // -> fake reverb used?
        {
            s_chan[ch].bRVBActive = 1; // -> activate it
            s_chan[ch].iRVBOffset = iReverbOff * 45;
            s_chan[ch].iRVBRepeat = iReverbRepeat * 45;
            s_chan[ch].iRVBNum = iReverbNum;
        }
    }
    else
        s_chan[ch].bRVBActive = 0; // else -> no reverb
}

void InitREVERB()
{
    if (iUseReverb == 2)
    {
        memset(sRVBStart, 0, 2 * sizeof(int));
    }
}

void StoreREVERB_CD(int left, int right)
{
    if (iUseReverb == 0)
        return;

    if (iUseReverb == 2) // Neil's reverb
    {
        const int   iRxl = left;
        const int   iRxr = right;

        // -> we mix all active reverb channels into an extra buffer
        *sRVBStart += CLAMP16(*sRVBStart + (iRxl));
        *(sRVBStart + 1) += CLAMP16(*(sRVBStart + 1) + (iRxr));
    }
}

void StoreREVERB(int ch)
{
    if (iUseReverb == 0)
        return;

    if (iUseReverb == 2) // Neil's reverb
    {
        const int   iRxl = (s_chan[ch].sval * s_chan[ch].iLeftVolume) / 0x4000;
        const int   iRxr = (s_chan[ch].sval * s_chan[ch].iRightVolume) / 0x4000;

        *sRVBStart += iRxl; // -> we mix all active reverb channels into an extra buffer
        *(sRVBStart + 1) += iRxr;
    }
    else // Pete's easy fake reverb
    {
        int *pN;
        int iRn, iRr = 0;

        // we use the half channel volume (/0x8000) for the first reverb effects, quarter for next and so on

        int iRxl = (s_chan[ch].sval * s_chan[ch].iLeftVolume) / 0x8000;
        int iRxr = (s_chan[ch].sval * s_chan[ch].iRightVolume) / 0x8000;

        for (iRn = 1; iRn <= s_chan[ch].iRVBNum; iRn++, iRr += s_chan[ch].iRVBRepeat, iRxl /= 2, iRxr /= 2)
        {
            pN = sRVBPlay + ((s_chan[ch].iRVBOffset + iRr) << 1);
            if (pN >= sRVBEnd)
                pN = sRVBStart + (pN - sRVBEnd);

            (*pN) += iRxl;
            pN++;
            (*pN) += iRxr;
        }
    }
}

int g_buffer(int iOff) // get_buffer content helper: takes care about wraps
{
    short   *p = (short *)spuMem;

    iOff = (iOff * 4) + rvb.CurrAddr;
    if (iOff > 0x3FFFF)
        iOff = rvb.StartAddr + iOff - 0x40000;

    if (iOff < rvb.StartAddr)
        iOff = 0x3ffff - (rvb.StartAddr - iOff);

    return (int)*(p + iOff);
}

static void s_buffer(int iOff, int iVal) // set_buffer content helper: takes care about wraps and clipping
{
    short   *p = (short *)spuMem;

    iOff = (iOff * 4) + rvb.CurrAddr;
    if (iOff > 0x3FFFF)
        iOff = rvb.StartAddr + iOff - 0x40000;

    if (iOff < rvb.StartAddr)
        iOff = 0x3ffff - (rvb.StartAddr - iOff);

    *(p + iOff) = (short)CLAMP16(iVal);
}

static void s_buffer1(int iOff, int iVal) // set_buffer (+1 sample) content helper: takes care about wraps and clipping
{
    short   *p = (short *)spuMem;

    iOff = (iOff * 4) + rvb.CurrAddr + 1;
    if (iOff > 0x3FFFF)
        iOff = rvb.StartAddr + iOff - 0x40000;

    if (iOff < rvb.StartAddr)
        iOff = 0x3ffff - (rvb.StartAddr - iOff);

    *(p + iOff) = (short)CLAMP16(iVal);
}

int MixREVERBLeft()
{
    if (iUseReverb == 0)
        return 0;

    if (iUseReverb == 2)
    {
        static int  iCnt = 0; // this func will be called with 44.1 khz

        if (!rvb.StartAddr) // reverb is off
        {
            rvb.iLastRVBLeft = rvb.iLastRVBRight = rvb.iRVBLeft = rvb.iRVBRight = 0;
            return 0;
        }

        iCnt++;

        if (iCnt & 1) // we work on every second left value: downsample to 22 khz
        {
            if (spuCtrl & 0x80) // -> reverb on? oki
            {
                int ACC0, ACC1, FB_A0, FB_A1, FB_B0, FB_B1;

                const int   INPUT_SAMPLE_L = *sRVBStart;
                const int   INPUT_SAMPLE_R = *(sRVBStart + 1);

                const int   IIR_INPUT_A0 = (g_buffer(rvb.IIR_SRC_A0) * rvb.IIR_COEF) / 32768L + (INPUT_SAMPLE_L * rvb.IN_COEF_L) / 32768L;
                const int   IIR_INPUT_A1 = (g_buffer(rvb.IIR_SRC_A1) * rvb.IIR_COEF) / 32768L + (INPUT_SAMPLE_R * rvb.IN_COEF_R) / 32768L;
                const int   IIR_INPUT_B0 = (g_buffer(rvb.IIR_SRC_B0) * rvb.IIR_COEF) / 32768L + (INPUT_SAMPLE_L * rvb.IN_COEF_L) / 32768L;
                const int   IIR_INPUT_B1 = (g_buffer(rvb.IIR_SRC_B1) * rvb.IIR_COEF) / 32768L + (INPUT_SAMPLE_R * rvb.IN_COEF_R) / 32768L;

                const int   IIR_A0 = (IIR_INPUT_A0 * rvb.IIR_ALPHA) / 32768L + (g_buffer(rvb.IIR_DEST_A0) * (32768L - rvb.IIR_ALPHA)) / 32768L;
                const int   IIR_A1 = (IIR_INPUT_A1 * rvb.IIR_ALPHA) / 32768L + (g_buffer(rvb.IIR_DEST_A1) * (32768L - rvb.IIR_ALPHA)) / 32768L;
                const int   IIR_B0 = (IIR_INPUT_B0 * rvb.IIR_ALPHA) / 32768L + (g_buffer(rvb.IIR_DEST_B0) * (32768L - rvb.IIR_ALPHA)) / 32768L;
                const int   IIR_B1 = (IIR_INPUT_B1 * rvb.IIR_ALPHA) / 32768L + (g_buffer(rvb.IIR_DEST_B1) * (32768L - rvb.IIR_ALPHA)) / 32768L;

                s_buffer1(rvb.IIR_DEST_A0, IIR_A0);
                s_buffer1(rvb.IIR_DEST_A1, IIR_A1);
                s_buffer1(rvb.IIR_DEST_B0, IIR_B0);
                s_buffer1(rvb.IIR_DEST_B1, IIR_B1);

                ACC0 = (g_buffer(rvb.ACC_SRC_A0) * rvb.ACC_COEF_A) / 32768L +
                      (g_buffer(rvb.ACC_SRC_B0) * rvb.ACC_COEF_B) / 32768L +
                      (g_buffer(rvb.ACC_SRC_C0) * rvb.ACC_COEF_C) / 32768L +
                      (g_buffer(rvb.ACC_SRC_D0) * rvb.ACC_COEF_D) / 32768L;
                ACC1 = (g_buffer(rvb.ACC_SRC_A1) * rvb.ACC_COEF_A) / 32768L +
                      (g_buffer(rvb.ACC_SRC_B1) * rvb.ACC_COEF_B) / 32768L +
                      (g_buffer(rvb.ACC_SRC_C1) * rvb.ACC_COEF_C) / 32768L +
                      (g_buffer(rvb.ACC_SRC_D1) * rvb.ACC_COEF_D) / 32768L;

                FB_A0 = g_buffer(rvb.MIX_DEST_A0 - rvb.FB_SRC_A);
                FB_A1 = g_buffer(rvb.MIX_DEST_A1 - rvb.FB_SRC_A);
                FB_B0 = g_buffer(rvb.MIX_DEST_B0 - rvb.FB_SRC_B);
                FB_B1 = g_buffer(rvb.MIX_DEST_B1 - rvb.FB_SRC_B);

                s_buffer(rvb.MIX_DEST_A0, ACC0 - (FB_A0 * rvb.FB_ALPHA) / 32768L);
                s_buffer(rvb.MIX_DEST_A1, ACC1 - (FB_A1 * rvb.FB_ALPHA) / 32768L);

                s_buffer(rvb.MIX_DEST_B0, (rvb.FB_ALPHA * ACC0) / 32768L - (FB_A0 * (int)(rvb.FB_ALPHA ^ 0xFFFF8000)) / 32768L - (FB_B0 * rvb.FB_X) / 32768L);
                s_buffer(rvb.MIX_DEST_B1, (rvb.FB_ALPHA * ACC1) / 32768L - (FB_A1 * (int)(rvb.FB_ALPHA ^ 0xFFFF8000)) / 32768L - (FB_B1 * rvb.FB_X) / 32768L);

                rvb.iLastRVBLeft  = rvb.iRVBLeft;
                rvb.iLastRVBRight = rvb.iRVBRight;

                rvb.iRVBLeft  = (g_buffer(rvb.MIX_DEST_A0) + g_buffer(rvb.MIX_DEST_B0)) / 3;
                rvb.iRVBRight = (g_buffer(rvb.MIX_DEST_A1) + g_buffer(rvb.MIX_DEST_B1)) / 3;

                rvb.iRVBLeft  = (rvb.iRVBLeft  * rvb.VolLeft)  / 0x4000;
                rvb.iRVBRight = (rvb.iRVBRight * rvb.VolRight) / 0x4000;

                rvb.CurrAddr++;
                if (rvb.CurrAddr > 0x3ffff)
                    rvb.CurrAddr = rvb.StartAddr;

                return rvb.iLastRVBLeft + (rvb.iRVBLeft - rvb.iLastRVBLeft) / 2;
            }
            else // -> reverb off
            {
                // Vib Ribbon - grab current reverb sample (cdda data)
                // - mono data

                rvb.iRVBLeft = (short)spuMem[rvb.CurrAddr];
                rvb.iRVBRight = rvb.iRVBLeft;
                rvb.iLastRVBLeft = (rvb.iRVBLeft * rvb.VolLeft) / 0x4000;
                rvb.iLastRVBRight = (rvb.iRVBRight * rvb.VolRight) / 0x4000;

                //rvb.iLastRVBLeft=rvb.iLastRVBRight=rvb.iRVBLeft=rvb.iRVBRight=0;
          }

            Check_IRQ(rvb.CurrAddr * 2, 0);

            rvb.CurrAddr++;
            if (rvb.CurrAddr > 0x3ffff)
                rvb.CurrAddr = rvb.StartAddr;
        }

        return rvb.iLastRVBLeft;
    }
    else // easy fake reverb:
    {
        const int   iRV = *sRVBPlay; // -> simply take the reverb mix buf value
        *sRVBPlay++ = 0; // -> init it after
        if (sRVBPlay >= sRVBEnd)
            sRVBPlay = sRVBStart; // -> and take care about wrap arounds
        return iRV; // -> return reverb mix buf val
    }
}

int MixREVERBRight()
{
    if (iUseReverb == 0)
        return 0;

    if (iUseReverb == 2) // Neill's reverb:
    {
        // Vib Ribbon - reverb always on (!), reverb write flag
        if (spuCtrl & CTRL_REVERB) // -> reverb on? oki
        {
            int i = rvb.iLastRVBRight + (rvb.iRVBRight - rvb.iLastRVBRight) / 2;
            rvb.iLastRVBRight = rvb.iRVBRight;
            return i; // -> just return the last right reverb val (little bit scaled by the previous right val)
        }
        else
        {
            // Vib Ribbon - return reverb buffer (cdda data)
            return CLAMP16(rvb.iLastRVBRight);
        }
    }
    else // easy fake reverb:
    {
        const int   iRV = *sRVBPlay; // -> simply take the reverb mix buf value
        *sRVBPlay++ = 0; // -> init it after
        if (sRVBPlay >= sRVBEnd)
            sRVBPlay = sRVBStart; // -> and take care about wrap arounds

        return iRV; // -> return reverb mix buf val
    }
}
