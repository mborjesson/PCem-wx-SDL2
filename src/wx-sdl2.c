#include "wx-sdl2.h"

#include <SDL2/SDL.h>
#include <wx/defs.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "ibm.h"
#include "cdrom-ioctl.h"
#include "cdrom-iso.h"
#include "config.h"
#include "video.h"
#include "cpu.h"
#include "ide.h"
#include "model.h"
#include "mouse.h"
#include "nvr.h"
#include "sound.h"
#include "thread.h"
#include "disc.h"
#include "disc_img.h"

#include "wx-sdl2-video.h"
#include "wx-utils.h"
#include "wx-common.h"
#include "wx-display.h"

#define ID_IS(s) wParam == wx_xrcid(s)
#define ID_RANGE(a, b) wParam >= wx_xrcid(a) && wParam <= wx_xrcid(b)

static int save_window_pos = 0;
uint64_t timer_freq;


int gfx_present[GFX_MAX];

SDL_mutex* ghMutex;
SDL_mutex* mainMutex;
SDL_cond* mainCond;

SDL_Thread* mainthreadh = NULL;

SDL_TimerID onesectimer;

int running = 0;

int drawits = 0;

int romspresent[ROM_MAX];
int quited = 0;

SDL_Rect oldclip;

void* ghwnd = 0;

void* menu;

emulation_state_t emulation_state = EMULATION_STOPPED;
int pause = 0;
int old_cdrom_drive = 0;

int window_doreset = 0;
int renderer_doreset = 0;
int window_dofullscreen = 0;
int window_dowindowed = 0;
int window_doremember = 0;
int window_doinputgrab = 0;
int window_doinputrelease = 0;
int window_dotogglefullscreen = 0;

int video_scale = 1;

char menuitem[60];

void warning(const char *format, ...)
{
        char buf[1024];
        va_list ap;

        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);

        wx_messagebox(ghwnd, buf, "PCem", WX_MB_OK);
}

void updatewindowsize(int x, int y)
{
        int winsizex = 640, winsizey = 480;
        switch(video_scale)
        {
                case 0:
                        winsizex = x >> 1;
                        winsizey = y >> 1;
                        break;
                case 2:
                        winsizex = (x * 3) >> 1;
                        winsizey = (y * 3) >> 1;
                        break;
                case 3:
                        winsizex = x << 1;
                        winsizey = y << 1;
                        break;
                case 1:
                default:
                        winsizex = x;
                        winsizey = y;
                        break;
        }

        SDL_Rect rect;
        rect.x = rect.y = 0;
        rect.w = winsizex;
        rect.h = winsizey;
        sdl_scale(video_fullscreen_scale, rect, &rect, winsizex, winsizey);
        winsizex = rect.w;
        winsizey = rect.h;

        display_resize(winsizex, winsizey);
}

void startblit()
{
        SDL_LockMutex(ghMutex);
}

void endblit()
{
        SDL_UnlockMutex(ghMutex);
}

void enter_fullscreen()
{
        window_dofullscreen = window_doinputgrab = 1;
}

void leave_fullscreen()
{
        window_dowindowed = window_doinputrelease = 1;
}

void toggle_fullscreen()
{
        window_dotogglefullscreen = 1;
}

uint64_t main_time;

int mainthread(void* param)
{
        int i;

        SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

        int t = 0;
        int frames = 0;
        uint32_t old_time, new_time;

//        Sleep(500);
        drawits = 0;
        old_time = SDL_GetTicks();
        running = 1;
        while (running)
        {
                new_time = SDL_GetTicks();
                drawits += new_time - old_time;
                old_time = new_time;

//                printf("%f\n", main_time);

                if (drawits > 0 && !pause)
                {
                        uint64_t start_time = timer_read();
                        uint64_t end_time;
                        drawits -= 10;
                        if (drawits > 50)
                                drawits = 0;
                        runpc();
                        frames++;
                        if (frames >= 200 && nvr_dosave)
                        {
                                frames = 0;
                                nvr_dosave = 0;
                                savenvr();
                        }
                        end_time = timer_read();
                        main_time += end_time - start_time;
                }
                else
                        SDL_Delay(1);
        }

        SDL_LockMutex(mainMutex);
        SDL_CondSignal(mainCond);
        SDL_UnlockMutex(mainMutex);

        return TRUE;
}

