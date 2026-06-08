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
* Internal PSX HLE functions.
*/

#include "system.h"

#include "psxhle.h"

static void hleDummy()
{
    psxRegs.pc = psxRegs.GPR.n.ra;

    psxBranchTest();
}

static void hleA0()
{
    biosA0[psxRegs.GPR.n.t1 & 0xff]();

    psxBranchTest();
}

static void hleB0()
{
    biosB0[psxRegs.GPR.n.t1 & 0xff]();

    psxBranchTest();
}

static void hleC0()
{
    biosC0[psxRegs.GPR.n.t1 & 0xff]();

    psxBranchTest();
}

static void hleBootstrap() // 0xbfc00000
{
    //SysPrintf("hleBootstrap\n");
    //CheckCdrom();
    LoadCdrom();
    //SysPrintf("CdromLabel: \"%s\": PC = %8.8x (SP = %8.8x)\n", CdromLabel, (unsigned int)psxRegs.pc, (unsigned int)psxRegs.GPR.n.sp);
}

typedef struct {
    u32 _pc0;
    u32 gp0;
    u32 t_addr;
    u32 t_size;
    u32 d_addr;
    u32 d_size;
    u32 b_addr;
    u32 b_size;
    u32 S_addr;
    u32 s_size;
    u32 _sp,_fp,_gp,ret,base;
} EXEC;

static void hleExecRet() {
    EXEC *header = (EXEC*)PSXM(psxRegs.GPR.n.s0);

    SysPrintf("ExecRet %x: %x\n", psxRegs.GPR.n.s0, header->ret);

    psxRegs.GPR.n.ra = header->ret;
    psxRegs.GPR.n.sp = header->_sp;
    psxRegs.GPR.n.s8 = header->_fp;
    psxRegs.GPR.n.gp = header->_gp;
    psxRegs.GPR.n.s0 = header->base;

    psxRegs.GPR.n.v0 = 1;
    psxRegs.pc = psxRegs.GPR.n.ra;
}

void (*psxHLEt[256])() = {
    hleDummy, hleA0, hleB0, hleC0,
    hleBootstrap, hleExecRet,
    hleDummy, hleDummy
};
