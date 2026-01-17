#ifndef __XA__
#define __XA__

#include "decode_xa.h"

void MixXA(int *, int *);
void FeedXA(xa_decode_t *xap);
void FeedCDDA(u8 *pcm, int nBytes);

#endif
