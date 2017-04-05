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

#include "plat-midi.h"
#include "plat-keyboard.h"

#include "wx-sdl2-video.h"
#include "wx-utils.h"
#include "wx-common.h"

#define ID_IS(s) wParam == wx_xrcid(s)
#define ID_RANGE(a, b) wParam >= wx_xrcid(a) && wParam <= wx_xrcid(b)

static int save_window_pos = 0;
uint64_t timer_freq;


int winsizex = 640, winsizey = 480;
int gfx_present[GFX_MAX];

SDL_mutex* ghMutex;
SDL_mutex* rendererMutex;
SDL_mutex* mainMutex;
SDL_cond* rendererCond;
SDL_cond* mainCond;

SDL_Thread* mainthreadh = NULL;
SDL_Thread* renderthread = NULL;

SDL_TimerID onesectimer;

SDL_Window* window = NULL;

int running = 0;
int rendering = 0;

int drawits = 0;

int romspresent[ROM_MAX];
int quited = 0;

SDL_Rect oldclip;
int mousecapture = 0;

void* ghwnd;

void* menu;

int pause = 0;

static int win_doresize = 0;

static int window_doreset = 0;
static int renderer_doreset = 0;
static int window_dofullscreen = 0;
static int window_dowindowed = 0;
static int window_doremember = 0;
static int window_doinputgrab = 0;
static int window_doinputrelease = 0;
static int window_dotogglefullscreen = 0;

SDL_Rect remembered_rect;
int remembered_mouse_x = 0;
int remembered_mouse_y = 0;

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

        win_doresize = 1;
}

