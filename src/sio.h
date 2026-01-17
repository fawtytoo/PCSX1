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

#ifndef _SIO_H_
#define _SIO_H_

#include "psxcommon.h"
#include "r3000a.h"
#include "psxmem.h"

#define MCD_SECT_SIZE   (8 * 16)
#define MCD_SIZE        (1024 * MCD_SECT_SIZE)

extern char MemCard[2][MCD_SIZE];
extern int  mcdAmended[2];
extern u8   cardh[4];

void sioWrite8(u8 value);
void sioWriteStat16(u16 value);
void sioWriteMode16(u16 value);
void sioWriteCtrl16(u16 value);
void sioWriteBaud16(u16 value);

u8  sioRead8();
u16 sioReadStat16();
u16 sioReadMode16();
u16 sioReadCtrl16();
u16 sioReadBaud16();

void netError();

void sioInterrupt();

void MCD_Load(int, char *);
void MCD_Save(int, char *);
void MCD_Format(int);

void SIO1irq(void);

#endif
