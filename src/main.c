#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>

#include <SDL2/SDL.h>

#include "../include.h"
#include "sio.h"
#include "system.h"
#include "pad.h"
#include "cdriso.h"
#include "gpu.h"
#include "spu.h"

#define FULLSCREEN  winFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0

#define TITLE       "PCSX1"

typedef struct stat STATUS;

static SDL_Window           *sdlWindow;
static SDL_Rect             winViewport = {0, 0, 0, 0};
static SDL_Renderer         *sdlRenderer;
static SDL_Texture          *sdlTexture;
static SDL_Rect             texViewport = {0, 0, 1, 1};
static int                  texWidth, texHeight, texSizePending = 0;

static SDL_AudioDeviceID    sdlAudio;
static u16                  audioPlay = 0xffff;
static int                  iReadPos = 0;
static int                  vSync = 1;
static int                  aSamples = SAMPLERATE / 50;

static bool                 discInside = true;

static u32                  getMouseTimer = 0;

static int                  psxPower = 0;
static int                  psxPaused = 0;

static const int            mapJoyButton[16] =
{
    12, // triangle
    13, // circle
    14, // cross
    15, // square
    8,  // l2
    9,  // r2
    10, // l1
    11, // r1
    0,  // select
    3,  // start
    7,  // left
    5,  // right
    4,  // up
    6,  // down
    1,  // l3
    2   // r3
};

char                        *winTitle = TITLE;
int                         winW = 800, winH = 600; // a reasonable minimum
int                         winFullscreen = 0;
bool                        audioMute = false;

char                        *gameFile[5] = {"", "", "", "", ""};

bool                        getMouse = 0;

void (*PsxExecute)(void);

// help ------------------------------------------------------------------------
#define HELP(s, c)      for (i = 0; i < c; i++) { printf("    %-16s - %s\n", s[i].name, s[i].description); }

#define HELP_COUNT      7
#define KEY_COUNT       9

typedef struct
{
    char    *name;
    char    *description;
}
OPTION;

static OPTION           emuHelp[HELP_COUNT] =
{
    {"-disc FILE", "Load game image file (.toc)"},
    {"-cfg FILE", "Load configuration file(s)"},
    {"-psxout", "Enable PSX output"},
    {"-bios FILE", "Load SONY BIOS file"},
    {"-region $", "Force region: $ = NTSC/PAL"},
    {"-mcd $ FILE", "$ = create/load1/load2 memory card"},
    {"-mute", "Start with audio muted"}
};

static OPTION           fnKey[KEY_COUNT] =
{
    {"F2", "Toggle mouse capture"},
    {"F3", "Toggle audio mute/unmute"},
    {"F5", "Reset emulator"},
    {"F10", "Toggle widescreen"},
    {"F11", "Fullscreen/window mode"},
    {"Escape", "Power off"},
    {"Pause", "Pause emulator"},
    {"1-5", "Insert disc"},
    {"0", "Eject disc"}
};

void PowerOff()
{
    psxPower = 0;
}

void SysSetFrameRate(int hz)
{
    aSamples = SAMPLERATE * 200 / hz;
}

void SysWaitTime()
{
    do
    {
        SDL_Delay(1);
    }
    while (!vSync);
    vSync = !psxPower;

    if (!getMouse && SDL_GetTicks() > getMouseTimer)
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
}

void SignalExit(int sig)
{
    (void)sig;

    PowerOff();
}

void SetRenderer()
{
    SDL_RenderClear(sdlRenderer);
    SDL_RenderCopy(sdlRenderer, sdlTexture, &texViewport, &winViewport);
    SDL_RenderPresent(sdlRenderer);
}

void AdjustAspect()
{
    if (winH * 4 / 3 <= winW) // landscape
    {
        winViewport.h = PSXHEIGHT * winH / PSXHEIGHT;
        winViewport.w = winViewport.h * 4 / 3;
    }
    else // portrait
    {
        winViewport.w = PSXWIDTH * winW / PSXWIDTH;
        winViewport.h = winViewport.w * 3 / 4;
    }

    winViewport.x = (winW - winViewport.w) / 2;
    winViewport.y = (winH - winViewport.h) / 2;

    if (Config.Widescreen)
    {
        winViewport.w = winW;
        winViewport.x = 0;
    }

    SetRenderer();
}

void WindowSize(int w, int h)
{
    if (w == texViewport.w && h == texViewport.h)
    {
        return;
    }

    if (w == 0 || h == 0)
    {
        return;
    }

    texWidth = w;
    texHeight = h;

    texSizePending = 1;
}

void UpdateVideo()
{
    SDL_Surface     *surface;

    SDL_LockTextureToSurface(sdlTexture, NULL, &surface);

    if (texSizePending)
    {
        texSizePending = 0;
        texViewport.w = texWidth;
        texViewport.h = texHeight;

        SDL_FillRect(surface, NULL, 0);
    }

    DoBufferSwap((u32 *)surface->pixels);
    SDL_UnlockTexture(sdlTexture);

    SetRenderer();
}

