/***************************************************************************
                            xa.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

#include "spu_externals.h"

////////////////////////////////////////////////////////////////////////
// XA GLOBALS
////////////////////////////////////////////////////////////////////////

u32     XAStart[44100];
u32     *XAFeed = XAStart;
u32     *XAPlay = XAStart;
u32     *XAEnd = XAStart + 44100;

u32     XARepeat = 0;
u32     XALastVal = 0;

u32     CDDAStart[44100];
u32     *CDDAFeed = CDDAStart;
u32     *CDDAPlay = CDDAStart;
u32     *CDDAEnd = CDDAStart + 44100;

int         iLeftXAVol = 0x8000;
int         iRightXAVol = 0x8000;

////////////////////////////////////////////////////////////////////////
// MIX XA & CDDA
////////////////////////////////////////////////////////////////////////

static int  lastxa_lc, lastxa_rc;
static int  lastcd_lc, lastcd_rc;

extern u32      decoded_ptr;

void MixXA(int *left, int *right)
{
    int             lc,rc;
    u32     cdda_l;

    lc = 0;
    rc = 0;

    if (XAPlay != XAFeed)
    {
        XALastVal = *XAPlay++;
        if (XAPlay == XAEnd)
            XAPlay = XAStart;

        lc = (short)(XALastVal & 0xffff);
        rc = (short)((XALastVal >> 16) & 0xffff);

        // improve crackle - buffer under
        // - not update fast enough
        lastxa_lc = lc;
        lastxa_rc = rc;

        // Tales of Phantasia - voice meter
        spuMem[(decoded_ptr + 0x000) / 2] = (short)lc;
        spuMem[(decoded_ptr + 0x400) / 2] = (short)rc;

        lc = CLAMP16((lc * iLeftXAVol) / 0x8000);
        rc = CLAMP16((rc * iRightXAVol) / 0x8000);

        // reverb write flag
        if (spuCtrl & CTRL_CD_REVERB)
        {
            StoreREVERB_CD(lc, rc);
        }

        // play flag
        if (spuCtrl & CTRL_CD_PLAY )
        {
            *left += lc;
            *right += rc;
        }
    }

    if (XAPlay == XAFeed && XARepeat)
    {
        //XARepeat--;
        // improve crackle - buffer under
        // - not update fast enough
        lc = lastxa_lc;
        rc = lastxa_rc;

        // Tales of Phantasia - voice meter
        spuMem[(decoded_ptr + 0x000) / 2] = (short)lc;
        spuMem[(decoded_ptr + 0x400) / 2] = (short)rc;

        lc = CLAMP16((lc * iLeftXAVol) / 0x8000);
        rc = CLAMP16((rc * iRightXAVol) / 0x8000);

        // reverb write flags
        if (spuCtrl & CTRL_CD_REVERB)
        {
            StoreREVERB_CD(lc, rc);
        }

        // play flag
        if (spuCtrl & CTRL_CD_PLAY)
        {
            *left += lc;
            *right += rc;
        }
    }

    if (CDDAPlay != CDDAFeed)
    {
        cdda_l = *CDDAPlay++;
        if (CDDAPlay == CDDAEnd)
            CDDAPlay = CDDAStart;

        lc = (short)(cdda_l & 0xffff);
        rc = (short)((cdda_l >> 16) & 0xffff);

        // improve crackle - buffer under
        // - not update fast enough
        lastcd_lc = lc;
        lastcd_rc = rc;

        // Vib Ribbon - playback
        spuMem[(decoded_ptr + 0x000) / 2] = (short)lc;
        spuMem[(decoded_ptr + 0x400) / 2] = (short)rc;

        // Rayman - stage end fadeout
        lc = CLAMP16((lc * iLeftXAVol) / 0x8000);
        rc = CLAMP16((rc * iRightXAVol) / 0x8000);

        // reverb write flag
        if (spuCtrl & CTRL_CD_REVERB)
        {
            StoreREVERB_CD(lc, rc);
        }

        // play flag
        if (spuCtrl & CTRL_CD_PLAY)
        {
            *left += lc;
            *right += rc;
        }
    }

    if (CDDAPlay == CDDAFeed && XARepeat)
    {
        //XARepeat--;
        // improve crackle - buffer under
        // - not update fast enough
        lc = lastcd_lc;
        rc = lastcd_rc;

        // Vib Ribbon - playback
        spuMem[(decoded_ptr + 0x000) / 2] = (short)lc;
        spuMem[(decoded_ptr + 0x400) / 2] = (short)rc;

        // Rayman - stage end fadeout
        lc = CLAMP16((lc * iLeftXAVol) / 0x8000);
        rc = CLAMP16((rc * iRightXAVol) / 0x8000);

        // reverb write flag
        if (spuCtrl & CTRL_CD_REVERB)
        {
            StoreREVERB_CD(lc, rc);
        }

        // play flag
        if (spuCtrl & CTRL_CD_PLAY)
        {
            *left += lc;
            *right += rc;
        }
    }
}

void FeedXA(xa_decode_t *xap)
{
    int sinc, spos, i, iSize, iPlace;

    XARepeat = 100; // set up repeat

    iSize = ((44100 * xap->nsamples) / xap->freq); // get size

    if (!iSize)
        return; // none? bye

    if (XAFeed < XAPlay)
        iPlace = XAPlay - XAFeed; // how much space in my buf?
    else
        iPlace =(XAEnd - XAFeed) + (XAPlay - XAStart);

    if (iPlace == 0)
        return; // no place at all

    spos = 0x10000L;
    sinc = (xap->nsamples << 16) / iSize; // calc freq by num / size

    if (xap->stereo)
    {
        u32     *pS = (u32 *)xap->pcm;
        u32     l = 0;

        for (i = 0; i < iSize; i++)
        {
            while (spos >= 0x10000L)
            {
                l = *pS++;
                spos -= 0x10000L;
            }

            *XAFeed++ = l;

            if (XAFeed == XAEnd)
                XAFeed = XAStart;

            if (XAFeed == XAPlay)
            {
                if (XAPlay != XAStart)
                    XAFeed = XAPlay - 1;

                break;
            }

            spos += sinc;
        }
    }
    else
    {
        u16     *pS = (u16 *)xap->pcm;
        u32     l;
        short           s = 0;

        for (i = 0; i < iSize; i++)
        {
            while (spos >= 0x10000L)
            {
                s = *pS++;
                spos -= 0x10000L;
            }
            l=s;

            *XAFeed++ = ((l & 0xffff) | (l << 16));

            if (XAFeed == XAEnd)
                XAFeed = XAStart;

            if (XAFeed == XAPlay)
            {
                if (XAPlay != XAStart)
                    XAFeed = XAPlay - 1;

                break;
            }

            spos += sinc;
        }
    }
}

void FeedCDDA(u8 *pcm, int nBytes)
{
    while (nBytes > 0)
    {
        if (CDDAFeed == CDDAEnd)
            CDDAFeed = CDDAStart;

        *CDDAFeed++ = *(u32 *)pcm;
        nBytes -= 4;
        pcm += 4;
    }
}
