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

#ifndef _GPU_SOFT_H_
#define _GPU_SOFT_H_

void offsetPSXLine(void);
void offsetPSX2(void);
void offsetPSX3(void);
void offsetPSX4(void);

void FillSoftwareAreaTrans(short, short, short, short, u16);
void FillSoftwareArea(short, short, short, short, u16);
void drawPoly3G(s32, s32, s32);
void drawPoly4G(s32, s32, s32, s32);
void drawPoly3F(s32);
void drawPoly4F(s32);
void drawPoly4FT(u8*);
void drawPoly4GT(u8*);
void drawPoly3FT(u8*);
void drawPoly3GT(u8*);
void DrawSoftwareSprite(u8*, short, short, s32, s32);
void DrawSoftwareSpriteTWin(u8*, s32, s32);
void DrawSoftwareSpriteMirror(u8*, s32, s32);
void DrawSoftwareLineShade(s32, s32);
void DrawSoftwareLineFlat(s32);

#endif // _GPU_SOFT_H_
