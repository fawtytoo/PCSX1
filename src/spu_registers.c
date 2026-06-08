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

/*
// adsr time values (in ms) by James Higgs ... see the end of
// the adsr.c source for details

#define ATTACK_MS     514L
#define DECAYHALF_MS  292L
#define DECAY_MS      584L
#define SUSTAIN_MS    450L
#define RELEASE_MS    446L
*/

// we have a timebase of 1.020408f ms, not 1 ms... so adjust adsr defines
#define ATTACK_MS      494L
#define DECAYHALF_MS   286L
#define DECAY_MS       572L
#define SUSTAIN_MS     441L
#define RELEASE_MS     437L

u16                     regArea[10000];

int Check_IRQ(int addr, int force)
{
    if (spuCtrl & CTRL_IRQ) // some callback and irq active?
    {
        if ((bIrqHit == 0) && (force == 1 || pSpuIrq == spuMemC + addr))
        {
            if (irqCallback)
                irqCallback(); // -> call main emu

            // one-time
            bIrqHit = 1;
            spuStat |= STAT_IRQ;

            return 1;
        }
    }

    return 0;
}

void SoundOn(int start, int end, u16 val)
{
    int ch;

    for (ch = start; ch < end; ch++, val >>= 1) // loop channels
    {
        if ((val & 1) && s_chan[ch].pStart) // mmm... start has to be set before key on !?!
        {
            s_chan[ch].bLoopJump = 0;
            s_chan[ch].bNew = 1;

            // do this here, not in StartSound
            // - fixes fussy timing issues
            s_chan[ch].iSilent = 0;
            s_chan[ch].bStop = 0;
            s_chan[ch].bOn = 1;
            s_chan[ch].pCurr = s_chan[ch].pStart;

#if 0
         // ADSR init time (guess to # apu cycles)
         s_chan[ch].adsr.StartDelay = 0;
#endif

            // Final Fantasy 7 - don't do any of these
            // - sets loop address before VoiceOn
            //s_chan[ch].pLoop = s_chan[ch].pStart;
        }
    }
}

void SoundOff(int start, int end, u16 val)
{
    int ch;

    for (ch = start; ch < end; ch++, val >>= 1) // loop channels
    {
        if (val & 1) // && s_chan[i].bOn)  mmm...
        {
            s_chan[ch].bStop = 1;

            // Jungle Book - Rhythm 'n Groove
            // - turns off buzzing sound (loop hangs)
            s_chan[ch].bNew = 0;
        }
    }
}

void FModOn(int start, int end, u16 val)
{
    int ch;

    for (ch = start; ch < end; ch++, val >>= 1) // loop channels
    {
        s_chan[ch].bFMod = val & 1; // -> fmod on/off
        if (ch > 0)
        {
            s_chan[ch - 1].bFMod = 2; // --> freq channel
        }
    }
}

void NoiseOn(int start, int end, u16 val)
{
    int ch;

    for (ch = start; ch < end; ch++, val >>= 1) // loop channels
    {
        s_chan[ch].bNoise = val & 1; // -> noise on/off
    }
}

// please note: sweep and phase invert are wrong... but I've never seen
// them used

void SetVolumeL(int ch, short vol)
{
    s_chan[ch].iLeftVolRaw = vol;

    if (vol & 0x8000) // sweep?
    {
        short   sInc = 1; // -> sweep up?
        if (vol & 0x2000)
            sInc = -1; // -> or down?

        if (vol & 0x1000)
            vol ^= 0xffff; // -> mmm... phase inverted? have to investigate this

        vol = ((vol & 0x7f) + 1) / 2; // -> sweep: 0..127 -> 0..64
        vol += vol / (2 * sInc); // -> HACK: we don't sweep right now, so we just raise/lower the volume by the half!
        vol *= 128;
    }
    else // no sweep:
    {
        if (vol & 0x4000) // -> mmm... phase inverted? have to investigate this
            //vol^=0xffff;
            vol = 0x3fff - (vol & 0x3fff);
    }

    vol &= 0x3fff;
    s_chan[ch].iLeftVolume = vol; // store volume
}