void releasemouse()
{
        if (mousecapture)
        {
                SDL_SetWindowGrab(window, SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                mousecapture = 0;
        }
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

int is_fullscreen()
{
        int flags = SDL_GetWindowFlags(window);
        return (flags&SDL_WINDOW_FULLSCREEN) || (flags&SDL_WINDOW_FULLSCREEN_DESKTOP);
}

int is_input_grab() {
        return SDL_GetWindowGrab(window);
}

int get_border_size(int* top, int* left, int* bottom, int* right)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
        return SDL_GetWindowBordersSize(window, top, left, bottom, right);
#else
        if (top) *top = 0;
        if (left) *left = 0;
        if (bottom) *bottom = 0;
        if (right) *right = 0;
        return 0;
#endif
}

uint64_t render_time;
int render_fps = 0;

int renderer_thread(void* params)
{
        SDL_Event event;
        SDL_Rect rect;
        int border_x, border_y;
        int internal_rendering;

        SDL_LockMutex(rendererMutex);
        SDL_CondSignal(rendererCond);
        SDL_UnlockMutex(rendererMutex);

        SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");

        if (start_in_fullscreen) {
                start_in_fullscreen = 0;
                window_dofullscreen = 1;
                window_doinputgrab = 1;
        }

        if (window_remember)
        {
                rect.x = window_x;
                rect.y = window_y;
                rect.w = window_w;
                rect.h = window_h;
        }
        else
        {
                rect.x = SDL_WINDOWPOS_CENTERED;
                rect.y = SDL_WINDOWPOS_CENTERED;
                rect.w = 640;
                rect.h = 480;
        }

        window_doreset = 1;
        rendering = 1;
        while (rendering) {

                window = SDL_CreateWindow("PCem Display",
                                rect.x, rect.y, rect.w, rect.h,
                                vid_resize ? SDL_WINDOW_RESIZABLE : 0);
                if (!window)
                {
                        char message[200];
                        sprintf(message,
                                        "SDL window could not be created! Error: %s\n",
                                        SDL_GetError());
                        wx_messagebox(ghwnd, message, "SDL Error", WX_MB_OK);
                        rendering = 0;
                        wx_exit(ghwnd, 1);
                }

                uint32_t frame_time = SDL_GetTicks();
                uint32_t frames = 0;

                renderer_doreset = 1;
                internal_rendering = 1;
                while (rendering && internal_rendering)
                {
                        uint64_t start_time = timer_read();
                        uint64_t end_time;

                        if (window_doreset)
                        {
                                pclog("window_doreset\n");
                                window_doreset = 0;
                                renderer_doreset = 0;
                                internal_rendering = 0;
                                continue;
                        }
                        if (renderer_doreset)
                        {
                                pclog("renderer_doreset\n");
                                renderer_doreset = 0;
                                sdl_renderer_close();
                                sdl_renderer_init(window);

                                device_force_redraw();
                                video_wait_for_blit();
                        }
                        while(SDL_PollEvent(&event))
                        {
                                switch (event.type) {
                                case SDL_MOUSEBUTTONUP:
                                        if (!mousecapture && event.button.button == SDL_BUTTON_LEFT) {
                                                window_doinputgrab = 1;
                                                if (video_fullscreen) {
                                                        window_dofullscreen = 1;
                                                }
                                        }
                                        break;
                                case SDL_MOUSEWHEEL:
                                        if (mousecapture) mouse_wheel_update(event.wheel.y);
                                        break;
                                case SDL_WINDOWEVENT:
                                        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                                                wx_exit(ghwnd, 0);
                                        }
                                        if (window_remember) {
                                                int flags = SDL_GetWindowFlags(window);
                                                if (!(flags&SDL_WINDOW_FULLSCREEN) && !(flags&SDL_WINDOW_FULLSCREEN_DESKTOP)) {
                                                        if (event.window.event == SDL_WINDOWEVENT_MOVED) {
                                                                get_border_size(&border_x, &border_y, 0, 0);
                                                                window_x = event.window.data1-border_x;
                                                                window_y = event.window.data2-border_y;
                                                        } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                                                                window_w = event.window.data1;
                                                                window_h = event.window.data2;
                                                        }
                                                        save_window_pos = 1;
                                                }
                                        }
                                        break;
                                case SDL_KEYUP:
                                        if (event.key.keysym.scancode == SDL_SCANCODE_PAGEDOWN && (event.key.keysym.mod&KMOD_CTRL) && (event.key.keysym.mod&KMOD_ALT))
                                        {
                                                toggle_fullscreen();
                                        }
                                        else if (event.key.keysym.scancode == SDL_SCANCODE_PAGEUP && (event.key.keysym.mod&KMOD_CTRL) && (event.key.keysym.mod&KMOD_ALT))
                                        {
                                                wx_togglewindow(ghwnd);
                                        }
                                        else if (event.key.keysym.scancode == SDL_SCANCODE_END && (event.key.keysym.mod&KMOD_CTRL))
                                        {
                                                if (!is_fullscreen())
                                                        window_doinputrelease = 1;
                                        }
                                        break;
                                }
                        }
                        if (window_doremember) {
                                window_doremember = 0;
                                SDL_GetWindowPosition(window, &window_x, &window_y);
                                SDL_GetWindowSize(window, &window_w, &window_h);
                                get_border_size(&border_y, &border_x, 0, 0);
                                window_x -= border_x;
                                window_y -= border_y;
                                saveconfig();
                        }

                        if (window_dotogglefullscreen)
                        {
                                window_dotogglefullscreen = 0;
                                if (is_input_grab() || is_fullscreen())
                                {
                                        window_doinputrelease = 1;
                                        if (is_fullscreen())
                                                window_dowindowed = 1;
                                }
                                else
                                {
                                        window_doinputgrab = 1;
                                        window_dofullscreen = 1;
                                }
                        }

                        if (window_dofullscreen)
                        {
                                window_dofullscreen = 0;
                                video_wait_for_blit();
                                SDL_RaiseWindow(window);
                                SDL_GetGlobalMouseState(&remembered_mouse_x, &remembered_mouse_y);
                                SDL_GetWindowPosition(window, &remembered_rect.x, &remembered_rect.y);
                                get_border_size(&border_y, &border_x, 0, 0);
                                remembered_rect.x -= border_x;
                                remembered_rect.y -= border_y;
                                SDL_GetWindowSize(window, &remembered_rect.w, &remembered_rect.h);
                                SDL_SetWindowFullscreen(window, video_fullscreen_mode == 0 ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN);
                        }
                        if (window_doinputgrab) {
                                window_doinputgrab = 0;
                                mousecapture = 1;
                                SDL_GetRelativeMouseState(0, 0);
                                SDL_SetWindowGrab(window, SDL_TRUE);
                                SDL_SetRelativeMouseMode(SDL_TRUE);
                        }

                        if (window_doinputrelease) {
                                window_doinputrelease = 0;
                                mousecapture = 0;
                                SDL_SetWindowGrab(window, SDL_FALSE);
                                SDL_SetRelativeMouseMode(SDL_FALSE);
                        }
                        if (window_dowindowed)
                        {
                                window_dowindowed = 0;
                                SDL_SetWindowFullscreen(window, 0);
                                SDL_SetWindowSize(window, remembered_rect.w, remembered_rect.h);
                                SDL_SetWindowPosition(window, remembered_rect.x, remembered_rect.y);
                                SDL_WarpMouseGlobal(remembered_mouse_x, remembered_mouse_y);
                        }

                        if (win_doresize)
                        {
                                win_doresize = 0;
                                if (!vid_resize || (flags&SDL_WINDOW_FULLSCREEN)) {
                                        SDL_GetWindowSize(window, &rect.w, &rect.h);
                                        if (rect.w != winsizex || rect.h != winsizey) {
                                                SDL_GetWindowPosition(window, &rect.x, &rect.y);
                                                SDL_SetWindowSize(window, winsizex, winsizey);
                                                SDL_SetWindowPosition(window, rect.x, rect.y);
                                                device_force_redraw();
                                        }
                                }
                        }

                        if (sdl_renderer_update(window)) sdl_renderer_present(window);

                        end_time = timer_read();
                        render_time += end_time - start_time;

                        ++frames;
                        uint32_t ticks = SDL_GetTicks();
                        if (ticks-frame_time >= 1000) {
                                render_fps = frames/((ticks-frame_time)/1000.0);
                                frames = 0;
                                frame_time = ticks;
                        }

                        SDL_Delay(1);
                }
                sdl_renderer_close();

                if (window) {
                        if (rendering) { /* save current position and size of the window */
                                SDL_GetWindowPosition(window, &rect.x, &rect.y);
                                SDL_GetWindowSize(window, &rect.w, &rect.h);
                                get_border_size(&border_y, &border_x, 0, 0);
                                rect.x -= border_x;
                                rect.y -= border_y;
                        }

                        SDL_DestroyWindow(window);
                }
        }

        SDL_LockMutex(rendererMutex);
        SDL_CondSignal(rendererCond);
        SDL_UnlockMutex(rendererMutex);

        return SDL_TRUE;
}