void get_executable_name(char *s, int size)
{
        char* path = SDL_GetBasePath();
        strcpy(s, path);
}

void set_window_title(char *s)
{
//        wx_setwindowtitle(ghwnd, s);
}

uint64_t timer_read()
{
        return SDL_GetPerformanceCounter();
}

Uint32 timer_onesec(Uint32 interval, void* param)
{
        onesec();

        return interval;
}

void update_toolbar(void* toolbar)
{
        wx_enabletoolbaritem(toolbar, WX_ID("TOOLBAR_RUN"), emulation_state != EMULATION_RUNNING);
        wx_enabletoolbaritem(toolbar, WX_ID("TOOLBAR_PAUSE"), emulation_state == EMULATION_RUNNING);
        wx_enabletoolbaritem(toolbar, WX_ID("TOOLBAR_STOP"), emulation_state != EMULATION_STOPPED);
}

void sdl_loadconfig()
{
        video_fullscreen = config_get_int("SDL2", "fullscreen", video_fullscreen);
        video_fullscreen_mode = config_get_int("SDL2", "fullscreen_mode", video_fullscreen_mode);
        video_scale = config_get_int("SDL2", "scale", video_scale);
        video_scale_mode = config_get_int("SDL2", "scale_mode", video_scale_mode);
        video_vsync = config_get_int("SDL2", "vsync", video_vsync);
        video_focus_dim = config_get_int("SDL2", "focus_dim", video_focus_dim);
        requested_render_driver = sdl_get_render_driver_by_name(config_get_string("SDL2", "render_driver", ""), RENDERER_AUTO);
}

void sdl_saveconfig()
{
        config_set_int("SDL2", "fullscreen", video_fullscreen);
        config_set_int("SDL2", "fullscreen_mode", video_fullscreen_mode);
        config_set_int("SDL2", "scale", video_scale);
        config_set_int("SDL2", "scale_mode", video_scale_mode);
        config_set_int("SDL2", "vsync", video_vsync);
        config_set_int("SDL2", "focus_dim", video_focus_dim);
        config_set_string("SDL2", "render_driver", (char*)requested_render_driver.sdl_id);
}

void wx_setupitems()
{
        int c;
        update_toolbar(wx_gettoolbar(ghwnd));
        menu = wx_getmenu(ghwnd);

        if (!cdrom_enabled)
                wx_checkmenuitem(menu, WX_ID("IDM_CDROM_DISABLED"), WX_MB_CHECKED);
        else
        {
                if (cdrom_drive == CDROM_ISO)
                        wx_checkmenuitem(menu, WX_ID("IDM_CDROM_ISO"), WX_MB_CHECKED);
                else
                {
                        wx_checkmenuitem(menu, WX_ID("IDM_CDROM_EMPTY"), WX_MB_CHECKED);
//                       sprintf(menuitem, "IDM_CDROM_REAL[%d]", cdrom_drive);
//                        wx_checkmenuitem(menu, ID(menuitem), MF_CHECKED);
                }
        }
        if (vid_resize)
                wx_checkmenuitem(menu, WX_ID("IDM_VID_RESIZE"), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_FS[%d]", video_fullscreen_scale);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_FULLSCREEN"), video_fullscreen);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_REMEMBER"),
                        window_remember ? WX_MB_CHECKED : WX_MB_UNCHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_BPB_DISABLE"), bpb_disable ? WX_MB_CHECKED : WX_MB_UNCHECKED);

        // wx-sdl2 specific
        wx_checkmenuitem(menu, WX_ID("IDM_STATUS"), show_status);
        wx_checkmenuitem(menu, WX_ID("IDM_SPEED_HISTORY"), show_speed_history);
        wx_checkmenuitem(menu, WX_ID("IDM_DISC_ACTIVITY"), show_disc_activity);
        wx_checkmenuitem(menu, WX_ID("IDM_MACHINE_MOUNT_PATHS"), show_mount_paths);
        sprintf(menuitem, "IDM_VID_SCALE_MODE[%d]", video_scale_mode);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_SCALE[%d]", video_scale);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_FS_MODE[%d]", video_fullscreen_mode);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_VSYNC"), video_vsync);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_LOST_FOCUS_DIM"), video_focus_dim);

        int num_renderers;
        sdl_render_driver* drivers = sdl_get_render_drivers(&num_renderers);
        for (c = 1; c < num_renderers; ++c)
        {
                sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", drivers[c].id);
                wx_enablemenuitem(menu, WX_ID(menuitem), 0);
        }
        SDL_RendererInfo renderInfo;
        for (c = 0; c < SDL_GetNumRenderDrivers(); ++c)
        {
                SDL_GetRenderDriverInfo(c, &renderInfo);
                sdl_render_driver* driver = sdl_get_render_driver_by_name_ptr(renderInfo.name);

                if (driver)
                {
                        pclog("Renderer: %s (%d)\n", renderInfo.name, driver->id);
                        sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", driver->id);
                        wx_enablemenuitem(menu, WX_ID(menuitem), 1);
                }
        }
        sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", requested_render_driver.id);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
}

