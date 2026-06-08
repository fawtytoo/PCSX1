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

static int  RateTableAdd[128];
static int  RateTableAdd_f[128];
static int  RateTableSub[128];
static int  RateTableSub_f[128];
static const int RateTable_denom = 1 << (((4 * 32) >> 2) - 11);

void InitADSR()
{
    int lcv;

    memset(RateTableAdd, 0, sizeof(int) * 128);
    memset(RateTableAdd_f, 0, sizeof(int) * 128);
    memset(RateTableSub, 0, sizeof(int) * 128);
    memset(RateTableSub_f, 0, sizeof(int) * 128);

    // Optimize table - Dr. Hell ADSR math
    for (lcv = 0; lcv < 48; lcv++)
    {
        RateTableAdd[lcv] = (7 - (lcv & 3)) << (11 - (lcv >> 2));
        RateTableSub[lcv] = (-8 + (lcv & 3)) << (11 - (lcv >> 2));

        RateTableAdd_f[lcv] = 0;
        RateTableSub_f[lcv] = 0;
    }

    for (lcv = 48; lcv < 128; lcv++)
    {
        int denom;

        denom = 1 << ((lcv >> 2) - 11);

        // whole
        RateTableAdd[lcv] = (7 - (lcv & 3)) / denom;
        RateTableSub[lcv] = (-8 + (lcv & 3)) / denom;

        // fraction
        RateTableAdd_f[lcv] = (7 - (lcv & 3)) % denom;
        RateTableSub_f[lcv] = (-8 + (lcv & 3)) % denom;

        RateTableAdd_f[lcv] *= RateTable_denom / denom;
        RateTableSub_f[lcv] *= RateTable_denom / denom;

        // goofy compiler - mod
        if (RateTableSub_f[lcv] > 0)
            RateTableSub_f[lcv] = -RateTableSub_f[lcv];
    }
}

void StartADSR(int ch)
{
    s_chan[ch].adsr.lVolume = 1;
    s_chan[ch].adsr.State = 0;
    s_chan[ch].adsr.EnvelopeVol = 0;
    s_chan[ch].adsr.EnvelopeVol_f = 0;
}

