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

#ifndef __EXTERNALS__
#define __EXTERNALS__

#include "spu_xa.h"
#include "spu_registers.h"
#include "spu_reverb.h"
#include "spu_adsr.h"

// num of channels
#define MAXCHAN     24

typedef struct
{
    int     State;
    int     AttackModeExp;
    int     AttackRate;
    int     DecayRate;
    int     SustainLevel;
    int     SustainModeExp;
    int     SustainIncrease;
    int     SustainRate;
    int     ReleaseModeExp;
    int     ReleaseRate;
    int     EnvelopeVol;
    int     EnvelopeVol_f;  // fraction
    int     lVolume;
    int     lDummy1;
    int     lDummy2;
}
ADSR;

typedef struct
{
    int     bNew;           // start flag

    int     iSBPos;         // mixing stuff
    int     spos;
    int     sinc;
    int     SB[32 + 32];    // Pete added another 32 dwords in 1.6 ... prevents overflow issues with gaussian/cubic interpolation (thanx xodnizel!), and can be used for even better interpolations, eh? :)
    int     sval;

    u8      *pStart, *pCurr, *pLoop;    // start/current/loop ptr in sound mem

    int     bOn;            // is channel active (sample playing?)
    int     bStop;          // is channel stopped (sample _can_ still be playing, ADSR Release phase)
    int     bReverb;        // can we do reverb on this channel? must have ctrl register bit, to get active
    int     iActFreq;       // current psx pitch
    int     iUsedFreq;      // current pc pitch
    int     iLeftVolume;    // left volume
    int     iLeftVolRaw;    // left psx volume value
    int     bLoopJump;      // ignore loop bit, if an external loop address is used
    int     iMute;          // mute mode (debug)
    int     iSilent;        // voice on - sound on/off
    int     iRightVolume;   // right volume
    int     iRightVolRaw;   // right psx volume value
    int     iRawPitch;      // raw pitch (0...3fff)
    int     iIrqDone;       // debug irq done flag
    int     s_1, s_2;       // last decoding infos
    int     bRVBActive;     // reverb active flag
    int     iRVBOffset;     // reverb offset
    int     iRVBRepeat;     // reverb repeat
    int     bNoise;         // noise active flag
    int     bFMod;          // freq mod (0=off, 1=sound channel, 2=freq channel)
    int     iRVBNum;        // another reverb helper
    int     iOldNoise;      // old noise val for this channel
    ADSR    adsr;           // active ADSR settings
}
CHANNEL;

// psx buffers / addresses

extern u16      spuMem[];
extern u8       *spuMemC;
extern u8       *pSpuIrq;

// user settings

extern int        iUseInterpolation;
// MISC

extern CHANNEL  s_chan[];

extern u32      dwNoiseVal, dwNoiseClock, dwNoiseCount;
extern u16      spuCtrl, spuStat, spuIrq;
extern u32      spuAddr;
extern u32      bIrqHit;

extern void (*irqCallback)(void);                  // func of main emu, called on spu irq

///////////////////////////////////////////////////////////
// XA.C globals
///////////////////////////////////////////////////////////

extern u32      *XAFeed, *XAPlay, XAStart[], *XAEnd;

extern u32      XARepeat;
extern u32      XALastVal;

extern u32      *CDDAFeed, *CDDAPlay, CDDAStart[], *CDDAEnd;

extern int      iLeftXAVol, iRightXAVol;

///////////////////////////////////////////////////////////
// REVERB.C globals
///////////////////////////////////////////////////////////

extern int      *sRVBPlay, *sRVBEnd, sRVBStart[];
extern int      iReverbOff;
extern int      iReverbRepeat;
extern int      iReverbNum;

// 15-bit value + 1-sign
int CLAMP16(int);

#endif