void SetVolumeR(int ch, short vol)
{
    s_chan[ch].iRightVolRaw = vol;

    if (vol & 0x8000) // comments... see above :)
    {
        short   sInc = 1;

        if (vol & 0x2000)
            sInc = -1;

        if (vol & 0x1000)
            vol ^= 0xffff;

        vol = ((vol & 0x7f) + 1) / 2;
        vol += vol / (2 * sInc);
        vol *= 128;
    }
    else
    {
        if (vol & 0x4000)
            //vol = vol ^= 0xffff;
            vol = 0x3fff - (vol & 0x3fff);
    }

    vol &= 0x3fff;

    s_chan[ch].iRightVolume = vol;
}

void SetPitch(int ch, u16 val)
{
    int NP;

    if (val > 0x3fff)
        NP = 0x3fff; // get pitch val
    else
        NP = val;

    s_chan[ch].iRawPitch = NP;

    NP = (44100 * NP) / 4096L; // calc frequency
    if (NP < 1)
        NP = 1; // some security

    s_chan[ch].iActFreq = NP; // store frequency
}

void ReverbOn(int start, int end, u16 val)    // REVERB ON PSX COMMAND
{
    int ch;

    for (ch = start; ch < end; ch++, val >>= 1) // loop channels
    {
        s_chan[ch].bReverb = val & 1; // -> reverb on/off
    }
}

