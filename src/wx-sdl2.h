extern int mousecapture;

#ifdef __cplusplus
extern "C" {
#endif

void leave_fullscreen();

#ifdef __cplusplus
}
#endif

extern char openfilestring[260];

void hdconf_open(void* hwnd);

void config_open(void* hwnd);

void deviceconfig_open(void* hwnd, struct device_t *device);
void joystickconfig_open(void* hwnd, int joy_nr, int type);

extern int pause;

extern int video_scale_mode;
extern int video_vsync;
extern int video_focus_dim;
extern int video_fullscreen_mode;
