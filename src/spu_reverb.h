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

#ifndef __REVERB__
#define __REVERB__

typedef struct
{
    int StartAddr;      // reverb area start addr in samples
    int CurrAddr;       // reverb area curr addr in samples

    int VolLeft;
    int VolRight;
    int iLastRVBLeft;
    int iLastRVBRight;
    int iRVBLeft;
    int iRVBRight;

    int FB_SRC_A, FB_SRC_B;         // (offset)
    int IIR_ALPHA;                  // (coef.)
    int ACC_COEF_A, ACC_COEF_B, ACC_COEF_C, ACC_COEF_D; // (coef.)
    int IIR_COEF;                   // (coef.)
    int FB_ALPHA;                   // (coef.)
    int FB_X;                       // (coef.)
    int IIR_DEST_A0, IIR_DEST_A1;   // (offset)
    int ACC_SRC_A0, ACC_SRC_A1;     // (offset)
    int ACC_SRC_B0, ACC_SRC_B1;     // (offset)
    int IIR_SRC_A0, IIR_SRC_A1;     // (offset)
    int IIR_DEST_B0, IIR_DEST_B1;   // (offset)
    int ACC_SRC_C0, ACC_SRC_C1;     // (offset)
    int ACC_SRC_D0, ACC_SRC_D1;     // (offset)
    int IIR_SRC_B1, IIR_SRC_B0;     // (offset)
    int MIX_DEST_A0, MIX_DEST_A1;   // (offset)
    int MIX_DEST_B0, MIX_DEST_B1;   // (offset)
    int IN_COEF_L, IN_COEF_R;       // (coef.)
}
REVERB;

extern REVERB   rvb;
extern int      iUseReverb;

void InitREVERB(void);
void SetREVERB(u16);
void StartREVERB(int);
void StoreREVERB(int);

void StoreREVERB_CD(int, int);

int MixREVERBLeft(void);
int MixREVERBRight(void);

#endif
