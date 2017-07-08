/*ATI 28800 emulation (VGA Charger)*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "rom.h"
#include "video.h"
#include "vid_ati28800.h"
#include "vid_ati_eeprom.h"
#include "vid_svga.h"
#include "vid_svga_render.h"

typedef struct ati28800_t
{
        svga_t svga;
        ati_eeprom_t eeprom;
        
        rom_t bios_rom;
        
        uint8_t regs[256];
        int index;
} ati28800_t;

void ati28800_out(uint16_t addr, uint8_t val, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        svga_t *svga = &ati28800->svga;
        uint8_t old;
        
//        pclog("ati28800_out : %04X %02X  %04X:%04X\n", addr, val, CS,pc);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout&1)) 
                addr ^= 0x60;

        switch (addr)
        {
                case 0x1ce:
                ati28800->index = val;
                break;
                case 0x1cf:
                ati28800->regs[ati28800->index] = val;
                switch (ati28800->index)
                {
                        case 0xb2:
                        case 0xbe:
                        if (ati28800->regs[0xbe] & 8) /*Read/write bank mode*/
                        {
                                svga->read_bank  = ((ati28800->regs[0xb2] >> 5) & 7) * 0x10000;
                                svga->write_bank = ((ati28800->regs[0xb2] >> 1) & 7) * 0x10000;
                        }
                        else                    /*Single bank mode*/
                                svga->read_bank = svga->write_bank = ((ati28800->regs[0xb2] >> 1) & 7) * 0x10000;
                        break;
                        case 0xb3:
                        ati_eeprom_write(&ati28800->eeprom, val & 8, val & 2, val & 1);
                        break;
                }
                break;
                
                case 0x3D4:
                svga->crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
                        return;
                if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
                        val = (svga->crtc[7] & ~0x10) | (val & 0x10);
                old = svga->crtc[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;
                if (old != val)
                {
                        if (svga->crtcreg < 0xe || svga->crtcreg > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                }
                break;
        }
        svga_out(addr, val, svga);
}

uint8_t ati28800_in(uint16_t addr, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        svga_t *svga = &ati28800->svga;
        uint8_t temp;

//        if (addr != 0x3da) pclog("ati28800_in : %04X ", addr);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout&1)) addr ^= 0x60;
             
        switch (addr)
        {
                case 0x1ce:
                temp = ati28800->index;
                break;
                case 0x1cf:
                switch (ati28800->index)
                {
                        case 0xb7:
                        temp = ati28800->regs[ati28800->index] & ~8;
                        if (ati_eeprom_read(&ati28800->eeprom))
                                temp |= 8;
                        break;
                        
                        default:
                        temp = ati28800->regs[ati28800->index];
                        break;
                }
                break;

                case 0x3c2:
                if ((svga->vgapal[0].r + svga->vgapal[0].g + svga->vgapal[0].b) >= 0x50)
                        temp = 0;
                else
                        temp = 0x10;
                break;
                case 0x3D4:
                temp = svga->crtcreg;
                break;
                case 0x3D5:
                temp = svga->crtc[svga->crtcreg];
                break;
                default:
                temp = svga_in(addr, svga);
                break;
        }
        if (addr != 0x3da) pclog("%02X  %04X:%04X\n", temp, CS,cpu_state.pc);
        return temp;
}

void ati28800_recalctimings(svga_t *svga)
{
        ati28800_t *ati28800 = (ati28800_t *)svga->p;
        pclog("ati28800_recalctimings\n");
        if (!svga->scrblank && (ati28800->regs[0xb0] & 0x20)) /*Extended 256 colour modes*/
        {
                pclog("8bpp_highres\n");
                svga->render = svga_render_8bpp_highres;
                svga->rowoffset <<= 1;
                svga->ma <<= 1;
        }
}               

void *ati28800_init()
{
        ati28800_t *ati28800 = malloc(sizeof(ati28800_t));
        memset(ati28800, 0, sizeof(ati28800_t));
        
        rom_init(&ati28800->bios_rom, "bios.bin", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
        
        svga_init(&ati28800->svga, ati28800, 1 << 19, /*512kb*/
                   ati28800_recalctimings,
                   ati28800_in, ati28800_out,
                   NULL,
                   NULL);

        io_sethandler(0x01ce, 0x0002, ati28800_in, NULL, NULL, ati28800_out, NULL, NULL, ati28800);
        io_sethandler(0x03c0, 0x0020, ati28800_in, NULL, NULL, ati28800_out, NULL, NULL, ati28800);

        ati28800->svga.miscout = 1;

        ati_eeprom_load(&ati28800->eeprom, "ati28800.nvr", 0);

        return ati28800;
}

static int ati28800_available()
{
        return rom_present("bios.bin");
}

void ati28800_close(void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;

        svga_close(&ati28800->svga);
        
        free(ati28800);
}

void ati28800_speed_changed(void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        
        svga_recalctimings(&ati28800->svga);
}

void ati28800_force_redraw(void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;

        ati28800->svga.fullchange = changeframecount;
}

void ati28800_add_status_info(char *s, int max_len, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        
        svga_add_status_info(s, max_len, &ati28800->svga);
}

device_t ati28800_device =
{
        "ATI-28800",
        0,
        ati28800_init,
        ati28800_close,
        ati28800_available,
        ati28800_speed_changed,
        ati28800_force_redraw,
        ati28800_add_status_info
};