int MixADSR(int ch)
{
    int EnvelopeVol = s_chan[ch].adsr.EnvelopeVol;
    int EnvelopeVol_f = s_chan[ch].adsr.EnvelopeVol_f;

    // dead volume - voice on
    if (s_chan[ch].iSilent == 2)
    {
        if (s_chan[ch].bStop)
            s_chan[ch].bOn = 0;

        return 0;
    }

    if (s_chan[ch].bStop) // should be stopped:
    {                                                    // do release
        if (s_chan[ch].adsr.ReleaseModeExp)
            EnvelopeVol += (RateTableSub[s_chan[ch].adsr.ReleaseRate * 4] * EnvelopeVol) >> 15;
        else
            EnvelopeVol += RateTableSub[s_chan[ch].adsr.ReleaseRate * 4];

        EnvelopeVol_f += RateTableSub_f[s_chan[ch].adsr.ReleaseRate * 4];
        if (EnvelopeVol_f < 0)
        {
            EnvelopeVol_f += RateTable_denom;
            EnvelopeVol--;
        }

        if (EnvelopeVol < 0)
        {
            EnvelopeVol = 0;
            EnvelopeVol_f = 0;
            // don't stop if this chan can still cause irqs
            if (!(spuCtrl & 0x40) || (s_chan[ch].pCurr > pSpuIrq && s_chan[ch].pLoop > pSpuIrq))
                s_chan[ch].bOn = 0;
        }

        s_chan[ch].adsr.EnvelopeVol = EnvelopeVol;
        s_chan[ch].adsr.EnvelopeVol_f = EnvelopeVol_f;
        s_chan[ch].adsr.lVolume = EnvelopeVol >> 5;
        return EnvelopeVol >> 5;
    }
    else // not stopped yet?
    {
        if (s_chan[ch].adsr.State == 0) // -> attack
        {
            if (s_chan[ch].adsr.AttackModeExp)
            {
                if (EnvelopeVol >= 0x6000)
                {
                    EnvelopeVol += RateTableAdd[s_chan[ch].adsr.AttackRate + 8];
                    EnvelopeVol_f += RateTableAdd_f[ s_chan[ch].adsr.AttackRate + 8];
                }
                else
                {
                    EnvelopeVol += RateTableAdd[s_chan[ch].adsr.AttackRate + 0];
                    EnvelopeVol_f += RateTableAdd_f[s_chan[ch].adsr.AttackRate + 0];
                }
            }
            else
            {
                EnvelopeVol += RateTableAdd[s_chan[ch].adsr.AttackRate + 0];
                EnvelopeVol_f += RateTableAdd_f[s_chan[ch].adsr.AttackRate + 0];
            }

            if (EnvelopeVol_f >= RateTable_denom)
            {
                EnvelopeVol_f -= RateTable_denom;
                EnvelopeVol++;
            }

            if (EnvelopeVol >= 0x8000)
            {
                EnvelopeVol = 0x7FFF;
                EnvelopeVol_f = RateTable_denom;
                s_chan[ch].adsr.State = 1;
            }

            s_chan[ch].adsr.EnvelopeVol = EnvelopeVol;
            s_chan[ch].adsr.EnvelopeVol_f = EnvelopeVol_f;
            s_chan[ch].adsr.lVolume = EnvelopeVol >> 5;
            return EnvelopeVol >> 5;
        }

        if (s_chan[ch].adsr.State == 1) // -> decay
        {
            EnvelopeVol += (RateTableSub[s_chan[ch].adsr.DecayRate * 4] * EnvelopeVol) >> 15;
            EnvelopeVol_f += RateTableSub_f[s_chan[ch].adsr.DecayRate * 4];
            if (EnvelopeVol_f < 0)
            {
                EnvelopeVol_f += RateTable_denom;
                EnvelopeVol--;
            }

            if (EnvelopeVol < 0)
            {
                EnvelopeVol = 0;
                EnvelopeVol_f = 0;
            }

            // FF7 cursor - use Neill's 4-bit accuracy
            if (((EnvelopeVol >> 11) & 0xf) <= s_chan[ch].adsr.SustainLevel)
            {
                s_chan[ch].adsr.State = 2;
            }

            s_chan[ch].adsr.EnvelopeVol = EnvelopeVol;
            s_chan[ch].adsr.EnvelopeVol_f = EnvelopeVol_f;
            s_chan[ch].adsr.lVolume = EnvelopeVol >> 5;
            return EnvelopeVol >> 5;
        }

        if (s_chan[ch].adsr.State == 2) // -> sustain
        {
            if (s_chan[ch].adsr.SustainIncrease)
            {
                if (s_chan[ch].adsr.SustainModeExp)
                {
                    if (EnvelopeVol >= 0x6000)
                    {
                        EnvelopeVol += RateTableAdd[s_chan[ch].adsr.SustainRate + 8];
                        EnvelopeVol_f += RateTableAdd_f[s_chan[ch].adsr.SustainRate + 8];
                    }
                    else
                    {
                        EnvelopeVol += RateTableAdd[s_chan[ch].adsr.SustainRate + 0];
                        EnvelopeVol_f += RateTableAdd_f[s_chan[ch].adsr.SustainRate + 0];
                    }
                }
                else
                {
                    EnvelopeVol += RateTableAdd[s_chan[ch].adsr.SustainRate + 0];
                    EnvelopeVol_f += RateTableAdd_f[s_chan[ch].adsr.SustainRate + 0];
                }

                if (EnvelopeVol_f >= RateTable_denom)
                {
                    EnvelopeVol_f -= RateTable_denom;
                    EnvelopeVol++;
                }

                if (EnvelopeVol >= 0x8000)
                {
                    EnvelopeVol = 0x7FFF;
                    EnvelopeVol_f = RateTable_denom;
                }
            }
            else
            {
                if (s_chan[ch].adsr.SustainModeExp)
                    EnvelopeVol += (RateTableSub[s_chan[ch].adsr.SustainRate] * EnvelopeVol) >> 15;
                else
                    EnvelopeVol += RateTableSub[s_chan[ch].adsr.SustainRate];

                EnvelopeVol_f += RateTableSub_f[s_chan[ch].adsr.SustainRate];
                if (EnvelopeVol_f < 0)
                {
                    EnvelopeVol_f += RateTable_denom;
                    EnvelopeVol--;
                }

                if (EnvelopeVol < 0)
                {
                    EnvelopeVol = 0;
                    EnvelopeVol_f = 0;
                }
            }

            s_chan[ch].adsr.EnvelopeVol = EnvelopeVol;
            s_chan[ch].adsr.EnvelopeVol_f = EnvelopeVol_f;
            s_chan[ch].adsr.lVolume = EnvelopeVol >> 5;
            return EnvelopeVol >> 5;
        }
    }
    return 0;
}
