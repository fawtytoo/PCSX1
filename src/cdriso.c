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

#include "psxcommon.h"
#include "cdrom.h"
#include "system.h"

#include "cdriso.h"

bool    subChanRaw = false;
bool    subChanMissing = false;

u8      cdbuffer[CD_FRAMESIZE_RAW];
u8      subbuffer[SUB_FRAMESIZE];

bool    playing = false;
u32     cddaCurPos = 0;

int     cdOpenCase = 0; // for lid interrupt

/* Frame offset into CD image where pregap data would be found if it was there.
 * If a game seeks there we must *not* return subchannel data since it's
 * not in the CD image, so that cdrom code can fake subchannel data instead.
 * XXX: there could be multiple pregaps but PSX dumps only have one? */
u32     pregapOffset;

int (*CdRead)(u32, void *, int);

typedef struct
{
    enum
    {
        DATA = 1,
        CDDA
    }       type;
    char    start[3];        // MSF-format
    char    length[3];       // MSF-format
    u32     start_offset; // byte offset from start of above file
}
TRACKINFO;

// a CD can hold 99 tracks
// not sure if this applies to playstation discs
#define MAXTRACKS   100

int         numtracks = 0;
TRACKINFO   ti[MAXTRACKS];

// get a sector from a msf-array
u32 msf2sec(char *msf)
{
    return ((msf[0] * 60 + msf[1]) * 75) + msf[2];
}

void sec2msf(u32 s, char *msf)
{
    msf[0] = s / 75 / 60;
    s = s - msf[0] * 75 * 60;
    msf[1] = s / 75;
    s = s - msf[1] * 75;
    msf[2] = s;
}

// decode 'raw' subchannel data ripped by cdrdao
void DecodeRawSubData()
{
    u8  subQData[12];
    int i;

    memset(subQData, 0, sizeof(subQData));

    for (i = 0; i < 8 * 12; i++)
    {
        if (subbuffer[i] & (1 << 6))
        { // only subchannel Q is needed
            subQData[i >> 3] |= (1 << (7 - (i & 7)));
        }
    }

    memcpy(&subbuffer[12], subQData, 12);
}

int CdReadNormal(u32 base, void *dest, int sector)
{
    File_DiscSeek(base + sector * CD_FRAMESIZE_RAW);
    return File_DiscRead(dest, CD_FRAMESIZE_RAW);
}

int CdReadSubMixed(u32 base, void *dest, int sector)
{
    int     error;

    File_DiscSeek(base + sector * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE));
    error = File_DiscRead(dest, CD_FRAMESIZE_RAW);
    File_DiscRead(subbuffer, SUB_FRAMESIZE);

    if (subChanRaw)
    {
        DecodeRawSubData();
    }

    return error;
}

// generic sscanf
void Sscanf(char *input, char *msf)
{
    int time[3] = {0, 0, 0};

    sscanf(input, "%d:%d:%d", &time[0], &time[1], &time[2]);
    msf[0] = time[0];
    msf[1] = time[1];
    msf[2] = time[2];
}

