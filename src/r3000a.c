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
* R3000A CPU functions.
*/

#include "system.h"
#include "cdrom.h"
#include "mdec.h"
#include "gpu.h"
#include "gte.h"

#include "r3000a.h"

psxRegisters psxRegs;

void PSX_Reset()
{
    PsxCpuReset();

    psxMemReset();

    memset(&psxRegs, 0, sizeof(psxRegs));

    psxRegs.pc = 0xbfc00000; // Start in bootstrap

    psxRegs.CP0.r[12] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
    psxRegs.CP0.r[15] = 0x00000002; // PRevID = Revision ID, same as R3000A

    psxHwReset();
    psxBiosInit();

    if (Config.HLE == false)
    {
        psxExecuteBios();
        if (Config.SlowBoot == false)
        {
            psxRegs.pc = psxRegs.GPR.n.ra;
        }
    }
}

void psxException(u32 code, u32 bd) {
    // Set the Cause
    psxRegs.CP0.n.Cause = code;

    // Set the EPC & PC
    if (bd) {
        SysPrintf("bd set!!!\n");
        psxRegs.CP0.n.Cause |= 0x80000000;
        psxRegs.CP0.n.EPC = (psxRegs.pc - 4);
    } else
        psxRegs.CP0.n.EPC = (psxRegs.pc);

    if (psxRegs.CP0.n.Status & 0x400000)
        psxRegs.pc = 0xbfc00180;
    else
        psxRegs.pc = 0x80000080;

    // Set the Status
    psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status &~0x3f) |
                          ((psxRegs.CP0.n.Status & 0xf) << 2);

    if (Config.HLE) psxBiosException();
}

void psxBranchTest() {
    // GameShark Sampler: Give VSync pin some delay before exception eats it
    if (psxHu32(0x1070) & psxHu32(0x1074)) {
        if ((psxRegs.CP0.n.Status & 0x401) == 0x401) {
            u32 opcode;

            // Crash Bandicoot 2: Don't run exceptions when GTE in pipeline
            opcode = SWAP32(*Read_ICache(psxRegs.pc, true));
            if( ((opcode >> 24) & 0xfe) != 0x4a ) {
                psxException(0x400, 0);
            }
        }
    }

#if 0
    if( SPU_async )
    {
        static int init;
        int elapsed;

        if( init == 0 ) {
            // 10 apu cycles
            // - Final Fantasy Tactics (distorted - dropped sound effects)
            psxRegs.intCycle[PSXINT_SPUASYNC].cycle = PSXCLK / 44100 * 10;

            init = 1;
        }

        elapsed = psxRegs.cycle - psxRegs.intCycle[PSXINT_SPUASYNC].sCycle;
        if (elapsed >= psxRegs.intCycle[PSXINT_SPUASYNC].cycle) {
            SPU_async( elapsed );

            psxRegs.intCycle[PSXINT_SPUASYNC].sCycle = psxRegs.cycle;
        }
    }
#endif

    if ((psxRegs.cycle - psxNextsCounter) >= psxNextCounter)
        psxRcntUpdate();

    if (psxRegs.interrupt) {
        if ((psxRegs.interrupt & (1 << PSXINT_SIO)) && !Config.SioIrq) { // sio
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_SIO].sCycle) >= psxRegs.intCycle[PSXINT_SIO].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_SIO);
                sioInterrupt();
            }
        }
        if (psxRegs.interrupt & (1 << PSXINT_CDR)) { // cdr
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDR].sCycle) >= psxRegs.intCycle[PSXINT_CDR].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_CDR);
                cdrInterrupt();
            }
        }
        if (psxRegs.interrupt & (1 << PSXINT_CDREAD)) { // cdr read
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDREAD].sCycle) >= psxRegs.intCycle[PSXINT_CDREAD].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_CDREAD);
                cdrReadInterrupt();
            }
        }
        if (psxRegs.interrupt & (1 << PSXINT_GPUDMA)) { // gpu dma
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_GPUDMA].sCycle) >= psxRegs.intCycle[PSXINT_GPUDMA].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_GPUDMA);
                gpuInterrupt();
            }
        }
        if (psxRegs.interrupt & (1 << PSXINT_MDECOUTDMA)) { // mdec out dma
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_MDECOUTDMA].sCycle) >= psxRegs.intCycle[PSXINT_MDECOUTDMA].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_MDECOUTDMA);
                mdec1Interrupt();
            }
        }
        if (psxRegs.interrupt & (1 << PSXINT_SPUDMA)) { // spu dma
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_SPUDMA].sCycle) >= psxRegs.intCycle[PSXINT_SPUDMA].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_SPUDMA);
                spuInterrupt();
            }
        }
        if (psxRegs.interrupt & (1 << PSXINT_MDECINDMA)) { // mdec in
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_MDECINDMA].sCycle) >= psxRegs.intCycle[PSXINT_MDECINDMA].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_MDECINDMA);
                mdec0Interrupt();
            }
        }

        if (psxRegs.interrupt & (1 << PSXINT_GPUOTCDMA)) { // gpu otc
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_GPUOTCDMA].sCycle) >= psxRegs.intCycle[PSXINT_GPUOTCDMA].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_GPUOTCDMA);
                gpuotcInterrupt();
            }
        }

        if (psxRegs.interrupt & (1 << PSXINT_CDRDMA)) { // cdrom
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDRDMA].sCycle) >= psxRegs.intCycle[PSXINT_CDRDMA].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_CDRDMA);
                cdrDmaInterrupt();
            }
        }

        if (psxRegs.interrupt & (1 << PSXINT_CDRPLAY)) { // cdr play timing
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDRPLAY].sCycle) >= psxRegs.intCycle[PSXINT_CDRPLAY].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_CDRPLAY);
                cdrPlayInterrupt();
            }
        }

        if (psxRegs.interrupt & (1 << PSXINT_CDRDBUF)) { // cdr decoded buffer
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDRDBUF].sCycle) >= psxRegs.intCycle[PSXINT_CDRDBUF].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_CDRDBUF);
                cdrDecodedBufferInterrupt();
            }
        }

        if (psxRegs.interrupt & (1 << PSXINT_CDRLID)) { // cdr lid states
            if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDRLID].sCycle) >= psxRegs.intCycle[PSXINT_CDRLID].cycle) {
                psxRegs.interrupt &= ~(1 << PSXINT_CDRLID);
                cdrLidSeekInterrupt();
            }
        }
    }
}

void psxJumpTest()
{
    if (Config.HLE == true && Config.PsxOut == false)
    {
        return;
    }

    u32     call = psxRegs.GPR.n.t1 & 0xff;

    switch (psxRegs.pc & 0x1fffff)
    {
      case 0xa0:
        biosA0[call]();
        break;

      case 0xb0:
        biosB0[call]();
        break;

      case 0xc0:
        biosC0[call]();
        break;
    }
}

void psxExecuteBios() {
    while (psxRegs.pc != 0x80030000)
        PsxCpuExecuteBlock();
}
