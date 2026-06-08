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
* Miscellaneous functions, including CD-ROM loading.
*/

#include "system.h"
#include "cdrom.h"
#include "mdec.h"

#include "misc.h"

char CdromId[10] = "";
char CdromLabel[33] = "";

#define ISODCL(from, to)    (to - from + 1)

typedef struct
{
    char    length[ISODCL(1, 1)]; /* 711 */
    char    ext_attr_length[ISODCL(2, 2)]; /* 711 */
    char    extent[ISODCL(3, 10)]; /* 733 */
    char    size[ISODCL(11, 18)]; /* 733 */
    char    date[ISODCL(19, 25)]; /* 7 by 711 */
    char    flags[ISODCL(26, 26)];
    char    file_unit_size[ISODCL(27, 27)]; /* 711 */
    char    interleave[ISODCL(28, 28)]; /* 711 */
    char    volume_sequence_number[ISODCL(29, 32)]; /* 723 */
    u8      name_len[ISODCL(33, 33)]; /* 711 */
    char    name[1];
}
CDDIRRECORD;

//local extern

void mmssdd(char *b, char *p)
{
    int m, s, d;
#if defined(__BIGENDIAN__)
    int block = (b[0] & 0xff) | ((b[1] & 0xff) << 8) | ((b[2] & 0xff) << 16) | (b[3] << 24);
#else
    int block = *((int *)b);
#endif

    block += 150;
    m = block / 4500;           // minutes
    block = block - m * 4500;   // minutes rest
    s = block / 75;             // seconds
    d = block - s * 75;         // seconds rest

    m = ((m / 10) << 4) | m % 10;
    s = ((s / 10) << 4) | s % 10;
    d = ((d / 10) << 4) | d % 10;

    p[0] = m;
    p[1] = s;
    p[2] = d;
}

#define incTime() \
    time[0] = btoi(time[0]); \
    time[1] = btoi(time[1]); \
    time[2] = btoi(time[2]); \
    time[2]++; \
    if(time[2] == 75) \
    { \
        time[2] = 0; \
        time[1]++; \
        if (time[1] == 60) \
        { \
            time[1] = 0; \
            time[0]++; \
        } \
    } \
    time[0] = itob(time[0]); \
    time[1] = itob(time[1]); \
    time[2] = itob(time[2]);

#define READTRACK() \
    if (ISO_ReadTrack(time) == 0) \
    { \
        return false; \
    } \
    buf = ISO_GetBuffer(); \
    if (buf == NULL) \
    { \
        return false; \
    }

#define READDIR(_dir) \
    READTRACK(); \
    memcpy(_dir, buf + 12, 2048); \
    incTime(); \
    READTRACK(); \
    memcpy(_dir + 2048, buf + 12, 2048);

bool GetCdromFile(char *mdir, u8 *time, char *filename)
{
    CDDIRRECORD *dir;
    char        ddir[4096];
    u8          *buf;
    int         i;

    // only try to scan if a filename is given
    if (strlen(filename) == 0)
    {
        return false;
    }

    i = 0;
    while (i < 4096)
    {
        dir = (CDDIRRECORD *)&mdir[i];
        if (dir->length[0] == 0)
        {
            return false;
        }
        i += dir->length[0];

        if (dir->flags[0] & 0x2) // it's a dir
        {
            if (!strncasecmp((char *)&dir->name[0], filename, dir->name_len[0]))
            {
                if (filename[dir->name_len[0]] != '\\')
                {
                    continue;
                }

                filename += dir->name_len[0] + 1;

                mmssdd(dir->extent, (char *)time);
                READDIR(ddir);
                i = 0;
                mdir = ddir;
            }
        }
        else
        {
            if (!strncasecmp((char *)&dir->name[0], filename, strlen(filename)))
            {
                mmssdd(dir->extent, (char *)time);
                break;
            }
        }
    }
    return true;
}

