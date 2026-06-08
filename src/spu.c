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
* Sound (SPU) functions.
*/

#include "spu_externals.h"

#include "spu.h"

short       pSndBuffer[BUFFERSIZE + 1];
int         iWritePos = BUFFERSIZE + 1 - SAMPLERATE; // you tell me! TODO

void SPU_Async(u32 new)
{
    static int  cycles = 0;

    cycles += new;

    while (cycles >= PSXCLK / SAMPLERATE)
    {
        SPU_Cycle();

        iWritePos &= BUFFERSIZE;

        pSndBuffer[iWritePos++] = (short)SSumL;
        pSndBuffer[iWritePos++] = (short)SSumR;

        cycles -= PSXCLK / SAMPLERATE;
    }
}

void SPU_Irq()
{
    psxHu32ref(0x1070) |= SWAPu32(0x200);
}

// -----------------------------------------------------------------------------

// globals

// psx buffer / addresses

u16             spuMem[256 * 1024];
u8              *spuMemC = (u8 *)spuMem;
u8              *pSpuIrq = 0;

// user settings

// 0 = off, 1 = simple, 2 = full
int             iUseInterpolation = 0;

// MAIN infos struct for each channel

CHANNEL         s_chan[MAXCHAN + 1]; // channel + 1 infos (1 is security for fmod handling)

// global noise generator
u32             dwNoiseVal = 1;
u32             dwNoiseCount;
u32             dwNoiseClock;

u32             decoded_ptr = 0;
u32             bIrqHit = 0;

// some vars to store psx reg infos
u16             spuCtrl = 0;
u16             spuStat = 0;
u16             spuIrq = 0;

u32             spuAddr = 0x200; // address into spu mem

void (*irqCallback)(void) = 0; // func of main emu, called on spu irq

