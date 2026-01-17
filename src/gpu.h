#ifndef __GPU_H__
#define __GPU_H__

#define PSXWIDTH    640
#define PSXHEIGHT   512

int gpuReadStatus();

void psxDma2(u32 madr, u32 bcr, u32 chcr);
void gpuInterrupt();

void GPU_Open(void);
void GPU_Close(void);
void GPU_WriteStatus(u32);
void GPU_WriteData(u32);
void GPU_WriteDataMem(u32 *, int);
u32 GPU_ReadStatus(void);
u32 GPU_ReadData(void);
void GPU_ReadDataMem(u32 *, int);
long GPU_DmaChain(u32 *, u32);
void GPU_UpdateLace(void);
void GPU_HSync(int);
void GPU_VBlank(int);

#endif