bool LoadCdrom()
{
    EXE_HEADER  tmpHead;
    CDDIRRECORD *dir;
    u8          time[4], *buf;
    char        mdir[4096];
    char        exename[256];

    time[0] = itob(0);
    time[1] = itob(2);
    time[2] = itob(0x10);

    READTRACK();

    // skip head and sub, and go to the root directory record
    dir = (CDDIRRECORD *)&buf[12 + 156];

    mmssdd(dir->extent, (char *)time);

    READDIR(mdir);

    // Load SYSTEM.CNF and scan for the main executable
    if (GetCdromFile(mdir, time, "SYSTEM.CNF;1") == false)
    {
        // if SYSTEM.CNF is missing, start an existing PSX.EXE
        if (GetCdromFile(mdir, time, "PSX.EXE;1") == false)
        {
            return false;
        }

        READTRACK();
    }
    else
    {
        // read the SYSTEM.CNF
        READTRACK();

        sscanf((char *)buf + 12, "BOOT = cdrom:\\%255s", exename);
        if (GetCdromFile(mdir, time, exename) == false)
        {
            sscanf((char *)buf + 12, "BOOT = cdrom:%255s", exename);
            if (GetCdromFile(mdir, time, exename) == false)
            {
                char    *ptr = strstr((char *)buf + 12, "cdrom:");
                if (ptr != NULL)
                {
                    ptr += 6;
                    while (*ptr == '\\' || *ptr == '/')
                    {
                        ptr++;
                    }
                    strncpy(exename, ptr, 255);
                    exename[255] = '\0';
                    ptr = exename;
                    while (*ptr != '\0' && *ptr != '\r' && *ptr != '\n')
                    {
                        ptr++;
                    }
                    *ptr = '\0';
                    if (GetCdromFile(mdir, time, exename) == false)
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
        }

        // Read the EXE-Header
        READTRACK();
    }

    memcpy(&tmpHead, buf + 12, sizeof(EXE_HEADER));

    psxRegs.pc = SWAP32(tmpHead.pc0);
    psxRegs.GPR.n.gp = SWAP32(tmpHead.gp0);
    psxRegs.GPR.n.sp = SWAP32(tmpHead.s_addr);
    if (psxRegs.GPR.n.sp == 0)
    {
        psxRegs.GPR.n.sp = 0x801fff00;
    }

    tmpHead.t_size = SWAP32(tmpHead.t_size);
    tmpHead.t_addr = SWAP32(tmpHead.t_addr);

    // Read the rest of the main executable
    while (tmpHead.t_size)
    {
        void    *ptr = (void *)PSXM(tmpHead.t_addr);

        incTime();
        READTRACK();

        if (ptr != NULL)
        {
            memcpy(ptr, buf + 12, 2048);
        }

        tmpHead.t_size -= 2048;
        tmpHead.t_addr += 2048;
    }

    return true;
}

bool LoadCdromFile(const char *filename, EXE_HEADER *head)
{
    CDDIRRECORD *dir;
    u8          time[4],*buf;
    char        mdir[4096], exename[256];
    u32         size, addr;
    void        *psxaddr;

    if (sscanf(filename, "cdrom:\\%255s", exename) <= 0)
    {
        // Some games omit backslash (NFS4)
        if (sscanf(filename, "cdrom:%255s", exename) <= 0)
        {
            SysPrintf("LoadCdromFile: EXE NAME PARSING ERROR (%s (%u))\n", filename, strlen(filename));
            exit(1);
        }
    }

    time[0] = itob(0);
    time[1] = itob(2);
    time[2] = itob(0x10);

    READTRACK();

    // skip head and sub, and go to the root directory record
    dir = (CDDIRRECORD *)&buf[12 + 156];

    mmssdd(dir->extent, (char *)time);

    READDIR(mdir);

    if (GetCdromFile(mdir, time, exename) == false)
    {
        return false;
    }

    READTRACK();

    memcpy(head, buf + 12, sizeof(EXE_HEADER));
    size = head->t_size;
    addr = head->t_addr;

    // Cache clear/invalidate dynarec/int. Fixes startup of Casper/X-Files and possibly others.
    psxRegs.ICache_valid = false;

    while (size)
    {
        incTime();
        READTRACK();

        psxaddr = (void *)PSXM(addr);
        assert(psxaddr != NULL);
        memcpy(psxaddr, buf + 12, 2048);

        size -= 2048;
        addr += 2048;
    }

    return true;
}

bool CheckCdrom()
{
    CDDIRRECORD *dir;
    u8          time[4], *buf;
    char        mdir[4096];
    char        exename[256];
    int         i, len, c;

    time[0] = itob(0);
    time[1] = itob(2);
    time[2] = itob(0x10);

    READTRACK();

    memset(CdromLabel, 0, sizeof(CdromLabel));
    memset(CdromId, 0, sizeof(CdromId));
    memset(exename, 0, sizeof(exename));

    strncpy(CdromLabel, (char *)buf + 52, 32);

    // skip head and sub, and go to the root directory record
    dir = (CDDIRRECORD *)&buf[12 + 156];

    mmssdd(dir->extent, (char *)time);

    READDIR(mdir);

    if (GetCdromFile(mdir, time, "SYSTEM.CNF;1") != false)
    {
        READTRACK();

        sscanf((char *)buf + 12, "BOOT = cdrom:\\%255s", exename);
        if (GetCdromFile(mdir, time, exename) == false)
        {
            sscanf((char *)buf + 12, "BOOT = cdrom:%255s", exename);
            if (GetCdromFile(mdir, time, exename) == false)
            {
                char    *ptr = strstr((char *)buf + 12, "cdrom:"); // possibly the executable is in some subdir
                if (ptr != NULL)
                {
                    ptr += 6;
                    while (*ptr == '\\' || *ptr == '/')
                    {
                        ptr++;
                    }
                    strncpy(exename, ptr, 255);
                    exename[255] = '\0';
                    ptr = exename;
                    while (*ptr != '\0' && *ptr != '\r' && *ptr != '\n')
                    {
                        ptr++;
                    }
                    *ptr = '\0';
                    if (GetCdromFile(mdir, time, exename) == false)
                    {
                        return false; // main executable not found
                    }
                }
                else
                {
                    return false;
                }
            }
        }
    }
    else if (GetCdromFile(mdir, time, "PSX.EXE;1") != false)
    {
        strcpy(exename, "PSX.EXE;1");
        strcpy(CdromId, "SLUS99999");
    }
    else
    {
        return false; // SYSTEM.CNF and PSX.EXE not found
    }

    if (CdromId[0] == '\0')
    {
        len = strlen(exename);
        c = 0;
        for (i = 0; i < len; i++)
        {
            if (exename[i] == ';' || c >= (int)sizeof(CdromId) - 1)
            {
                break;
            }
            if (isalnum(exename[i]))
            {
                CdromId[c++] = toupper(exename[i]);
            }
        }
    }

    Config.CdromId = toupper(CdromId[2]);
    if (Config.RegionAuto)
    {
        switch (Config.CdromId)
        {
          case 'I':
            Config.Region = PSX_TYPE_NTSC;
            SysPrintf("Game detected as NTSC (Japan)\n");
            break;

          case 'A':
            Config.Region = PSX_TYPE_NTSC;
            SysPrintf("Game detected as NTSC (America)\n");
            break;

          case 'E':
            Config.Region = PSX_TYPE_PAL;
            SysPrintf("Game detected as PAL (Europe)\n");
            break;
        }
    }

    if (CdromLabel[0] == ' ')
    {
        strncpy(CdromLabel, CdromId, 9);
    }
    SysPrintf("CD-ROM Label: %.32s\n", CdromLabel);
    SysPrintf("CD-ROM ID: %.9s\n", CdromId);
    SysPrintf("CD-ROM EXE Name: %.255s\n", exename);

    return true;
}