// certain globals (were local before, but with the new timeproc I need em global)
static const int    f[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
int                 SSumR;
int                 SSumL;
int                 iFMod;

////////////////////////////////////////////////////////////////////////
// CODE AREA
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// helpers for gauss interpolation

#define gval(x) (((short *)(&s_chan[ch].SB[29]))[(gpos + x) & 3])

void StartSound(int ch)
{
    StartADSR(ch);
    StartREVERB(ch);

    // fussy timing issues - do in VoiceOn
    //s_chan[ch].pCurr=s_chan[ch].pStart;                   // set sample start
    //s_chan[ch].bStop=0;
    //s_chan[ch].bOn=1;

    s_chan[ch].s_1 = 0; // init mixing vars
    s_chan[ch].s_2 = 0;
    s_chan[ch].iSBPos = 28;

    s_chan[ch].bNew = 0; // init channel flags

    s_chan[ch].SB[29] = 0; // init our interpolation helpers
    s_chan[ch].SB[30] = 0;

    if (iUseInterpolation) // gauss interpolation?
    {
        s_chan[ch].spos = 0x30000L;
        s_chan[ch].SB[28] = 0;
    } // -> start with more decoding
    else
    {
        s_chan[ch].spos = 0x10000L;
        s_chan[ch].SB[31] = 0;
    } // -> no/simple interpolation starts with one 44100 decoding
}

void VoiceChangeFrequency(int ch)
{
    s_chan[ch].iUsedFreq = s_chan[ch].iActFreq; // -> take it and calc steps
    s_chan[ch].sinc = s_chan[ch].iRawPitch << 4;
    if (!s_chan[ch].sinc)
        s_chan[ch].sinc = 1;
}

void FModChangeFrequency(int ch)
{
    int NP = s_chan[ch].iRawPitch;

    NP = ((32768L + iFMod) * NP) / 32768L;

    if (NP > 0x3fff)
        NP = 0x3fff;

    if (NP < 0x1)
        NP = 0x1;

    NP = (44100L * NP) / (4096L); // calc frequency

    s_chan[ch].iActFreq = NP;
    s_chan[ch].iUsedFreq = NP;
    s_chan[ch].sinc = (((NP / 10) << 16) / 4410);
    if (!s_chan[ch].sinc)
        s_chan[ch].sinc = 1;

    iFMod = 0;
}

////////////////////////////////////////////////////////////////////////

/*
Noise Algorithm
- Dr.Hell (Xebra PS1 emu)
- 100% accurate (waveform + frequency)
- http://drhell.web.fc2.com

Level change cycle
Freq = 0x8000 >> (NoiseClock >> 2);

Frequency of half cycle
Half = ((NoiseClock & 3) * 2) / (4 + (NoiseClock & 3));
- 0 = (0*2)/(4+0) = 0/4
- 1 = (1*2)/(4+1) = 2/5
- 2 = (2*2)/(4+2) = 4/6
- 3 = (3*2)/(4+3) = 6/7

-------------------------------

5*6*7 = 210
4 -  0*0 = 0
5 - 42*2 = 84
6 - 35*4 = 140
7 - 30*6 = 180
*/

// Noise Waveform - Dr. Hell (Xebra)
char    NoiseWaveAdd[64] =
{
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1
};

u16     NoiseFreqAdd[5] = {0, 84, 140, 180, 210};

void NoiseClock()
{
    u32     level;

    level = 0x8000 >> (dwNoiseClock >> 2);
    level <<= 16;

    dwNoiseCount += 0x10000;

    // Dr. Hell - fraction
    dwNoiseCount += NoiseFreqAdd[dwNoiseClock & 3];
    if ((dwNoiseCount & 0xffff) >= NoiseFreqAdd[4])
    {
        dwNoiseCount += 0x10000;
        dwNoiseCount -= NoiseFreqAdd[dwNoiseClock & 3];
    }

    if (dwNoiseCount >= level)
    {
        while (dwNoiseCount >= level)
            dwNoiseCount -= level;

        // Dr. Hell - form
        dwNoiseVal = (dwNoiseVal << 1) | NoiseWaveAdd[(dwNoiseVal >> 10) & 63];
    }
}

int iGetNoiseVal(int ch)
{
    (void)ch;

    int fa;

    fa = (short)dwNoiseVal;

 // no clip need
 //if(fa>32767L)  fa=32767L;
 //if(fa<-32767L) fa=-32767L;

 // don't upset VAG decoder
 //if(iUseInterpolation<2)                               // no gauss/cubic interpolation?
  //pChannel->SB[29] = fa;                               // -> store noise val in "current sample" slot

 // boost volume - no more!
 //return fa * 3 / 2;
    return fa;
}

void StoreInterpolationVal(int ch, int fa)
{
    /*
    // fmod channel = sound output
 if(s_chan[ch].bFMod==2)                               // fmod freq channel
  s_chan[ch].SB[29]=fa;
 else
 */
    {
        if ((spuCtrl & 0x4000) == 0)
            fa = 0; // muted?
        else // else adjust
        {
            fa = CLAMP16(fa);
        }

        if (iUseInterpolation) // gauss/cubic interpolation
        {
            int gpos = s_chan[ch].SB[28];
            gval(0) = fa;
            gpos = (gpos + 1) & 3;
            s_chan[ch].SB[28] = gpos;
        }
        else
            s_chan[ch].SB[29] = fa; // no interpolation
  }
}

int iGetInterpolationVal(int ch)
{
    int fa;
    int gpos;

 // fmod channel = sound output
 //if(s_chan[ch].bFMod==2) return s_chan[ch].SB[29];

    if (iUseInterpolation) // cubic interpolation
    {
        long    xd;

        xd = ((s_chan[ch].spos) >> 1) + 1;
        gpos = s_chan[ch].SB[28];

        fa  = gval(3) - 3 * gval(2) + 3 * gval(1) - gval(0);
        fa *= (xd - (2 << 15)) / 6;
        fa >>= 15;
        fa += gval(2) - gval(1) - gval(1) + gval(0);
        fa *= (xd - (1 << 15)) >> 1;
        fa >>= 15;
        fa += gval(1) - gval(0);
        fa *= xd;
        fa >>= 15;
        fa = fa + gval(0);
    }
    else // no interpolation
    {
        fa = s_chan[ch].SB[29];
    }

    return fa;
}

////////////////////////////////////////////////////////////////////////
// MAIN SPU FUNCTION
// here is the main job handler... thread, timer or direct func call
// basically the whole sound processing is done in this fat func!
void SPU_Cycle()
{
    int s_1, s_2, fa;

    u8      *start;
    u32     nSample;
    int             ch, predict_nr, shift_factor, flags, d, s;
    int             bIRQReturn = 0;
    u32     decoded_voice = 0;

    // main channel loop
    decoded_voice = decoded_ptr;

    SSumL = 0;
    SSumR = 0;

    // decoded buffer values - dummy
    spuMem[(0x000 + decoded_voice) / 2] = (short)0;
    spuMem[(0x400 + decoded_voice) / 2] = (short)0;
    spuMem[(0x800 + decoded_voice) / 2] = (short)0;
    spuMem[(0xc00 + decoded_voice) / 2] = (short)0;

    NoiseClock();

    for (ch = 0; ch < MAXCHAN; ch++) // loop em all... we will collect 1 ms of sound of each playing channel
    {
        if (s_chan[ch].bNew)
        {
#if 1
            StartSound(ch); // start new sound
#else
            if (s_chan[ch].adsr.StartDelay == 0)
            {
                StartSound(ch); // start new sound
            }
            else
            {
                s_chan[ch].adsr.StartDelay--;
            }
#endif
        }
        if (!s_chan[ch].bOn)
            continue; // channel not playing? next

        if (s_chan[ch].iActFreq != s_chan[ch].iUsedFreq) // new psx frequency?
            VoiceChangeFrequency(ch);

        if (s_chan[ch].bFMod == 1 && iFMod) // fmod freq channel
            FModChangeFrequency(ch);

        while (s_chan[ch].spos >= 0x10000L)
        {
            if (s_chan[ch].iSBPos == 28) // 28 reached?
            {
                // Xenogears - Anima Relic dungeon (exp gain)
                if (s_chan[ch].bLoopJump == 1)
                    s_chan[ch].pCurr = s_chan[ch].pLoop;

                s_chan[ch].bLoopJump = 0;
                start = s_chan[ch].pCurr; // set up the current pos

                if (start == spuMemC)
                    s_chan[ch].bOn = 0;

                if (s_chan[ch].iSilent == 1)
                {
                    // silence = let channel keep running (IRQs)
                    //s_chan[ch].bOn=0;                       // -> turn everything off
                    s_chan[ch].iSilent = 2;

                    s_chan[ch].adsr.lVolume = 0;
                    s_chan[ch].adsr.EnvelopeVol = 0;
                }

                s_chan[ch].iSBPos = 0;

                // spu irq handler here? mmm... do it later

                s_1 = s_chan[ch].s_1;
                s_2 = s_chan[ch].s_2;

                predict_nr = (int)*start;
                start++;
                shift_factor = predict_nr & 0xf;
                predict_nr >>= 4;
                flags=(int)*start;
                start++;

                // Silhouette Mirage - Serah fight
                if (predict_nr > 4)
                    predict_nr = 0;

                // -------------------------------------- //

                for (nSample = 0; nSample < 28; start++)
                {
                    d = (int)*start;
                    s = ((d & 0xf) << 12);
                    if (s & 0x8000)
                        s |= 0xffff0000;

                    fa = (s >> shift_factor);
                    fa = fa + ((s_1 * f[predict_nr][0]) >> 6) + ((s_2 * f[predict_nr][1]) >> 6);

                    // snes brr clamps
                    fa = CLAMP16(fa);

                    s_2 = s_1;
                    s_1 = fa;
                    s = ((d & 0xf0) << 8);

                    s_chan[ch].SB[nSample++] = fa;

                    if (s & 0x8000)
                        s |= 0xffff0000;

                    fa = (s >> shift_factor);
                    fa = fa + ((s_1 * f[predict_nr][0]) >> 6) + ((s_2 * f[predict_nr][1]) >> 6);

                    // snes brr clamps
                    fa = CLAMP16(fa);

                    s_2 = s_1;
                    s_1 = fa;

                    s_chan[ch].SB[nSample++] = fa;
                }

                // irq check

#if 1
                // Check channel/loop IRQs (e.g. Castlevania Chronicles) and at pos-8 for unknown reason
                if (Check_IRQ((s_chan[ch].pCurr) - spuMemC, 0) ||
                    Check_IRQ((start - spuMemC)-0, 0) ||
                    Check_IRQ((start - spuMemC)-8, 0))
                {
#else
                if (irqCallback && (spuCtrl & 0x40)) // some callback and irq active?
                {
                    if ((pSpuIrq > start - 16 &&              // irq address reached?
                        pSpuIrq <= start) ||
                        ((flags & 1) &&                        // special: irq on looping addr, when stop/loop flag is set
                        (pSpuIrq > s_chan[ch].pLoop - 16 &&
                        pSpuIrq <= s_chan[ch].pLoop)))
#endif
                    {
                        s_chan[ch].iIrqDone = 1; // -> debug flag
                        //irqCallback(); // -> call main emu (checked & called on Check_IRQ)
                    }
                }

                // flag handler

            /*
            SPU2-X:
            $4 = set loop to current block
            $2 = keep envelope on (no mute)
            $1 = jump to loop address

            silence means no volume (ADSR keeps playing!!)
            */

                if (flags & 4)
                    s_chan[ch].pLoop = start - 16;

            // Jungle Book - Rhythm 'n Groove - don't reset ignore status
            // - fixes gameplay speed (IRQ hits)
            //s_chan[ch].bIgnoreLoop = 0;

                if (flags & 1)
                {
                // ...?
                //s_chan[ch].bIgnoreLoop = 0;

                    // Xenogears - 7 = play missing sounds
                    // set jump flag
                    s_chan[ch].bLoopJump = 1;

                    // silence = keep playing..?
                    if ((flags & 2) == 0)
                    {
                        s_chan[ch].iSilent = 1;

                    // silence = don't start release phase
                    //s_chan[ch].bStop = 1;

                    //start = (unsigned char *) -1;
                    }
                }

#if 0
            // crash check
            if( start == 0 )
                start = (unsigned char *) -1;
            if( start >= spuMemC + 0x80000 )
                start = spuMemC - 0x80000;
#endif

                // Silhouette Mirage - ending mini-game

                // ??
                if (start - spuMemC >= 0x80000)
                {
                    start -= 16;

                    s_chan[ch].iSilent = 1;
                    s_chan[ch].bStop = 1;
                }

                s_chan[ch].pCurr = start; // store values for next cycle
                s_chan[ch].s_1 = s_1;
                s_chan[ch].s_2 = s_2;
            }

            fa = s_chan[ch].SB[s_chan[ch].iSBPos++]; // get sample data

            StoreInterpolationVal(ch, fa); // store val for later interpolation

            s_chan[ch].spos -= 0x10000L;
        }

        if (s_chan[ch].bNoise)
            fa = iGetNoiseVal(ch); // get noise val
        else
            fa = iGetInterpolationVal(ch); // get sample val

        // Voice 1/3 decoded buffer
        if (ch == 0)
        {
            spuMem[(0x800 + decoded_voice) / 2] = (short)fa;
        }
        else if (ch == 2)
        {
            spuMem[(0xc00 + decoded_voice) / 2] = (short)fa;
        }

        s_chan[ch].sval = (MixADSR(ch) * fa) / 1023;  // mix adsr

        if (s_chan[ch].bFMod == 2) // fmod freq channel
            iFMod = s_chan[ch].sval; // -> store 1T sample data, use that to do fmod on next channel

             // mix fmod channel into output
             // - Xenogears save icon (high pitch)
        {
            // ok, left/right sound volume (psx volume goes from 0 ... 0x3fff)

            if (s_chan[ch].iMute)
                s_chan[ch].sval = 0; // debug mute
            else
            {
                SSumL += (s_chan[ch].sval *s_chan[ch].iLeftVolume) / 0x4000L;
                SSumR += (s_chan[ch].sval * s_chan[ch].iRightVolume) / 0x4000L;
            }

            // now let us store sound data for reverb
            if (s_chan[ch].bRVBActive)
                StoreREVERB(ch);
        }

        s_chan[ch].spos += s_chan[ch].sinc;
    }

    // ok, go on until 1 ms data of this channel is collected

    // decoded buffer - voice
    decoded_voice += 2;
    decoded_voice &= 0x3ff;

    // status flag
    if (decoded_voice >= 0x200)
    {
        spuStat |= STAT_DECODED;
    }
    else
    {
        spuStat &= ~STAT_DECODED;
    }

    // IRQ work
    u8      *old_irq;
    u32     old_ptr;

    old_irq = pSpuIrq;
    old_ptr = decoded_voice;
#if 0
    // align to boundaries ($0, $200, $400, $600)
    pSpuIrq = ((pSpuIrq - spuMemC) & (~0x1ff)) + spuMemC;
    decoded_voice = decoded_voice & (~0x1ff);
#endif
    // check all decoded buffer IRQs - timing issue
    Check_IRQ(decoded_voice + 0x000, 0);
    Check_IRQ(decoded_voice + 0x400, 0);
    Check_IRQ(decoded_voice + 0x800, 0);
    Check_IRQ(decoded_voice + 0xc00, 0);

    pSpuIrq = old_irq;
    decoded_voice = old_ptr;

    if (bIRQReturn) // special return for "spu irq - wait for cpu action"
    {
        bIRQReturn = 0;
        return;
    }

    // here we have another 1 ms of sound data
    // mix XA infos (if any)
    MixXA(&SSumL, &SSumR);

    // now safe to update decoded buffer ptr
    decoded_ptr += 2;
    decoded_ptr &= 0x3ff;

    // mix all channels (including reverb) into one buffer
    SSumL += MixREVERBLeft();
    SSumL = CLAMP16(SSumL);

    SSumR += MixREVERBRight();
    SSumR = CLAMP16(SSumR);

    InitREVERB();
}

// XA AUDIO

void SPU_PlayADPCM_Channel(xa_decode_t *xap)
{
    if (!xap)
        return;

    if (!xap->freq)
        return; // no xa freq ? bye

    FeedXA(xap); // call main XA feeder
}

// CDDA AUDIO
void SPU_PlayCDDA_Channel(short *pcm, int nbytes)
{
    if (!pcm)
        return;

    if (nbytes <= 0)
        return;

    FeedCDDA((u8 *)pcm, nbytes);
}

// INIT/EXIT STUFF

// SPUOPEN: called by main emu after init
void SPU_Open()
{
    int i;

    memset((void *)s_chan, 0, (MAXCHAN + 1) * sizeof(CHANNEL));

    for (i = 0; i < MAXCHAN; i++) // loop sound channels
    {
        s_chan[i].adsr.SustainLevel = 1024; // -> init sustain
        s_chan[i].iMute = 0;
        s_chan[i].iIrqDone = 0;
        s_chan[i].pLoop = spuMemC;
        s_chan[i].pStart = spuMemC;
        s_chan[i].pCurr = spuMemC;
    }

    InitADSR();

    memset((void *)&rvb, 0, sizeof(REVERB));

    if (iUseReverb)
        sRVBEnd += 88200 * 2;
    else
        sRVBEnd += 2;
}

// SPUCLOSE: called before shutdown
void SPU_Close()
{
}

// SETUP CALLBACKS
// this functions will be called once,
// passes a callback that should be called on SPU-IRQ/cdda volume change
void SPU_RegisterCallback(void (*callback)(void))
{
    irqCallback = callback;
}

void SPU_ReadDMAMem(u16 *pusPSXMem, int iSize)
{
    int             i;
    u8      crc = 0;

    spuStat |= STAT_DATA_BUSY;

    for (i = 0; i < iSize; i++)
    {
        Check_IRQ(spuAddr, 0);

        crc |= *pusPSXMem++ = spuMem[spuAddr >> 1]; // spu addr got by writeregister
        spuAddr += 2; // inc spu addr
        //spuMem[spuAddr >> 1];

        // guess based on Vib Ribbon (below)
        if (spuAddr > 0x7ffff)
            break;
    }

/*
    Toshiden Subaru "story screen" hack.

    After character selection screen, the game checks values inside returned
    SPU buffer and all values cannot be 0x0.
    Due to XA timings(?) we return buffer that has only NULLs.
    Setting little lag to MixXA() causes buffer to have some non-NULL values,
    but causes garbage sound so this hack is preferable.

    Note: When messing with xa.c like fixing Suikoden II's demo video sound issue
    this should be handled as well.
*/
    if (crc == 0)
        *--pusPSXMem = 0xFF;

    spuStat &= ~STAT_DATA_BUSY;
    spuStat &= ~STAT_DMA_NON;
    spuStat &= ~STAT_DMA_W;
    spuStat |= STAT_DMA_R;
}

void SPU_WriteDMAMem(u16 *pusPSXMem, int iSize)
{
    int i;

    spuStat |= STAT_DATA_BUSY;

    for (i = 0; i < iSize; i++)
    {
        Check_IRQ(spuAddr, 0);

        spuMem[spuAddr >> 1] = *pusPSXMem++; // spu addr got by writeregister
        spuAddr += 2; // inc spu addr

        // Vib Ribbon - stop transfer (reverb playback)
        if (spuAddr > 0x7ffff)
            break;
    }

    spuStat &= ~STAT_DATA_BUSY;
    spuStat &= ~STAT_DMA_NON;
    spuStat &= ~STAT_DMA_R;
    spuStat |= STAT_DMA_W;
}
