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
