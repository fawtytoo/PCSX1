#ifndef __PAD_H__
#define __PAD_H__

#include "psxcommon.h"

void PAD_Init(int);
void PAD_Button(int, int, int);
void PAD_Motion(int, s8, s8);
void PAD_Axis(int, int, short);
u8 PAD_StartPoll(int);
u8 PAD_Poll(u8);

#endif