void sdl_onconfigloaded()
{
        if (ghwnd)
                wx_callback(ghwnd, wx_setupitems);
}

extern void wx_loadconfig();
extern void wx_saveconfig();

int pc_main(int argc, char** argv)
{
#ifndef __APPLE__
        display_init();
#endif
        sdl_video_init();

        add_config_callback(sdl_loadconfig, sdl_saveconfig, sdl_onconfigloaded);
        add_config_callback(wx_loadconfig, wx_saveconfig, 0);

        initpc(argc, argv);
        resetpchard();

        return TRUE;
}

int wx_start(void* hwnd)
{
        int c, d;
        ghwnd = hwnd;

#ifdef __APPLE__
        /* OSX requires SDL to be initialized after wxWidgets. */
        display_init();
#endif

        readflash = 0;

        wx_setupitems();

        d = romset;
        for (c = 0; c < ROM_MAX; c++)
        {
                romset = c;
                romspresent[c] = loadbios();
                pclog("romset %i - %i\n", c, romspresent[c]);
        }

        for (c = 0; c < ROM_MAX; c++)
        {
                if (romspresent[c])
                        break;
        }
        if (c == ROM_MAX)
        {
                wx_messagebox(hwnd,
                                "No ROMs present!\nYou must have at least one romset to use PCem.",
                                "PCem fatal error", WX_MB_OK);
                return 0;
        }

        romset = d;
        c = loadbios();

        if (!c)
        {
                if (romset != -1)
                        wx_messagebox(hwnd,
                                        "Configured romset not available.\nDefaulting to available romset.",
                                        "PCem error", WX_MB_OK);
                for (c = 0; c < ROM_MAX; c++)
                {
                        if (romspresent[c])
                        {
                                romset = c;
                                model = model_getmodel(romset);
                                saveconfig();
                                resetpchard();
                                break;
                        }
                }
        }

        for (c = 0; c < GFX_MAX; c++)
                gfx_present[c] = video_card_available(video_old_to_new(c));

        if (!video_card_available(video_old_to_new(gfxcard)))
        {
                if (romset != -1)
                        wx_messagebox(hwnd,
                                        "Configured video BIOS not available.\nDefaulting to available romset.",
                                        "PCem error", WX_MB_OK);
                for (c = GFX_MAX - 1; c >= 0; c--)
                {
                        if (gfx_present[c])
                        {
                                gfxcard = c;
                                saveconfig();
                                resetpchard();
                                break;
                        }
                }
        }
}