void renderer_start()
{
        if (!rendering)
        {
                // we need to wait for renderer to have started in the thread or pumpevents may not work
                SDL_LockMutex(rendererMutex);

                renderthread = SDL_CreateThread(renderer_thread, "SDL2 Thread",
                                NULL);

                SDL_CondWait(rendererCond, rendererMutex);

                SDL_UnlockMutex(rendererMutex);
        }
}

void renderer_stop(int timeout)
{
        if (rendering)
        {
                SDL_LockMutex(rendererMutex);
                rendering = 0;
                if (timeout)
                {
                        SDL_CondWaitTimeout(rendererCond, rendererMutex,
                                        timeout);
                }
                else
                {
                        SDL_CondWait(rendererCond, rendererMutex);
                }
                SDL_UnlockMutex(rendererMutex);

                renderthread = NULL;
        }
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
        SDL_Rect rect;
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

        renderer_stop(10 * 1000);

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

extern void wx_loadconfig();
extern void wx_saveconfig();

int pc_main(int argc, char** argv)
{
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
        {
                printf("SDL could not initialize! Error: %s\n", SDL_GetError());
                return 1;
        }

        SDL_version ver;
        SDL_GetVersion(&ver);
        printf("SDL %i.%i.%i initialized.\n", ver.major, ver.minor, ver.patch);

        sdl_video_init();

        add_config_callback(sdl_loadconfig, sdl_saveconfig);
        add_config_callback(wx_loadconfig, wx_saveconfig);

        initpc(argc, argv);

        return TRUE;
}

int wx_keydown(void* window, int keycode, int modifiers)
{
        return 1;
}

int wx_keyup(void* window, int keycode, int modifiers)
{
        // translate the necessary keys
        SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
        switch (keycode)
        {
        case WXK_PAGEUP:
                scancode = SDL_SCANCODE_PAGEUP;
                break;
        case WXK_PAGEDOWN:
                scancode = SDL_SCANCODE_PAGEDOWN;
                break;
        }
        int mod = 0;
        if (modifiers&wxMOD_ALT)
                mod |= KMOD_ALT;
        if (modifiers&wxMOD_CONTROL)
                mod |= KMOD_CTRL;
        if (scancode != SDL_SCANCODE_UNKNOWN)
        {
                SDL_Event event;
                event.type = SDL_KEYUP;
                event.key.keysym.scancode = scancode;
                event.key.keysym.mod = mod;
                SDL_PushEvent(&event);
                return 1;
        }
        return 0;
}

int wx_start(void* hwnd)
{
        int c, d;
        ghwnd = hwnd;

        wx_keydown_func = wx_keydown;
        wx_keyup_func = wx_keyup;

        menu = wx_getmenu(hwnd);

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
        wx_checkmenuitem(menu, WX_ID("IDM_HIDE_ON_CLOSE"), hide_on_close);
        wx_checkmenuitem(menu, WX_ID("IDM_HIDE_ON_START"), hide_on_start);
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

        loadbios();
        resetpchard();

        atexit(releasemouse);

        memset(rawinputkey, 0, sizeof(rawinputkey));

        ghMutex = SDL_CreateMutex();
        rendererMutex = SDL_CreateMutex();
        mainMutex = SDL_CreateMutex();
        rendererCond = SDL_CreateCond();
        mainCond = SDL_CreateCond();

        renderer_start();
        mainthreadh = SDL_CreateThread(mainthread, "Main Thread", NULL);

        onesectimer = SDL_AddTimer(1000, timer_onesec, NULL);

        updatewindowsize(640, 480);

        timer_freq = timer_read();

        return TRUE;
}

void wx_stop()
{
        pclog("Shutting down...\n");
        SDL_LockMutex(mainMutex);
        running = 0;
        SDL_CondWaitTimeout(mainCond, mainMutex, 10 * 1000);
        SDL_UnlockMutex(mainMutex);

        SDL_DestroyCond(rendererCond);
        SDL_DestroyCond(mainCond);
        SDL_DestroyMutex(rendererMutex);
        SDL_DestroyMutex(mainMutex);

        startblit();
        SDL_Delay(200);
        SDL_DetachThread(renderthread);
        SDL_DetachThread(mainthreadh);
        SDL_RemoveTimer(onesectimer);
        savenvr();
        saveconfig();
        if (save_window_pos && window_remember)
        {
                saveconfig();
        }
        closepc();
        releasemouse();

        sdl_video_close();

        SDL_DestroyMutex(ghMutex);

        SDL_Quit();

        printf("Shut down successfully!\n");
}

char openfilestring[260];
int getfile(void* hwnd, char *f, char *fn)
{
        return wx_filedialog(hwnd, "Open", fn, f, 1, openfilestring);
}

int getsfile(void* hwnd, char *f, char *fn)
{
        return wx_filedialog(hwnd, "Save", fn, f, 0, openfilestring);
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
        return wx_messagebox(NULL, "This will reset PCem!\nOkay to continue?",
                        "PCem", WX_MB_OKCANCEL) == WX_IDOK;
}

int wx_handle_menu(void* hwnd, int wParam, int checked)
{
        SDL_Rect rect;
        void* hmenu;
        char temp_iso_path[1024];
        int new_cdrom_drive;
        hmenu = wx_getmenu(hwnd);
        if (ID_IS("IDM_FILE_RESET"))
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
        else if (ID_IS("IDM_HIDE_ON_CLOSE"))
        {
                if (hide_on_close_first)
                {
                        hide_on_close_first = 0;
                        wx_messagebox(hwnd,
                                        "Press CTRL + ALT + PAGE UP to show the machine window",
                                        "PCem", WX_MB_OK);
                }
                hide_on_close = checked;
                saveconfig();
        }
        else if (ID_IS("IDM_HIDE_ON_START"))
        {
                if (hide_on_close_first)
                {
                        hide_on_close_first = 0;
                        wx_messagebox(hwnd,
                                        "Press CTRL + ALT + PAGE UP to show the machine window",
                                        "PCem", WX_MB_OK);
                }
                hide_on_start = checked;
                saveconfig();
        }
        else if (ID_IS("IDM_MACHINE_TOGGLE"))
        {
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
                        if (wx_messagebox(NULL,
                                        "This will reset PCem!\nOkay to continue?",
                                        "PCem", WX_MB_OKCANCEL) == WX_IDOK)
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