// This function is invoked by the front-end when opening an ISO
// file for playback
// the necessary data is put into the ti (trackinformation)-array
int ISO_Open(char *toc)
{
    int     error;
    char    *buffer = NULL, *posOld, *posNew;
    int     length;
    char    filename[PATH_MAX], name[256], *tmp, new[9], new2[9];
    char    time[3];
    u32     t, sector_offs, sector_size;
    u32     current_zero_gap = 0;
    int     i;

    subChanRaw = false;
    pregapOffset = 0;
    CdRead = CdReadNormal;

    if ((error = File_Load(toc, &buffer, NULL_SIZE)) == 1)
    {
        printf("Error opening game image:\n\t%s\n", toc);
        return 1;
    }
    else if (error > 0)
    {
        return 1;
    }

    strcpy(filename, toc);
    if ((tmp = strrchr(filename, '/')))
    {
        *(tmp + 1) = 0;
    }
    else
    {
        *filename = 0;
    }

    posOld = buffer;
    numtracks = 0;

    //memset(&ti, 0, sizeof(ti));

    sector_size = CD_FRAMESIZE_RAW;
    sector_offs = 2 * 75;

    while ((posNew = strchr(posOld, '\n')))
    {
        *posNew = 0;
        length = strlen(posOld);

        if (length >= 5 && !strncmp(posOld, "TRACK", 5))
        {
            sector_offs += current_zero_gap;
            current_zero_gap = 0;

            numtracks++;

            sscanf(posOld, "TRACK %9s", name);
            if (!strncmp(name, "MODE2_RAW", 9))
            {
                ti[numtracks].type = DATA;
                sec2msf(2 * 75, ti[numtracks].start); // assume data track on 0:2:0

                sscanf(posOld, "TRACK MODE2_RAW %6s", name);
                if (!strncmp(name, "RW", 2))
                {
                    sector_size = CD_FRAMESIZE_RAW + SUB_FRAMESIZE;
                    CdRead = CdReadSubMixed;
                    if (!strncmp(name, "RW_RAW", 6))
                    {
                        subChanRaw = true;
                    }
                }
            }
            else if (!strncmp(name, "AUDIO", 5))
            {
                ti[numtracks].type = CDDA;
            }
        }
        else if (length >= 8 && !strncmp(posOld, "DATAFILE", 8))
        {
            if (ti[numtracks].type == CDDA)
            {
                sscanf(posOld, "DATAFILE \"%255[^\"]\" #%u %8s", name, &t, new);
                ti[numtracks].start_offset = t;
                t = t / sector_size + sector_offs;
                sec2msf(t, ti[numtracks].start);
                Sscanf(new, ti[numtracks].length);
            }
            else
            {
                sscanf(posOld, "DATAFILE \"%255[^\"]\" %8s", name, new);
                Sscanf(new, ti[numtracks].length);
                strcat(filename, name);
            }
        }
        else if (length >= 4 && !strncmp(posOld, "FILE", 4))
        {
            sscanf(posOld, "FILE \"%255[^\"]\" #%u %8s %8s", name, &t, new, new2);
            Sscanf(new, ti[numtracks].start);
            t += msf2sec(ti[numtracks].start) * sector_size;
            ti[numtracks].start_offset = t;
            t = t / sector_size + sector_offs;
            sec2msf(t, ti[numtracks].start);
            Sscanf(new2, ti[numtracks].length);
        }
        else if ((length >= 4 && !strncmp(posOld, "ZERO", 4)) || (length >= 7 && !strncmp(posOld, "SILENCE", 7)))
        {
            // skip unneeded optional fields
            tmp = strchr(posOld, ':');
            Sscanf(tmp - 2, time);
            current_zero_gap = msf2sec(time);

            if (numtracks > 1)
            {
                t = ti[numtracks - 1].start_offset;
                t /= sector_size;
                pregapOffset = t + msf2sec(ti[numtracks - 1].length);
            }
        }
        else if (length >= 5 && !strncmp(posOld, "START", 5))
        {
            sscanf(posOld, "START %8s", new);
            Sscanf(new, time);
            t = msf2sec(time);
            ti[numtracks].start_offset += (t - current_zero_gap) * sector_size;
            t = msf2sec(ti[numtracks].start) + t;
            sec2msf(t, ti[numtracks].start);
        }

        posOld = posNew + 1;
    }

    free(buffer);

    if (numtracks == 0)
    {
        printf("No tracks found in game image:\n\t%s\n", toc);
        return 1;
    }

    if (File_DiscOpen(filename) != 0)
    {
        printf("Error opening game image:\n\t%s\n", filename);
        return 1;
    }

    SysPrintf("Loaded game image: %s\n", filename);

    for (i = 1; i <= numtracks; i++)
    {
        SysPrintf("Track %.2d (%s)", i, (ti[i].type == DATA ? "DATA" : "CDDA"));
        SysPrintf(" - Start %.2d:%.2d:%.2d", ti[i].start[0], ti[i].start[1], ti[i].start[2]);
        SysPrintf(", Length %.2d:%.2d:%.2d\n", ti[i].length[0], ti[i].length[1], ti[i].length[2]);
    }

    if (subChanRaw == true)
    {
        SysPrintf("(Includes sub channel data)\n");
    }

    return 0;
}

