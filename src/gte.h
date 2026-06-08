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

#ifndef __GTE_H__
#define __GTE_H__

#include "psxcommon.h"
#include "r3000a.h"

void gteMFC2();
void gteCFC2();
void gteMTC2();
void gteCTC2();
void gteLWC2();
void gteSWC2();

void gteRTPS();
void gteNCLIP();
void gteOP();
void gteDPCS();
void gteINTPL();
void gteMVMVA();
void gteNCDS();
void gteCDP();
void gteNCDT();
void gteNCCS();
void gteCC();
void gteNCS();
void gteNCT();
void gteSQR();
void gteDCPL();
void gteDPCT();
void gteAVSZ3();
void gteAVSZ4();
void gteRTPT();
void gteGPF();
void gteGPL();
void gteNCCT();

#endif