int start_emulation(void* params)
{
        if (emulation_state == EMULATION_PAUSED)
        {
                emulation_state = EMULATION_RUNNING;
                pause = 0;
                return TRUE;
        }
        pclog("Starting emulation...\n");
        emulation_state = EMULATION_RUNNING;
        pause = 0;

        ghMutex = SDL_CreateMutex();
        mainMutex = SDL_CreateMutex();
        mainCond = SDL_CreateCond();

        loadbios();
        resetpchard();

        display_start(params);
        mainthreadh = SDL_CreateThread(mainthread, "Main Thread", NULL);

        onesectimer = SDL_AddTimer(1000, timer_onesec, NULL);

        updatewindowsize(640, 480);

        timer_freq = timer_read();

        return TRUE;
}

int pause_emulation()
{
        pclog("Emulation paused.\n");
        emulation_state = EMULATION_PAUSED;
        pause = 1;
        return TRUE;
}

int stop_emulation()
{
        emulation_state = EMULATION_STOPPED;
        pclog("Stopping emulation...\n");
        SDL_LockMutex(mainMutex);
        running = 0;
        SDL_CondWaitTimeout(mainCond, mainMutex, 10 * 1000);
        SDL_UnlockMutex(mainMutex);

        SDL_DestroyCond(mainCond);
        SDL_DestroyMutex(mainMutex);

        startblit();
        display_stop();

        SDL_DetachThread(mainthreadh);
        mainthreadh = NULL;
        SDL_RemoveTimer(onesectimer);
        savenvr();
        saveconfig();

        endblit();
        SDL_DestroyMutex(ghMutex);

        pclog("Emulation stopped.\n");

        wx_showwindow(ghwnd, 1);

        return TRUE;
}

int stop_emulation_confirm()
{
        if (emulation_state != EMULATION_STOPPED)
                if (wx_messagebox(NULL, "Stop emulation?", "PCem", WX_MB_OKCANCEL) == WX_IDOK)
                        return stop_emulation();
                else
                        return FALSE;
        return TRUE;
}


int wx_stop()
{
        if (!stop_emulation_confirm())
                return FALSE;
        pclog("Shutting down...\n");
        saveconfig();
        closepc();
        display_close();
        sdl_video_close();

        printf("Shut down successfully!\n");
        return TRUE;
}

char openfilestring[260];
int getfile(void* hwnd, char *f, char *fn)
{
        int ret = wx_filedialog(hwnd, "Open", fn, f, 1, openfilestring);
#ifdef __APPLE__
        /* wxWidgets on OSX may mess up the SDL-window somehow, so just in case we reset it here */
        window_doreset = 1;
#endif
        return ret;
}

int getsfile(void* hwnd, char *f, char *fn)
{
        int ret = wx_filedialog(hwnd, "Save", fn, f, 0, openfilestring);
#ifdef __APPLE__
        window_doreset = 1;
#endif
        return ret;
}

void atapi_close(void)
{
        switch (cdrom_drive)
        {
        case CDROM_ISO:
                iso_close();
                break;
        default:
                ioctl_close();
                break;
        }
}

int confirm()
{
        if (emulation_state != EMULATION_STOPPED) {
                return wx_messagebox(NULL, "This will reset PCem!\nOkay to continue?",
                                "PCem", WX_MB_OKCANCEL) == WX_IDOK;
        }
        return 1;
}