void SPU_WriteRegister(u32 reg, u16 val)
{
    const u32       r = reg & 0xfff;
    u32             lval;

    regArea[(r - 0xc00) >> 1] = val;

    if (r >= 0x0c00 && r < 0x0d80) // some channel info?
    {
        int ch = (r >> 4) - 0xc0; // calc channel
        switch (r & 0x0f)
        {
          case 0:
            SetVolumeL(ch, val);
            break;

          case 2:
            SetVolumeR(ch, val);
            break;

          case 4:
            SetPitch(ch, val);
            break;

          case 6: // start
            // Brain Dead 13 - align to 16 boundary
            s_chan[ch].pStart = spuMemC + (u32)((val << 3) & ~0xf);
            break;

          case 8: // level with pre-calcs
            lval = val;

            s_chan[ch].adsr.AttackModeExp = (lval & 0x8000) ? 1 : 0;
            s_chan[ch].adsr.AttackRate = (lval >> 8) & 0x007f;
            s_chan[ch].adsr.DecayRate = (lval >> 4) & 0x000f;
            s_chan[ch].adsr.SustainLevel = lval & 0x000f;
            break;

          case 10: // adsr times with pre-calcs
            lval = val;

            s_chan[ch].adsr.SustainModeExp = (lval & 0x8000) ? 1 : 0;
            s_chan[ch].adsr.SustainIncrease = (lval & 0x4000) ? 0 : 1;
            s_chan[ch].adsr.SustainRate = (lval >> 6) & 0x007f;
            s_chan[ch].adsr.ReleaseModeExp = (lval & 0x0020) ? 1 : 0;
            s_chan[ch].adsr.ReleaseRate = lval & 0x001f;
            break;

          case 12: // adsr volume... mmm have to investigate this
            break;

          case 14: // loop?
            s_chan[ch].pLoop = spuMemC + ((u32)((val << 3) & ~0xf));

                 //s_chan[ch].bIgnoreLoop=1;
            break;
        }
        return;
    }

    switch (r)
    {
      case H_SPUaddr:
        spuAddr = (u32)val << 3;
        break;

      case H_SPUdata:
        // BIOS - allow dma 00
        Check_IRQ(spuAddr, 0);

        spuMem[spuAddr >> 1] = val;
        spuAddr += 2;
        if (spuAddr > 0x7ffff)
            spuAddr = 0;

        break;

      case H_SPUctrl:
        spuCtrl = val;

        // flags
        if (spuCtrl & CTRL_CD_PLAY)
            spuStat |= CTRL_CD_PLAY;
        else
            spuStat &= ~CTRL_CD_PLAY;

        if (spuCtrl & CTRL_CD_REVERB)
            spuStat |= STAT_CD_REVERB;
        else
            spuStat &= ~STAT_CD_REVERB;

        if (spuCtrl & CTRL_EXT_PLAY)
            spuStat |= STAT_EXT_PLAY;
        else
            spuStat &= ~STAT_EXT_PLAY;

        if (spuCtrl & CTRL_EXT_REVERB)
            spuStat |= STAT_EXT_REVERB;
        else
            spuStat &= ~STAT_EXT_REVERB;

        spuStat &= ~(STAT_DMA_NON | STAT_DMA_R | STAT_DMA_W);

        if (spuCtrl & CTRL_DMA_F)
            spuStat |= STAT_DMA_F;

        if ((spuCtrl & CTRL_DMA_F) == CTRL_DMA_R)
            spuStat |= STAT_DMA_R;

        // reset IRQ flag
        if ((spuCtrl & CTRL_IRQ) == 0)
        {
            bIrqHit = 0;
            spuStat &= ~STAT_IRQ;
        }

        dwNoiseClock = (spuCtrl & CTRL_NOISE) >> 8;
        break;

      case H_SPUstat:
        spuStat = val & 0xf800;
        break;

      case H_SPUReverbAddr:
        if (val == 0xFFFF || val <= 0x200)
        {
            rvb.StartAddr = rvb.CurrAddr = 0;
        }
        else
        {
            const long  iv = (u32)val << 2;
            if (rvb.StartAddr != iv)
            {
                rvb.StartAddr = (u32)val << 2;
                rvb.CurrAddr = rvb.StartAddr;
            }
        }
        break;

      case H_SPUirqAddr:
        spuIrq = val;
        pSpuIrq = spuMemC + ((u32)val << 3);
        break;

      case H_SPUrvolL:
        rvb.VolLeft = val;
        break;

      case H_SPUrvolR:
        rvb.VolRight = val;
        break;

/*
    case H_ExtLeft:
     //auxprintf("EL %d\n",val);
      break;
    //-------------------------------------------------//
    case H_ExtRight:
     //auxprintf("ER %d\n",val);
      break;
    //-------------------------------------------------//
    case H_SPUmvolL:
     //auxprintf("ML %d\n",val);
      break;
    //-------------------------------------------------//
    case H_SPUmvolR:
     //auxprintf("MR %d\n",val);
      break;
    //-------------------------------------------------//
    case H_SPUMute1:
     //auxprintf("M0 %04x\n",val);
      break;
    //-------------------------------------------------//
    case H_SPUMute2:
     //auxprintf("M1 %04x\n",val);
      break;
*/

      case H_SPUon1:
        SoundOn(0, 16, val);
        break;

      case H_SPUon2:
        SoundOn(16, 24, val);
        break;

      case H_SPUoff1:
        SoundOff(0, 16, val);
        break;

      case H_SPUoff2:
        SoundOff(16, 24, val);
        break;

      case H_CDLeft:
        iLeftXAVol = val;
        break;

      case H_CDRight:
        iRightXAVol = val;
        break;

      case H_FMod1:
        FModOn(0, 16, val);
        break;

      case H_FMod2:
        FModOn(16, 24, val);
        break;

      case H_Noise1:
        NoiseOn(0, 16, val);
        break;

      case H_Noise2:
        NoiseOn(16, 24, val);
        break;

      case H_RVBon1:
        ReverbOn(0, 16, val);
        break;

      case H_RVBon2:
        ReverbOn(16, 24, val);
        break;

      case H_Reverb + 0:
        rvb.FB_SRC_A = val;

        // OK, here's the fake REVERB stuff...
        // depending on effect we do more or less delay and repeats... bah
        // still... better than nothing :)

        SetREVERB(val);
        break;

      case H_Reverb + 2:
        rvb.FB_SRC_B = (short)val;
        break;

      case H_Reverb + 4:
        rvb.IIR_ALPHA = (short)val;
        break;

      case H_Reverb + 6:
        rvb.ACC_COEF_A = (short)val;
        break;

      case H_Reverb + 8:
        rvb.ACC_COEF_B = (short)val;
        break;

      case H_Reverb + 10:
        rvb.ACC_COEF_C = (short)val;
        break;

      case H_Reverb + 12:
        rvb.ACC_COEF_D = (short)val;
        break;

      case H_Reverb + 14:
        rvb.IIR_COEF = (short)val;
        break;

      case H_Reverb + 16:
        rvb.FB_ALPHA = (short)val;
        break;

      case H_Reverb + 18:
        rvb.FB_X = (short)val;
        break;

      case H_Reverb + 20:
        rvb.IIR_DEST_A0 = (short)val;
        break;

      case H_Reverb + 22:
        rvb.IIR_DEST_A1 = (short)val;
        break;

      case H_Reverb + 24:
        rvb.ACC_SRC_A0 = (short)val;
        break;

      case H_Reverb + 26:
        rvb.ACC_SRC_A1 = (short)val;
        break;

      case H_Reverb + 28:
        rvb.ACC_SRC_B0 = (short)val;
        break;

      case H_Reverb + 30:
        rvb.ACC_SRC_B1 = (short)val;
        break;

      case H_Reverb + 32:
        rvb.IIR_SRC_A0 = (short)val;
        break;

      case H_Reverb + 34:
        rvb.IIR_SRC_A1 = (short)val;
        break;

      case H_Reverb + 36:
        rvb.IIR_DEST_B0 = (short)val;
        break;

      case H_Reverb + 38:
        rvb.IIR_DEST_B1 = (short)val;
        break;

      case H_Reverb + 40:
        rvb.ACC_SRC_C0 = (short)val;
        break;

      case H_Reverb + 42:
        rvb.ACC_SRC_C1 = (short)val;
        break;

      case H_Reverb + 44:
        rvb.ACC_SRC_D0 = (short)val;
        break;

      case H_Reverb + 46:
        rvb.ACC_SRC_D1 = (short)val;
        break;

      case H_Reverb + 48:
        rvb.IIR_SRC_B1 = (short)val;
        break;

      case H_Reverb + 50:
        rvb.IIR_SRC_B0 = (short)val;
        break;

      case H_Reverb + 52:
        rvb.MIX_DEST_A0 = (short)val;
        break;

      case H_Reverb + 54:
        rvb.MIX_DEST_A1 = (short)val;
        break;

      case H_Reverb + 56:
        rvb.MIX_DEST_B0 = (short)val;
        break;

      case H_Reverb + 58:
        rvb.MIX_DEST_B1 = (short)val;
        break;

      case H_Reverb + 60:
        rvb.IN_COEF_L = (short)val;
        break;

      case H_Reverb + 62:
        rvb.IN_COEF_R = (short)val;
        break;
   }
}

