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

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>

#include "system.h"

typedef struct stat STATUS;

typedef struct
{
    char    *name;
    char    *start;
    int     lines;
}
SECTION;

static char     *buffer = NULL;
static SECTION  *section = NULL;
static int      count = 0;

static char *SkipSpace(char *pos)
{
    while (*pos == ' ' || *pos == '\t')
    {
        pos++;
    }

    if (*pos == 0) // end of line
    {
        return NULL;
    }

    return pos;
}

static char *GetValuePos(char *pos, const char *key)
{
    int length;

    if ((pos = SkipSpace(pos)) == NULL)
    {
        return NULL;
    }

    if (*pos == '#')
    {
        return NULL;
    }

    length = strlen(key);

    if (strncasecmp(pos, key, length))
    {
        return NULL;
    }

    if ((pos = SkipSpace(pos + length)) == NULL)
    {
        return NULL;
    }

    if (*pos++ != '=')
    {
        return NULL;
    }

    if ((pos = SkipSpace(pos)) == NULL)
    {
        return NULL;
    }

    return pos;
}

static bool Cfg_Open(const char *filename)
{
    char    *posOld, *posNew;

    if (File_Load(filename, &buffer, NULL_SIZE) != 0)
    {
        return false;
    }

    posOld = buffer;
    count = 0;

    while ((posNew = strchr(posOld, '\n')))
    {
        *posNew = 0;
        if (*posOld == '[' && *(posNew - 1) == ']')
        {
            *(posNew - 1) = 0;
            count++;
            section = realloc(section, sizeof(SECTION) * count);
            section[count - 1].start = posNew + 1;
            section[count - 1].name = posOld + 1;
            section[count - 1].lines = 0;
        }
        else if (count)
        {
            section[count - 1].lines++;
        }

        posOld = posNew + 1;
    }
    if (count == 0)
    {
        return false;
    }

    return true;
}

static void Cfg_Close()
{
    if (section)
    {
        free(section);
        section = NULL;
    }

    if (buffer)
    {
        free(buffer);
        buffer = NULL;
    }
}

static char *GetValue(const char *cat, const char *key)
{
    int     i, j;
    char    *value, *pos;

    for (i = 0; i < count; i++)
    {
        if (strcasecmp(section[i].name, cat) == 0)
        {
            pos = section[i].start;
            for (j = 0; j < section[i].lines; j++)
            {
                if ((value = GetValuePos(pos, key)))
                {
                    return value;
                }

                pos += strlen(pos) + 1;
            }
            break;
        }
    }

    return NULL;
}

static void Cfg_GetString(const char *cat, const char *key, char **this)
{
    char    *value;

    if ((value = GetValue(cat, key)))
    {
        *this = strdup(value);
    }
}

static void Cfg_GetInteger(const char *cat, const char *key, int *this)
{
    char    *value, n[11];
    int     number;

    if ((value = GetValue(cat, key)) == NULL)
    {
        return;
    }

    number = atoi(value);
    sprintf(n, "%i", number);

    if (strcmp(value, n))
    {
        return;
    }

    *this = number;
}

static void Cfg_GetBoolean(const char *cat, const char *key, bool *this)
{
    char    *value;

    if ((value = GetValue(cat, key)) == NULL)
    {
        return;
    }

    if (strcasecmp(value, "False") == 0)
    {
        *this = false;
    }

    if (strcasecmp(value, "True") == 0)
    {
        *this = true;
    }
}

CONFIG  Config =
{
    .MemCard[0] = "",
    .MemCard[1] = "",
    .Region = PSX_TYPE_NTSC,
    .RegionAuto = true,

    .Widescreen = false,

    .HLE = true,
    .Bios = "",
    .SlowBoot = false,

    .SioIrq = false,
    .PsxOut = false,
    .SpuIrq = false,
    .RCntFix = false,
    .VSyncWA = false,
    .MemHack = false,
    .HackFix = false
};

extern char     *gameFile[5];
extern char     *winTitle;
extern int      winW, winH;
extern bool     winFullscreen;
extern int      iUseDither;         // 0 = off, 1 = game dependent, 2 = always
extern u32      dwCfgFixes;
extern int      padType[2];
extern bool     getMouse;
extern bool     audioMute;
extern int      iUseReverb;         // 0 = off, 1 = simple, 2 = playstation
extern int      iUseInterpolation;  // 0 = none, 1 = simple, 2 = gaussian, 3 = cubic

static bool     cfgOddEven = false;
static bool     cfgBrightness = false;
static bool     cfgCoord = false;
static bool     cfgTex = false;
static bool     cfgQuads = false;
static bool     cfgFake = false;

