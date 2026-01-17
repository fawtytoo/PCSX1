/***************************************************************************
 *   Copyright (C) 2007 PCSX-df Team                                       *
 *   Copyright (C) 2009 Wei Mingzhi                                        *
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

#ifndef CDRISO_H
#define CDRISO_H

typedef struct
{
    u32 Type;
    u32 Status;
    char    Time[3];
}
CDRSTAT;

typedef struct
{
    s8  res0[12];
    u8  ControlAndADR;
    u8  TrackNumber;
    u8  IndexNumber;
    u8  TrackRelativeAddress[3];
    u8  Filler;
    u8  AbsoluteAddress[3];
    u8  CRC[2];
    s8  res1[72];
}
SUBQ;

u32 msf2sec(char *);
void sec2msf(u32, char *);

int ISO_Open(char *);
void ISO_Close(void);
void ISO_GetTN(u8 *);
void ISO_GetTD(u8, char *);
int ISO_ReadTrack(u8 *);
u8 *ISO_GetBuffer(void);
u8 *ISO_GetBufferSub(void);
void ISO_Play(char *);
void ISO_Stop(void);
long ISO_GetStatus(CDRSTAT *);
void ISO_ReadCDDA(u8, u8, u8, u8 *);

void ISO_LidInterrupt(void);

#endif