int wx_handle_command(void* hwnd, int wParam, int checked)
{
        SDL_Rect rect;
        void* hmenu;
        void* toolbar;
        char temp_iso_path[1024];
        int new_cdrom_drive;
        hmenu = wx_getmenu(hwnd);
        toolbar = wx_gettoolbar(hwnd);
        if (ID_IS("TOOLBAR_RUN"))
        {
                start_emulation(hwnd);
                update_toolbar(toolbar);
        }
        else if (ID_IS("TOOLBAR_PAUSE"))
        {
                pause_emulation();
                update_toolbar(toolbar);
        }
        else if (ID_IS("TOOLBAR_STOP"))
        {
                stop_emulation_confirm();
                update_toolbar(toolbar);
        }
        else if (ID_IS("IDM_FILE_RESET"))
        {
                pause = 1;
                SDL_Delay(100);
                savenvr();
                resetpc();
                pause = 0;
        }
        else if (ID_IS("IDM_FILE_HRESET"))
        {
                pause = 1;
                SDL_Delay(100);
                savenvr();
                resetpchard();
                pause = 0;
        }
        else if (ID_IS("IDM_FILE_RESET_CAD"))
        {
                pause = 1;
                SDL_Delay(100);
                savenvr();
                resetpc_cad();
                pause = 0;
        }
        else if (ID_IS("IDM_FILE_EXIT"))
        {
                wx_exit(hwnd, 0);
        }
        else if (ID_IS("IDM_DISC_A"))
        {
                if (!getfile(hwnd,
                                "Disc image (*.img;*.ima;*.fdi)|*.img;*.ima;*.fdi|All files (*.*)|*.*",
                                discfns[0]))
                {
                        disc_close(0);
                        disc_load(0, openfilestring);
                        saveconfig();
                }
        }
        else if (ID_IS("IDM_DISC_B"))
        {
                if (!getfile(hwnd,
                                "Disc image (*.img;*.ima;*.fdi)|*.img;*.ima;*.fdi|All files (*.*)|*.*",
                                discfns[1]))
                {
                        disc_close(1);
                        disc_load(1, openfilestring);
                        saveconfig();
                }
        }
        else if (ID_IS("IDM_EJECT_A"))
        {
                disc_close(0);
                saveconfig();
        }
        else if (ID_IS("IDM_EJECT_B"))
        {
                disc_close(1);
                saveconfig();
        }
        else if (ID_IS("IDM_BPB_DISABLE"))
        {
                bpb_disable = checked;
                wx_checkmenuitem(hmenu, WX_ID("IDM_BPB_DISABLE"), bpb_disable ? WX_MB_CHECKED : WX_MB_UNCHECKED);
                saveconfig();
        }
        else if (ID_IS("IDM_HDCONF"))
        {
                hdconf_open(hwnd);
        }
        else if (ID_IS("IDM_CONFIG"))
        {
                config_open(hwnd);
        }
        else if (ID_IS("IDM_STATUS"))
        {
                show_status = checked;
                saveconfig();
        }
        else if (ID_IS("IDM_SPEED_HISTORY"))
        {
                show_speed_history = checked;
                saveconfig();
        }
        else if (ID_IS("IDM_DISC_ACTIVITY"))
        {
                show_disc_activity = checked;
                saveconfig();
        }
        else if (ID_IS("IDM_MACHINE_MOUNT_PATHS"))
        {
                show_mount_paths = checked;
                saveconfig();
        }
        else if (ID_IS("IDM_MACHINE_TOGGLE"))
        {
                if (emulation_state != EMULATION_STOPPED)
                        wx_togglewindow(hwnd);
        }
        else if (ID_IS("IDM_VID_RESIZE"))
        {
                vid_resize = checked;
                window_doreset = 1;
                saveconfig();
        }
        else if (ID_IS("IDM_VID_REMEMBER"))
        {
                window_remember = checked;
                wx_checkmenuitem(hmenu, WX_ID("IDM_VID_REMEMBER"),
                                window_remember ? WX_MB_CHECKED : WX_MB_UNCHECKED);
                window_doremember = 1;
                saveconfig();
        }
        else if (ID_IS("IDM_VID_FULLSCREEN"))
        {
                if (video_fullscreen_first)
                {
                        video_fullscreen_first = 0;
                        wx_messagebox(hwnd,
                                        "Use CTRL + ALT + PAGE DOWN to return to windowed mode",
                                        "PCem", WX_MB_OK);
                }
                video_fullscreen = checked;
                saveconfig();
        }
        else if (ID_IS("IDM_VID_FULLSCREEN_TOGGLE"))
        {
                toggle_fullscreen();
        }
        else if (ID_RANGE("IDM_VID_FS[start]", "IDM_VID_FS[end]"))
        {
                video_fullscreen_scale = wParam - wx_xrcid("IDM_VID_FS[start]");
                saveconfig();
        }
        else if (ID_RANGE("IDM_VID_SCALE_MODE[start]", "IDM_VID_SCALE_MODE[end]"))
        {
                video_scale_mode = wParam - wx_xrcid("IDM_VID_SCALE_MODE[start]");
                renderer_doreset = 1;
                saveconfig();
        }
        else if (ID_RANGE("IDM_VID_SCALE[start]", "IDM_VID_SCALE[end]"))
        {
                video_scale = wParam - wx_xrcid("IDM_VID_SCALE[start]");
                saveconfig();
        }
        else if (ID_RANGE("IDM_VID_FS_MODE[start]", "IDM_VID_FS_MODE[end]"))
        {
                video_fullscreen_mode = wParam - wx_xrcid("IDM_VID_FS_MODE[start]");
                saveconfig();
        }
        else if (ID_RANGE("IDM_VID_RENDER_DRIVER[start]", "IDM_VID_RENDER_DRIVER[end]"))
        {
                requested_render_driver = sdl_get_render_driver_by_id(wParam - wx_xrcid("IDM_VID_RENDER_DRIVER[start]"), RENDERER_AUTO);
                window_doreset = 1;
                saveconfig();
        }
        else if (ID_IS("IDM_VID_VSYNC"))
        {
                video_vsync = checked;
                renderer_doreset = 1;
                saveconfig();
        }
        else if (ID_IS("IDM_VID_LOST_FOCUS_DIM"))
        {
                video_focus_dim = checked;
                saveconfig();
        }
        else if (ID_IS("IDM_CONFIG_LOAD"))
        {
                pause = 1;
                if (!getfile(hwnd,
                                "Configuration (*.cfg)|*.cfg|All files (*.*)|*.*",
                                ""))
                {
                        if (confirm())
                        {
                                loadconfig(openfilestring);
                                config_save(config_file_default);
                                mem_resize();
                                loadbios();
                                resetpchard();
                        }
                }
                pause = 0;
        }
        else if (ID_IS("IDM_CONFIG_SAVE"))
        {
                pause = 1;
                if (!getsfile(hwnd,
                                "Configuration (*.cfg)|*.cfg|All files (*.*)|*.*",
                                ""))
                        config_save(openfilestring);
                pause = 0;
        }
        else if (ID_IS("IDM_CDROM_DISABLED"))
        {
                if (cdrom_enabled)
                {
                        if (!confirm())
                                return 0;
                }
                if (!cdrom_enabled)
                {
                        /* Switching from disabled to disabled. Do nothing. */
                        return 0;
                }
                atapi->exit();
                atapi_close();
                ioctl_set_drive(0);
                //                        wx_checkmenuitem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_UNCHECKED);
                wx_checkmenuitem(hmenu, WX_ID("IDM_CDROM_DISABLED"), WX_MB_CHECKED);
                //                        wx_checkmenuitem(hmenu, IDM_CDROM_ISO,                MF_UNCHECKED);
                old_cdrom_drive = cdrom_drive;
                cdrom_drive = 0;
                //                        wx_checkmenuitem(hmenu, IDM_CDROM_EMPTY,              MF_UNCHECKED);
                if (cdrom_enabled)
                {
                        pause = 1;
                        SDL_Delay(100);
                        cdrom_enabled = 0;
                        saveconfig();
                        resetpchard();
                        pause = 0;
                }
        }
        else if (ID_IS("IDM_CDROM_EMPTY"))
        {
                if (!cdrom_enabled)
                {
                        if (!confirm())
                                return 0;
                }
                if ((cdrom_drive == 0) && cdrom_enabled)
                {
                        /* Switch from empty to empty. Do nothing. */
                        return 0;
                }
                atapi->exit();
                atapi_close();
                ioctl_set_drive(0);
                if (cdrom_enabled)
                {
                        /* Signal disc change to the emulated machine. */
                        atapi_insert_cdrom();
                }
                //                        wx_checkmenuitem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_UNCHECKED);
                //                        wx_checkmenuitem(hmenu, IDM_CDROM_DISABLED,           MF_UNCHECKED);
                //                        wx_checkmenuitem(hmenu, IDM_CDROM_ISO,                MF_UNCHECKED);
                old_cdrom_drive = cdrom_drive;
                cdrom_drive = 0;
                wx_checkmenuitem(hmenu, WX_ID("IDM_CDROM_EMPTY"), WX_MB_CHECKED);
                saveconfig();
                if (!cdrom_enabled)
                {
                        pause = 1;
                        SDL_Delay(100);
                        cdrom_enabled = 1;
                        saveconfig();
                        resetpchard();
                        pause = 0;
                }
        }
        else if (ID_IS("IDM_CDROM_ISO") || ID_IS("IDM_CDROM_ISO_LOAD"))
        {
                if (!getfile(hwnd,
                                "CD-ROM image (*.iso)|*.iso|All files (*.*)|*.*",
                                iso_path))
                {
                        if (!cdrom_enabled)
                        {
                                if (!confirm())
                                        return 0;
                        }
                        old_cdrom_drive = cdrom_drive;
                        strcpy(temp_iso_path, openfilestring);
                        if ((strcmp(iso_path, temp_iso_path) == 0)
                                        && (cdrom_drive == CDROM_ISO)
                                        && cdrom_enabled)
                        {
                                /* Switching from ISO to the same ISO. Do nothing. */
                                return 0;
                        }
                        atapi->exit();
                        atapi_close();
                        iso_open(temp_iso_path);
                        if (cdrom_enabled)
                        {
                                /* Signal disc change to the emulated machine. */
                                atapi_insert_cdrom();
                        }
                        //                                wx_checkmenuitem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_UNCHECKED);
                        //                                wx_checkmenuitem(hmenu, IDM_CDROM_DISABLED,           MF_UNCHECKED);
                        //                                wx_checkmenuitem(hmenu, IDM_CDROM_ISO,                        MF_UNCHECKED);
                        cdrom_drive = CDROM_ISO;
                        wx_checkmenuitem(hmenu, WX_ID("IDM_CDROM_ISO"), WX_MB_CHECKED);
                        saveconfig();
                        if (!cdrom_enabled)
                        {
                                pause = 1;
                                SDL_Delay(100);
                                cdrom_enabled = 1;
                                saveconfig();
                                resetpchard();
                                pause = 0;
                        }
                }
        }
        else
        {
                //                        if (LOWORD(wParam)>=IDM_CDROM_REAL && LOWORD(wParam)<(IDM_CDROM_REAL+100))
                //                        {
                //                                if (!cdrom_enabled)
                //                                {
                //                                        if (wx_messagebox(NULL,"This will reset PCem!\nOkay to continue?","PCem",MB_OKCANCEL) != IDOK)
                //                                           break;
                //                                }
                //                                new_cdrom_drive = LOWORD(wParam)-IDM_CDROM_REAL;
                //                                if ((cdrom_drive == new_cdrom_drive) && cdrom_enabled)
                //                                {
                //                                        /* Switching to the same drive. Do nothing. */
                //                                        break;
                //                                }
                //                                old_cdrom_drive = cdrom_drive;
                //                                atapi->exit();
                //                                atapi_close();
                //                                ioctl_set_drive(new_cdrom_drive);
                //                                if (cdrom_enabled)
                //                                {
                //                                        /* Signal disc change to the emulated machine. */
                //                                        atapi_insert_cdrom();
                //                                }
                //                                wx_checkmenuitem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_UNCHECKED);
                //                                wx_checkmenuitem(hmenu, IDM_CDROM_DISABLED,           MF_UNCHECKED);
                //                                wx_checkmenuitem(hmenu, IDM_CDROM_ISO,                MF_UNCHECKED);
                //                                cdrom_drive = new_cdrom_drive;
                //                                wx_checkmenuitem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_CHECKED);
                //                                saveconfig();
                //                                if (!cdrom_enabled)
                //                                {
                //                                        pause = 1;
                //                                        SDL_Delay(100);
                //                                        cdrom_enabled = 1;
                //                                        saveconfig();
                //                                        resetpchard();
                //                                        pause = 0;
                //                                }
                //                        }
        }
        return 0;
}
