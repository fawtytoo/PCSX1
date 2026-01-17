/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include "psxcommon.h"

#define NULL_SIZE   &(int){0}

int File_Load(const char *, char **, int [static 1]);
void File_Save(const char *, const char *, const int);
int File_DiscOpen(const char *);
void File_DiscClose(void);
void File_DiscSeek(u32);
int File_DiscRead(void *, size_t);
void Cfg_Load(const char *);
void SysPrintf(const char *, ...);
void SysWaitTime(void);
void SysSetFrameRate(int);

// main.c
void SysUpdate(void);
void UpdateVideo(void);
void WindowSize(int, int);

// gpu/gpu.c
void DoBufferSwap(u32 *);

#endif