void SdlCallback(void *unused, Uint8 *stream, int length)
{
    (void)unused;

    static int      samples = 0;
    short           *out = (short *)stream;

    while (length)
    {
        if (samples == 0)
        {
            samples = aSamples;
            vSync = 1;
        }
        samples--;

        *out = pSndBuffer[iReadPos] & audioPlay;
        if (iReadPos != iWritePos)
        {
            iReadPos++;

            iReadPos &= BUFFERSIZE;
        }

        out++;
        length -= sizeof(short);
    }
}

void WindowTitle(char *temp)
{
    char    *title;
    int     length;

    if (temp == NULL)
    {
        temp = winTitle;
    }

    length = 1;
    length += strlen(temp);

    length += strlen(CdromId);
    length += 3;

    title = malloc(length);
    sprintf(title, "%s%c(%s)", temp, discInside ? ' ' : '\0', CdromId);

    SDL_SetWindowTitle(sdlWindow, title);
    free(title);
}

void PsxPaused()
{
    SysUpdate();
    //SysWaitTime(100);
}

void DoPause()
{
    PsxExecute = PsxPaused;
    WindowTitle("PAUSED");
}

void DoStart()
{
    PsxExecute = PsxCpuExecute;
    WindowTitle(NULL);
}

void DiscEject()
{
    if (!discInside)
    {
        return; // already ejected
    }
    ISO_Stop();
    ISO_LidInterrupt();
    ISO_Close();
}

void ChangeDisc(int disc)
{
    STATUS  status;

    if (stat(gameFile[disc], &status) < 0)
    {
        return;
    }

    DiscEject();
    ISO_Open(gameFile[disc]);
    CheckCdrom();
    discInside = true;
    WindowTitle(NULL);
}

void CaptureMouse(int capture)
{
    SDL_SetRelativeMouseMode(capture ? SDL_TRUE : SDL_FALSE);
}

void SysUpdate()
{
    SDL_Event   event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
          case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
              case SDLK_1:
              case SDLK_2:
              case SDLK_3:
              case SDLK_4:
              case SDLK_5:
                ChangeDisc(event.key.keysym.sym - SDLK_1);
                break;

              case SDLK_0:
                DiscEject();
                discInside = false;
                WindowTitle(TITLE);
                break;

              case SDLK_F2:
                getMouse ^= 1;
                CaptureMouse(getMouse);
                break;

              case SDLK_F3:
                audioPlay ^= 0xffff;
                break;

              case SDLK_F5:
                texWidth = texHeight = 0;
                texSizePending = 1;
                texViewport.w = 0;
                texViewport.h = 0;
                SetRenderer();
                Psx_MemInit();
                PSX_Reset();
                break;

              case SDLK_F10:
                Config.Widescreen = !Config.Widescreen;
                AdjustAspect();
                break;

              case SDLK_F11:
                winFullscreen ^= 1;
                SDL_SetWindowFullscreen(sdlWindow, FULLSCREEN);
                break;

              case SDLK_PAUSE:
                psxPaused ^= 1;
                PsxExecute = psxPaused ? DoPause : DoStart;
                break;

              case SDLK_ESCAPE:
                PowerOff();
                break;
            }
            break;

          case SDL_JOYBUTTONDOWN:
          case SDL_JOYBUTTONUP:
            PAD_Button(event.jbutton.which, mapJoyButton[event.jbutton.button], event.jbutton.state);
            break;

          case SDL_JOYAXISMOTION:
            PAD_Axis(event.jaxis.which, event.jaxis.axis, event.jaxis.value);
            break;

          case SDL_MOUSEBUTTONDOWN:
          case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                PAD_Button(event.button.which, 11, event.button.state);
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                PAD_Button(event.button.which, 10, event.button.state);
            }
            break;

          case SDL_MOUSEMOTION:
            PAD_Motion(event.motion.which, event.motion.xrel, event.motion.yrel);
            if (!winFullscreen && !getMouse)
            {
                SDL_ShowCursor(SDL_TRUE);
                getMouseTimer = SDL_GetTicks() + 2000;
            }
            break;

          case SDL_WINDOWEVENT:
            switch (event.window.event)
            {
              case SDL_WINDOWEVENT_RESIZED:
                winW = event.window.data1;
                winH = event.window.data2;
                AdjustAspect();
                texSizePending = 1;
                break;

              case SDL_WINDOWEVENT_EXPOSED:
                SetRenderer();
                break;

              case SDL_WINDOWEVENT_FOCUS_LOST:
                CaptureMouse(0);
                PsxExecute = DoPause;
                break;

              case SDL_WINDOWEVENT_FOCUS_GAINED:
                CaptureMouse(getMouse);
                PsxExecute = DoStart;
                break;
            }
            break;

          case SDL_QUIT:
            PowerOff();
            break;
        }
    }
}