void ISO_Close()
{
    File_DiscClose();

    numtracks = 0;
    ti[1].type = 0;

    memset(cdbuffer, 0, sizeof(cdbuffer));
}

// return Starting and Ending Track
// buffer:
//  byte 0 - start track
//  byte 1 - end track
void ISO_GetTN(u8 *buffer)
{
    buffer[0] = 1;

    if (numtracks > 0)
    {
        buffer[1] = numtracks;
    }
    else
    {
        buffer[1] = 1;
    }
}

// return Track Time
// buffer:
//  byte 0 - frame
//  byte 1 - second
//  byte 2 - minute
void ISO_GetTD(u8 track, char *buffer)
{
    if (track == 0)
    {
        u32     sect;
        char    time[3];

        sect = msf2sec(ti[numtracks].start) + msf2sec(ti[numtracks].length);
        sec2msf(sect, time);
        buffer[2] = time[0];
        buffer[1] = time[1];
        buffer[0] = time[2];
    }
    else if (numtracks > 0 && track <= numtracks)
    {
        buffer[2] = ti[track].start[0];
        buffer[1] = ti[track].start[1];
        buffer[0] = ti[track].start[2];
    }
    else
    {
        buffer[2] = 0;
        buffer[1] = 2;
        buffer[0] = 0;
    }
}

// read track
// time: byte 0 - minute; byte 1 - second; byte 2 - frame
// uses bcd format
int ISO_ReadTrack(u8 *time)
{
    u32     sector = MSF2SECT(btoi(time[0]), btoi(time[1]), btoi(time[2]));

    if (pregapOffset)
    {
        subChanMissing = false;
        if (sector >= pregapOffset)
        {
            sector -= 2 * 75;
            if (sector < pregapOffset)
            {
                subChanMissing = true;
            }
        }
    }

    return CdRead(0, cdbuffer, sector);
}

// plays cdda audio
// sector: byte 0 - minute; byte 1 - second; byte 2 - frame
// does NOT uses bcd format
void ISO_Play(char *time)
{
    (void)time; // FIXME

    playing = true;
}

// stops cdda audio
void ISO_Stop()
{
    playing = false;
}

u8 *ISO_GetBuffer()
{
    return cdbuffer + 12;
}

// gets subchannel data
u8 *ISO_GetBufferSub()
{
    if (subChanRaw && !subChanMissing)
    {
        return subbuffer;
    }

    return NULL;
}

long ISO_GetStatus(CDRSTAT *stat)
{
    u32 sect;

    if (cdOpenCase)
    {
        cdOpenCase = 0;
        stat->Status |= 0x10; // STATUS_SHELLOPEN
    }
    else
    {
        stat->Status &= ~0x10;
    }

    if (playing)
    {
        stat->Type = 0x02;
        stat->Status |= 0x80;
    }
    else
    {
        // BIOS - boot ID (CD type)
        stat->Type = ti[1].type;
    }

    // relative -> absolute time
    sect = cddaCurPos;
    sec2msf(sect, stat->Time);

    return 0;
}

// read CDDA sector into buffer
void ISO_ReadCDDA(u8 m, u8 s, u8 f, u8 *buffer)
{
    char    msf[3] = {m, s, f};
    u32     track, track_start = 0;

    cddaCurPos = msf2sec(msf);

    // find current track index
    for (track = numtracks; ; track--)
    {
        track_start = msf2sec(ti[track].start);
        if (track_start <= cddaCurPos)
        {
            break;
        }
        if (track == 1)
        {
            break;
        }
    }

    // data tracks play silent (or CDDA set to silent)
    if (ti[track].type != CDDA)
    {
        memset(buffer, 0, CD_FRAMESIZE_RAW);
        return;
    }

    if (CdRead(ti[track].start_offset, buffer, cddaCurPos - track_start) == 0)
    {
        memset(buffer, 0, CD_FRAMESIZE_RAW);
    }
}

void ISO_LidInterrupt()
{
    cdOpenCase = 1;
    CDR_LidInterrupt();
}