u16 SPU_ReadRegister(u32 reg)
{
    const u32       r = reg & 0xfff;

    if (r >= 0x0c00 && r < 0x0d80)
    {
        switch (r & 0x0f)
        {
          case 12: // get adsr vol
            {
            const int   ch = (r >> 4) - 0xc0;

            if (s_chan[ch].bNew)
                return 1; // we are started, but not processed? return 1

            // same here... we haven't decoded one sample yet, so no envelope yet. return 1 as well
            if (s_chan[ch].adsr.lVolume && !s_chan[ch].adsr.EnvelopeVol)
                return 1;

            return (u16)(s_chan[ch].adsr.EnvelopeVol);
            }
        }
    }

    switch (r)
    {
      case H_SPUaddr:
        return spuAddr >> 3;

      case H_SPUctrl:
        return spuCtrl;

      case H_SPUstat:
        return spuStat;

      case H_SPUdata:
        {
            u16     s = spuMem[spuAddr >> 1];

        spuAddr += 2;
        if (spuAddr > 0x7ffff)
            spuAddr = 0;

        return s;
        }

        //case H_SPUIsOn1:
        // return IsSoundOn(0,16);

        //case H_SPUIsOn2:
        // return IsSoundOn(16,24);
    }

    return regArea[(r - 0xc00) >> 1];
}