int main(int argc, char **argv)
{
    SDL_AudioSpec   want;
    int             help = 0;
    int             i;
    char            *build = TITLE" "VERSION;
    int             disc = 0;

    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-disc") && i + 1 < argc)
        {
            if (disc < 5)
            {
                gameFile[disc++] = argv[i + 1];
            }
            i += 1;
        }
        else if (!strcmp(argv[i], "-cfg") && i + 1 < argc)
        {
            Cfg_Load(argv[i + 1]);
            i += 1;
        }
        else if (!strcmp(argv[i], "-psxout"))
        {
            Config.PsxOut = true;
        }
        else if (!strcmp(argv[i], "-bios") && i + 1 < argc)
        {
            Config.Bios = argv[i + 1];
            i += 1;
            Config.SlowBoot = true;
        }
        else if (!strcmp(argv[i], "-mcd") && i + 2 < argc)
        {
            if (!strcmp(argv[i + 1], "create"))
            {
                MCD_Format(0);
                MCD_Save(0, argv[i + 2]);
                printf("Created memory card: %s\n", argv[i + 2]);
                return 0;
            }
            else if (!strcmp(argv[i + 1], "load1"))
            {
                Config.MemCard[0] = argv[i + 2];
            }
            else if (!strcmp(argv[i + 1], "load2"))
            {
                Config.MemCard[1] = argv[i + 2];
            }
            else
            {
                help = 1;
            }
            i += 2;
        }
        else if (!strcmp(argv[i], "-region") && i + 1 < argc)
        {
            i += 1;
            if (strcasecmp(argv[i], "ntsc") == 0)
            {
                Config.Region = PSX_TYPE_NTSC;
            }
            else if (strcasecmp(argv[i], "pal") == 0)
            {
                Config.Region = PSX_TYPE_PAL;
            }
            else
            {
                printf("Unknown Region specifier!\n");
                help = 1;
            }
            Config.RegionAuto = false;
        }
        else if (strcmp(argv[i], "-keys") == 0)
        {
            help = 2;
        }
        else if (!strcmp(argv[i], "-mute"))
        {
            audioMute = true;
        }
        else
        {
            help = 1;
        }
    }

    printf("Running %s\n", build);

    if (help == 2)
    {
        printf("  Emulator keys:\n");
        HELP(fnKey, KEY_COUNT);
        argc--;
    }

    if (argc == 1 || help == 1)
    {
        printf("  Command line options:\n");
        HELP(emuHelp, HELP_COUNT);
        return 0;
    }

    // this also sets bios region
    Psx_MemInit();

    MCD_Load(0, Config.MemCard[0]);
    MCD_Load(1, Config.MemCard[1]);

    if (ISO_Open(gameFile[0]) == 0)
    {
        CheckCdrom();

        if (Config.HLE && LoadCdrom() == false)
        {
            printf("Could not load CD-ROM!\n");
            return 1;
        }
    }
    else if (Config.HLE || Config.SlowBoot == false)
    {
        printf("No BIOS to run and no game file loadable!\n");
        return 1;
    }
    else
    {
        discInside = false;
    }

    PSX_Reset();

    signal(SIGINT, SignalExit);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

    SDL_JoystickEventState(SDL_ENABLE);
    if (getMouse)
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    sdlWindow = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, FULLSCREEN);
    AdjustAspect();

    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_ACCELERATED);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    // ABGR matches PSX video
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, PSXWIDTH, PSXHEIGHT);

    want.freq = SAMPLERATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = SdlCallback;

    audioPlay = audioMute ? 0x0000 : 0xffff;
    sdlAudio = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);

    SDL_PauseAudioDevice(sdlAudio, SDL_FALSE);

    SDL_JoystickOpen(0);
    SDL_JoystickOpen(1);

    PAD_Init(0);
    PAD_Init(1);

#ifdef ENABLE_NET
    NET_init();
    SIO1_init();
#endif

    SPU_Open();
    SPU_RegisterCallback(SPU_Irq);
    GPU_Open();

    // now unpause the audio,
    //  set the window title
    //  and start executing
    DoStart();

    psxPower = 1;

    while (psxPower)
    {
        PsxExecute();
    }

    MCD_Save(0, Config.MemCard[0]);
    MCD_Save(1, Config.MemCard[1]);

    psxBiosShutdown();

    ISO_Close();
    SPU_Close();
    GPU_Close();

#ifdef ENABLE_NET
    NET_close();
    NET_shutdown();
    SIO1_shutdown();
#endif

    SDL_CloseAudioDevice(sdlAudio);

    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(sdlWindow);

    SDL_Quit();

    SysPrintf("Goodbye.\n");

    return 0;
}