void Cfg_Load(const char *cfg)
{
    if (Cfg_Open(cfg) == false)
    {
        return;
    }

    Cfg_GetString("Game", "Bios", &Config.Bios);
    Cfg_GetBoolean("Game", "SlowBoot", &Config.SlowBoot);
    Cfg_GetString("Game", "Disc", &gameFile[0]);
    Cfg_GetString("Game", "Disc2", &gameFile[1]);
    Cfg_GetString("Game", "Disc3", &gameFile[2]);
    Cfg_GetString("Game", "Disc4", &gameFile[3]);
    Cfg_GetString("Game", "Disc5", &gameFile[4]);
    Cfg_GetString("Game", "MemCard1", &Config.MemCard[0]);
    Cfg_GetString("Game", "MemCard2", &Config.MemCard[1]);

    Cfg_GetInteger("Controls", "Pad1", &padType[0]);
    Cfg_GetInteger("Controls", "Pad2", &padType[1]);
    Cfg_GetBoolean("Controls", "CaptureMouse", &getMouse);

    Cfg_GetString("Video", "Title", &winTitle);
    Cfg_GetInteger("Video", "Width", &winW);
    Cfg_GetInteger("Video", "Height", &winH);
    Cfg_GetBoolean("Video", "Fullscreen", &winFullscreen);
    Cfg_GetBoolean("Video", "Widescreen", &Config.Widescreen);
    Cfg_GetInteger("Video", "Dithering", &iUseDither);

    Cfg_GetInteger("Audio", "Reverb", &iUseReverb);
    Cfg_GetInteger("Audio", "Interpolation", &iUseInterpolation);
    Cfg_GetBoolean("Audio", "Mute", &audioMute);

    Cfg_GetBoolean("PSX", "SioIrq", &Config.SioIrq);    // Wipeout
    Cfg_GetBoolean("PSX", "PsxOut", &Config.PsxOut);
    Cfg_GetBoolean("PSX", "SpuIrq", &Config.SpuIrq);
    Cfg_GetBoolean("PSX", "RCntFix", &Config.RCntFix);  // Parasite Eve 2, Vandal Hearts 1/2
    Cfg_GetBoolean("PSX", "VSyncWA", &Config.VSyncWA);  // InuYasha Sengoku Battle
    Cfg_GetBoolean("PSX", "MemHack", &Config.MemHack);  // Wipeout?
    Cfg_GetBoolean("PSX", "HackFix", &Config.HackFix);  // Raystorm/VH-D/MML/Cart World/...

    Cfg_GetBoolean("Fixes", "OddEvenBitHack", &cfgOddEven);
    Cfg_GetBoolean("Fixes", "IgnoreBrightnessColour", &cfgBrightness);
    Cfg_GetBoolean("Fixes", "DisableCoordCheck", &cfgCoord);
    Cfg_GetBoolean("Fixes", "RepeatedFlatTexTriangles", &cfgTex);
    Cfg_GetBoolean("Fixes", "DrawQuadsWithTriangles", &cfgQuads);
    Cfg_GetBoolean("Fixes", "FakeGpuBusyStates", &cfgFake);

    Cfg_Close();

    dwCfgFixes = 0;
    dwCfgFixes |= cfgOddEven ? 0x001 : 0;       // ChronoCross
    dwCfgFixes |= cfgBrightness ? 0x004 : 0;    // Lunar
    dwCfgFixes |= cfgCoord ? 0x008 : 0;         // _
    dwCfgFixes |= cfgTex ? 0x100 : 0;           // Dark Forces
    dwCfgFixes |= cfgQuads ? 0x200 : 0;         // better g-colors, worse textures (?)
    dwCfgFixes |= cfgFake ? 0x400 : 0;          // _
}

FILE    *discFile = NULL;

int File_DiscOpen(const char *filename)
{
    if ((discFile = fopen(filename, "rb")) != NULL)
    {
        return 0;
    }

    return 0;
}

void File_DiscClose()
{
    if (discFile == NULL)
    {
        return;
    }

    fclose(discFile);
    discFile = NULL;
}

void File_DiscSeek(u32 pos)
{
    if (discFile)
        fseek(discFile, pos, SEEK_SET);
}

int File_DiscRead(void *dest, size_t size)
{
    if (discFile)
        return fread(dest, 1, size, discFile) == size;
    else
        return 0;
}

int File_Load(const char *filename, char **buffer, int size[static 1])
{
    STATUS  status;
    FILE    *file;

    if (stat(filename, &status) < 0)
    {
        return 2;
    }

    if (*size > 0 && *size != status.st_size)
    {
        return 1;
    }

    if ((file = fopen(filename, "rb")) == NULL)
    {
        return 1;
    }

    if (*buffer == NULL)
    {
        *buffer = malloc(status.st_size);
    }
    fread(*buffer, 1, status.st_size, file);
    fclose(file);

    return 0;
}

void File_Save(const char *filename, const char *data, const int size)
{
    FILE    *file;

    if ((file = fopen(filename, "w")) == NULL)
    {
        return;
    }

    fwrite(data, 1, size, file);
    fclose(file);
}

void SysPrintf(const char *fmt, ...)
{
    va_list list;
    char    msg[512];

    va_start(list, fmt);
    vsprintf(msg, fmt, list);
    va_end(list);

    if (Config.PsxOut)
    {
        static char     *nl = " * ";

        printf("%s%s", nl, msg);

        if (strrchr(msg, '\n') != NULL)
        {
            nl = " * ";
        }
        else
        {
            nl = "";
        }
    }
}
