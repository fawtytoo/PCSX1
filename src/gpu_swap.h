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

#define HOST2LE32(x) (x)
#define LE2HOST32(x) (x)

#define HOST2LE16(x) (x)
#define LE2HOST16(x) (x)

#define GETLEs16(X) ((s16)GETLE16((u16 *)X))
#define GETLEs32(X) ((s16)GETLE32((u16 *)X))

#define GETLE16(X) LE2HOST16(*(u16 *)X)
#define GETLE32(X) LE2HOST32(*(u32 *)X)

#define PUTLE16(X, Y)   *((u16 *)X) = (u16)Y;
#define PUTLE32(X, Y)   *((u32 *)X) = (u32)Y;
