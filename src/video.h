#ifdef PCEM_ALLEGRO

#include "allegro-main.h"

#else

typedef struct
{
        int w, h;
        uint8_t *dat;
        uint8_t *line[0];
} BITMAP;

extern BITMAP *screen;

BITMAP *create_bitmap(int w, int h);

typedef struct
{
        uint8_t r, g, b;
} RGB;
        
typedef RGB PALETTE[256];

#define makecol(r, g, b)    ((b) | ((g) << 8) | ((r) << 16))
#define makecol32(r, g, b)  ((b) | ((g) << 8) | ((r) << 16))

#endif

extern BITMAP *buffer, *buffer32;

int video_card_available(int card);
char *video_card_getname(int card);
struct device_t *video_card_getdevice(int card);
int video_card_has_config(int card);
int video_card_getid(char *s);
int video_old_to_new(int card);
int video_new_to_old(int card);
char *video_get_internal_name(int card);
int video_get_video_from_internal_name(char *s);

extern int video_fullscreen, video_fullscreen_scale, video_fullscreen_first;

enum
{
        FULLSCR_SCALE_FULL = 0,
        FULLSCR_SCALE_43,
        FULLSCR_SCALE_SQ,
        FULLSCR_SCALE_INT
};

extern int egareads,egawrites;

extern int fullchange;
extern int changeframecount;

extern uint8_t fontdat[256][8];
extern uint8_t fontdatm[256][16];

extern uint32_t *video_15to32, *video_16to32;

extern int xsize,ysize;

extern float cpuclock;

extern int emu_fps, frames;

extern int readflash;

extern void (*video_recalctimings)();

void video_blit_memtoscreen(int x, int y, int y1, int y2, int w, int h);
void video_blit_memtoscreen_8(int x, int y, int w, int h);

extern void (*video_blit_memtoscreen_func)(int x, int y, int y1, int y2, int w, int h);
extern void (*video_blit_memtoscreen_8_func)(int x, int y, int w, int h);

extern int video_timing_b, video_timing_w, video_timing_l;
extern int video_speed;

extern int video_res_x, video_res_y, video_bpp;

extern int vid_resize;

void video_wait_for_blit();
void video_wait_for_buffer();
void loadfont(char *s, int format);

void initvideo();
void video_init();
void closevideo();

void video_updatetiming();

void hline(BITMAP *b, int x1, int y, int x2, uint32_t col);

void destroy_bitmap(BITMAP *b);
