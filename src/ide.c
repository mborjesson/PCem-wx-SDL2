/*RPCemu v0.6 by Tom Walker
  IDE emulation*/
//#define RPCEMU_IDE

#define IDE_TIME (5 * 100 * (1 << TIMER_SHIFT))

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>

#ifdef RPCEMU_IDE
        #include "rpcemu.h"
        #include "iomd.h"
        #include "arm.h"
#else
        #include "ibm.h"
        #include "io.h"
        #include "pic.h"
        #include "timer.h"
        #include "hdd_file.h"
#endif
#include "ide.h"

/* Bits of 'atastat' */
#define ERR_STAT		0x01
#define DRQ_STAT		0x08 /* Data request */
#define DSC_STAT                0x10
#define SERVICE_STAT            0x10
#define READY_STAT		0x40
#define BUSY_STAT		0x80

/* Bits of 'error' */
#define ABRT_ERR		0x04 /* Command aborted */
#define MCR_ERR			0x08 /* Media change request */

/* ATA Commands */
#define WIN_SRST			0x08 /* ATAPI Device Reset */
#define WIN_RECAL			0x10
#define WIN_RESTORE			WIN_RECAL
#define WIN_READ			0x20 /* 28-Bit Read */
#define WIN_READ_NORETRY                0x21 /* 28-Bit Read - no retry*/
#define WIN_WRITE			0x30 /* 28-Bit Write */
#define WIN_WRITE_NORETRY		0x31 /* 28-Bit Write */
#define WIN_VERIFY			0x40 /* 28-Bit Verify */
#define WIN_VERIFY_ONCE                 0x41 /* Deprecated command - same as 0x40 */
#define WIN_FORMAT			0x50
#define WIN_SEEK			0x70
#define WIN_DRIVE_DIAGNOSTICS           0x90 /* Execute Drive Diagnostics */
#define WIN_SPECIFY			0x91 /* Initialize Drive Parameters */
#define WIN_PACKETCMD			0xA0 /* Send a packet command. */
#define WIN_PIDENTIFY			0xA1 /* Identify ATAPI device */
#define WIN_READ_MULTIPLE               0xC4
#define WIN_WRITE_MULTIPLE              0xC5
#define WIN_SET_MULTIPLE_MODE           0xC6
#define WIN_READ_DMA                    0xC8
#define WIN_WRITE_DMA                   0xCA
#define WIN_SETIDLE1			0xE3
#define WIN_IDENTIFY			0xEC /* Ask drive to identify itself */

/* ATAPI Commands */
#define GPCMD_TEST_UNIT_READY           0x00
#define GPCMD_REQUEST_SENSE		0x03
#define GPCMD_READ_6			0x08
#define GPCMD_INQUIRY			0x12
#define GPCMD_MODE_SELECT_6		0x15
#define GPCMD_MODE_SENSE_6		0x1a
#define GPCMD_START_STOP_UNIT		0x1b
#define GPCMD_PREVENT_REMOVAL          	0x1e
#define GPCMD_READ_CDROM_CAPACITY	0x25
#define GPCMD_READ_10                   0x28
#define GPCMD_SEEK			0x2b
#define GPCMD_READ_SUBCHANNEL		0x42
#define GPCMD_READ_TOC_PMA_ATIP		0x43
#define GPCMD_READ_HEADER		0x44
#define GPCMD_PLAY_AUDIO_10		0x45
#define GPCMD_PLAY_AUDIO_MSF	        0x47
#define GPCMD_GET_EVENT_STATUS_NOTIFICATION	0x4a
#define GPCMD_PAUSE_RESUME		0x4b
#define GPCMD_STOP_PLAY_SCAN            0x4e
#define GPCMD_READ_DISC_INFORMATION	0x51
#define GPCMD_MODE_SELECT_10		0x55
#define GPCMD_MODE_SENSE_10		0x5a
#define GPCMD_PLAY_AUDIO_12		0xa5
#define GPCMD_READ_12                   0xa8
#define GPCMD_SEND_DVD_STRUCTURE	0xad
#define GPCMD_SET_SPEED			0xbb
#define GPCMD_MECHANISM_STATUS		0xbd
#define GPCMD_READ_CD			0xbe

/* Mode page codes for mode sense/set */
#define GPMODE_R_W_ERROR_PAGE		0x01
#define GPMODE_CDROM_PAGE		0x0d
#define GPMODE_CDROM_AUDIO_PAGE		0x0e
#define GPMODE_CAPABILITIES_PAGE	0x2a
#define GPMODE_ALL_PAGES		0x3f

/* ATAPI Sense Keys */
#define SENSE_NONE			0
#define SENSE_NOT_READY			2
#define SENSE_ILLEGAL_REQUEST		5
#define SENSE_UNIT_ATTENTION		6

/* ATAPI Additional Sense Codes */
#define ASC_AUDIO_PLAY_OPERATION	0x00
#define ASC_ILLEGAL_OPCODE		0x20
#define	ASC_LBA_OUT_OF_RANGE            0x21
#define	ASC_INV_FIELD_IN_CMD_PACKET	0x24
#define ASC_MEDIUM_MAY_HAVE_CHANGED	0x28
#define ASC_MEDIUM_NOT_PRESENT		0x3a
#define ASC_DATA_PHASE_ERROR		0x4b
#define ASC_ILLEGAL_MODE_FOR_THIS_TRACK	0x64

#define ASCQ_AUDIO_PLAY_OPERATION_IN_PROGRESS	0x11
#define ASCQ_AUDIO_PLAY_OPERATION_PAUSED	0x12
#define ASCQ_AUDIO_PLAY_OPERATION_COMPLETED	0x13

/* Tell RISC OS that we have a 4x CD-ROM drive (600kb/sec data, 706kb/sec raw).
   Not that it means anything */
#define CDROM_SPEED	706

/** Evaluate to non-zero if the currently selected drive is an ATAPI device */
#define IDE_DRIVE_IS_CDROM(ide)  (ide->type == IDE_CDROM)
/*
\
	(!ide.drive)*/

/* Event notification classes for GET EVENT STATUS NOTIFICATION */
#define GESN_NO_EVENTS                0
#define GESN_OPERATIONAL_CHANGE       1
#define GESN_POWER_MANAGEMENT         2
#define GESN_EXTERNAL_REQUEST         3
#define GESN_MEDIA                    4
#define GESN_MULTIPLE_HOSTS           5
#define GESN_DEVICE_BUSY              6

/* Event codes for MEDIA event status notification */
#define MEC_NO_CHANGE                 0
#define MEC_EJECT_REQUESTED           1
#define MEC_NEW_MEDIA                 2
#define MEC_MEDIA_REMOVAL             3 /* only for media changers */
#define MEC_MEDIA_CHANGED             4 /* only for media changers */
#define MEC_BG_FORMAT_COMPLETED       5 /* MRW or DVD+RW b/g format completed */
#define MEC_BG_FORMAT_RESTARTED       6 /* MRW or DVD+RW b/g format restarted */
#define MS_TRAY_OPEN                  1
#define MS_MEDIA_PRESENT              2

#define CHECK_READY		2
#define ALLOW_UA		1

/* Table of all ATAPI commands and their flags, needed for the new disc change / not ready handler. */
uint8_t atapi_cmd_table[0x100] =
{
	[GPCMD_TEST_UNIT_READY]               = CHECK_READY,
	[GPCMD_REQUEST_SENSE]                 = ALLOW_UA,
	[GPCMD_READ_6]                        = CHECK_READY,
	[GPCMD_INQUIRY]                       = ALLOW_UA,
	[GPCMD_MODE_SELECT_6]                 = 0,
	[GPCMD_MODE_SENSE_6]                  = 0,
	[GPCMD_START_STOP_UNIT]               = 0,
	[GPCMD_PREVENT_REMOVAL]               = CHECK_READY,
	[GPCMD_READ_CDROM_CAPACITY]           = CHECK_READY,
	[GPCMD_READ_10]                       = CHECK_READY,
	[GPCMD_SEEK]                          = CHECK_READY,
	[GPCMD_READ_SUBCHANNEL]               = CHECK_READY,
	[GPCMD_READ_TOC_PMA_ATIP]             = CHECK_READY | ALLOW_UA,		/* Read TOC - can get through UNIT_ATTENTION, per VIDE-CDD.SYS */
	[GPCMD_READ_HEADER]                   = CHECK_READY,
	[GPCMD_PLAY_AUDIO_10]                 = CHECK_READY,
	[GPCMD_PLAY_AUDIO_MSF]                = CHECK_READY,
	[GPCMD_GET_EVENT_STATUS_NOTIFICATION] = ALLOW_UA,
	[GPCMD_PAUSE_RESUME]                  = CHECK_READY,
	[GPCMD_STOP_PLAY_SCAN]                = CHECK_READY,
	[GPCMD_READ_DISC_INFORMATION]         = CHECK_READY,
	[GPCMD_MODE_SELECT_10]                = 0,
	[GPCMD_MODE_SENSE_10]                 = 0,
	[GPCMD_PLAY_AUDIO_12]                 = CHECK_READY,
	[GPCMD_READ_12]                       = CHECK_READY,
	[GPCMD_SEND_DVD_STRUCTURE]            = CHECK_READY,		/* Read DVD structure (NOT IMPLEMENTED YET) */
	[GPCMD_SET_SPEED]                     = 0,
	[GPCMD_MECHANISM_STATUS]              = 0,
	[GPCMD_READ_CD]                       = CHECK_READY,
	[0xBF] = CHECK_READY	/* Send DVD structure (NOT IMPLEMENTED YET) */
};

#define IMPLEMENTED		1

uint8_t mode_sense_pages[0x40] =
{
	[GPMODE_R_W_ERROR_PAGE]    = IMPLEMENTED,
	[GPMODE_CDROM_PAGE]        = IMPLEMENTED,
	[GPMODE_CDROM_AUDIO_PAGE]  = IMPLEMENTED,
	[GPMODE_CAPABILITIES_PAGE] = IMPLEMENTED,
	[GPMODE_ALL_PAGES]         = IMPLEMENTED
};

ATAPI *atapi;

int atapi_command = 0;
int readcdmode = 0;

int cdrom_channel = 2;

/* Mode sense/select stuff. */
uint8_t mode_pages_in[256][256];
#define PAGE_CHANGEABLE		1
#define PAGE_CHANGED		2
uint8_t page_flags[256] =
{
	[GPMODE_R_W_ERROR_PAGE]    = 0,
	[GPMODE_CDROM_PAGE]        = 0,
	[GPMODE_CDROM_AUDIO_PAGE]  = PAGE_CHANGEABLE,
	[GPMODE_CAPABILITIES_PAGE] = 0,
};
uint8_t prefix_len;
uint8_t page_current;

#define ATAPI_STATUS_IDLE            0
#define ATAPI_STATUS_COMMAND         1
#define ATAPI_STATUS_COMPLETE        2
#define ATAPI_STATUS_DATA            3
#define ATAPI_STATUS_PACKET_REQ      4
#define ATAPI_STATUS_PACKET_RECEIVED 5
#define ATAPI_STATUS_READCD          6
#define ATAPI_STATUS_REQ_SENSE       7
#define ATAPI_STATUS_ERROR           0x80
#define ATAPI_STATUS_ERROR_2         0x81

enum
{
        IDE_NONE = 0,
        IDE_HDD,
        IDE_CDROM
};

typedef struct IDE
{
        int type;
        int board;
        uint8_t atastat;
        uint8_t error;
        int secount,sector,cylinder,head,drive,cylprecomp;
        uint8_t command;
        uint8_t fdisk;
        int pos;
        int packlen;
        int packetstatus;
        int cdpos,cdlen;
        uint8_t asc;
        int reset;
        uint16_t buffer[65536];
        int irqstat;
        int service;
        int lba;
        uint32_t lba_addr;
        int skip512;
        int blocksize, blockcount;
        uint8_t sector_buffer[256*512];
        int do_initial_read;
        int sector_pos;
#ifdef RPCEMU_IDE
        FILE *hdfile;
        int spt,hpc;
        int tracks;
#else
        hdd_file_t hdd_file;
#endif
} IDE;

IDE ide_drives[4];

IDE *ext_ide;

char ide_fn[4][512];

int (*ide_bus_master_read_sector)(int channel, uint8_t *data);
int (*ide_bus_master_write_sector)(int channel, uint8_t *data);
void (*ide_bus_master_set_irq)(int channel);

static void callnonreadcd(IDE *ide);
static void callreadcd(IDE *ide);
static void atapicommand(int ide_board);

int idecallback[2] = {0, 0};

int cur_ide[2];

uint8_t getstat(IDE *ide) { return ide->atastat; }

static inline void ide_irq_raise(IDE *ide)
{
//        pclog("IDE_IRQ_RAISE\n");
	if (!(ide->fdisk&2)) {
#ifdef RPCEMU_IDE
		iomd.irqb.status |= IOMD_IRQB_IDE;
		updateirqs();
#else
//                if (ide->board && !ide->irqstat) pclog("IDE_IRQ_RAISE\n");
                picint((ide->board)?(1<<15):(1<<14));
                if (ide_bus_master_set_irq)
                        ide_bus_master_set_irq(ide->board);
#endif
	}
	ide->irqstat=1;
        ide->service=1;
}

static inline void ide_irq_lower(IDE *ide)
{
//        pclog("IDE_IRQ_LOWER\n");
//	if (ide.board == 0) {
#ifdef RPCEMU_IDE
		iomd.irqb.status &= ~IOMD_IRQB_IDE;
		updateirqs();
#else
                picintc((ide->board)?(1<<15):(1<<14));
#endif
//	}
	ide->irqstat=0;
}

void ide_irq_update(IDE *ide)
{
#ifdef RPCEMU_IDE
	if (ide->irqstat && !(iomd.irqb.status & IOMD_IRQB_IDE) && !(ide->fdisk & 2)) {
		iomd.irqb.status |= IOMD_IRQB_IDE;
		updateirqs();
        }
        else if (iomd.irqb.status & IOMD_IRQB_IDE)
        {
		iomd.irqb.status &= ~IOMD_IRQB_IDE;
		updateirqs();
        }
#else
	if (ide->irqstat && !((pic2.pend|pic2.ins)&0x40) && !(ide->fdisk & 2))
            picint((ide->board)?(1<<15):(1<<14));
        else if ((pic2.pend|pic2.ins)&0x40)
            picintc((ide->board)?(1<<15):(1<<14));
#endif
}
/**
 * Copy a string into a buffer, padding with spaces, and placing characters as
 * if they were packed into 16-bit values, stored little-endian.
 *
 * @param str Destination buffer
 * @param src Source string
 * @param len Length of destination buffer to fill in. Strings shorter than
 *            this length will be padded with spaces.
 */
static void
ide_padstr(char *str, const char *src, int len)
{
	int i, v;

	for (i = 0; i < len; i++) {
		if (*src != '\0') {
			v = *src++;
		} else {
			v = ' ';
		}
		str[i ^ 1] = v;
	}
}

/**
 * Copy a string into a buffer, padding with spaces. Does not add string
 * terminator.
 *
 * @param buf      Destination buffer
 * @param buf_size Size of destination buffer to fill in. Strings shorter than
 *                 this length will be padded with spaces.
 * @param src      Source string
 */
static void
ide_padstr8(uint8_t *buf, int buf_size, const char *src)
{
	int i;

	for (i = 0; i < buf_size; i++) {
		if (*src != '\0') {
			buf[i] = *src++;
		} else {
			buf[i] = ' ';
		}
	}
}

/**
 * Fill in ide->buffer with the output of the "IDENTIFY DEVICE" command
 */
static void ide_identify(IDE *ide)
{
	memset(ide->buffer, 0, 512);

	//ide->buffer[1] = 101; /* Cylinders */

#ifdef RPCEMU_IDE
	ide->buffer[1] = 65535; /* Cylinders */
	ide->buffer[3] = 16;  /* Heads */
	ide->buffer[6] = 63;  /* Sectors */
#else
	ide->buffer[1] = hdc[cur_ide[ide->board]].tracks; /* Cylinders */
	ide->buffer[3] = hdc[cur_ide[ide->board]].hpc;  /* Heads */
	ide->buffer[6] = hdc[cur_ide[ide->board]].spt;  /* Sectors */
#endif
	ide_padstr((char *) (ide->buffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (ide->buffer + 23), "v1.0", 8); /* Firmware */
#ifdef RPCEMU_IDE
	ide_padstr((char *) (ide->buffer + 27), "RPCemuHD", 40); /* Model */
#else
	ide_padstr((char *) (ide->buffer + 27), "PCemHD", 40); /* Model */
#endif
        ide->buffer[20] = 3;   /*Buffer type*/
        ide->buffer[21] = 512; /*Buffer size*/
        ide->buffer[47] = 16;  /*Max sectors on multiple transfer command*/
        ide->buffer[48] = 1;   /*Dword transfers supported*/
	ide->buffer[49] = (1 << 9) | (1 << 8); /* LBA and DMA supported */
	ide->buffer[50] = 0x4000; /* Capabilities */
	ide->buffer[51] = 2 << 8; /*PIO timing mode*/
	ide->buffer[52] = 2 << 8; /*DMA timing mode*/
	ide->buffer[59] = ide->blocksize ? (ide->blocksize | 0x100) : 0;
#ifdef RPCEMU_IDE
	ide->buffer[60] = (65535 * 16 * 63) & 0xFFFF; /* Total addressable sectors (LBA) */
	ide->buffer[61] = (65535 * 16 * 63) >> 16;
#else
	ide->buffer[60] = (hdc[cur_ide[ide->board]].tracks * hdc[cur_ide[ide->board]].hpc * hdc[cur_ide[ide->board]].spt) & 0xFFFF; /* Total addressable sectors (LBA) */
	ide->buffer[61] = (hdc[cur_ide[ide->board]].tracks * hdc[cur_ide[ide->board]].hpc * hdc[cur_ide[ide->board]].spt) >> 16;
#endif
	ide->buffer[63] = 7; /*Multiword DMA*/
	ide->buffer[80] = 0xe; /*ATA-1 to ATA-3 supported*/
}

/**
 * Fill in ide->buffer with the output of the "IDENTIFY PACKET DEVICE" command
 */
static void ide_atapi_identify(IDE *ide)
{
	memset(ide->buffer, 0, 512);

	ide->buffer[0] = 0x8000 | (5<<8) | 0x80 | (2<<5); /* ATAPI device, CD-ROM drive, removable media, accelerated DRQ */
	ide_padstr((char *) (ide->buffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (ide->buffer + 23), "v1.0", 8); /* Firmware */
#ifdef RPCEMU_IDE
	ide_padstr((char *) (ide->buffer + 27), "RPCemuCD", 40); /* Model */
#else
	ide_padstr((char *) (ide->buffer + 27), "PCemCD", 40); /* Model */
#endif
	ide->buffer[49] = 0x200; /* LBA supported */
}

/**
 * Fill in ide->buffer with the output of the ATAPI "MODE SENSE" command
 *
 * @param pos Offset within the buffer to start filling in data
 *
 * @return Offset within the buffer after the end of the data
 */
static uint32_t ide_atapi_mode_sense(IDE *ide, uint32_t pos, uint8_t type)
{
	uint8_t *buf = (uint8_t *) ide->buffer;
//        pclog("ide_atapi_mode_sense %02X\n",type);
        if (type==GPMODE_ALL_PAGES || type==GPMODE_R_W_ERROR_PAGE)
        {
        	/* &01 - Read error recovery */
        	buf[pos++] = GPMODE_R_W_ERROR_PAGE;
        	buf[pos++] = 6; /* Page length */
        	buf[pos++] = 0; /* Error recovery parameters */
        	buf[pos++] = 5; /* Read retry count */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        }

        if (type==GPMODE_ALL_PAGES || type==GPMODE_CDROM_PAGE)
        {
        	/* &0D - CD-ROM Parameters */
        	buf[pos++] = GPMODE_CDROM_PAGE;
        	buf[pos++] = 6; /* Page length */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 1; /* Inactivity time multiplier *NEEDED BY RISCOS* value is a guess */
        	buf[pos++] = 0; buf[pos++] = 60; /* MSF settings */
        	buf[pos++] = 0; buf[pos++] = 75; /* MSF settings */
        }

        if (type==GPMODE_ALL_PAGES || type==GPMODE_CDROM_AUDIO_PAGE)
        {
        	/* &0e - CD-ROM Audio Control Parameters */
        	buf[pos++] = GPMODE_CDROM_AUDIO_PAGE;
		buf[pos++] = 0xE; /* Page length */
		if (page_flags[GPMODE_CDROM_AUDIO_PAGE] & PAGE_CHANGED)
		{
                        int i;
                        
			for (i = 0; i < 14; i++)
			{
				buf[pos++] = mode_pages_in[GPMODE_CDROM_AUDIO_PAGE][i];
			}
		}
		else
		{
			buf[pos++] = 4; /* Reserved */
			buf[pos++] = 0; /* Reserved */
			buf[pos++] = 0; /* Reserved */
			buf[pos++] = 0; /* Reserved */
			buf[pos++] = 0; buf[pos++] = 75; /* Logical audio block per second */
			buf[pos++] = 1;    /* CDDA Output Port 0 Channel Selection */
			buf[pos++] = 0xFF; /* CDDA Output Port 0 Volume */
			buf[pos++] = 2;    /* CDDA Output Port 1 Channel Selection */
			buf[pos++] = 0xFF; /* CDDA Output Port 1 Volume */
			buf[pos++] = 0;    /* CDDA Output Port 2 Channel Selection */
			buf[pos++] = 0;    /* CDDA Output Port 2 Volume */
			buf[pos++] = 0;    /* CDDA Output Port 3 Channel Selection */
			buf[pos++] = 0;    /* CDDA Output Port 3 Volume */
		}
        }

        if (type==GPMODE_ALL_PAGES || type==GPMODE_CAPABILITIES_PAGE)
        {
//                pclog("Capabilities page\n");
               	/* &2A - CD-ROM capabilities and mechanical status */
        	buf[pos++] = GPMODE_CAPABILITIES_PAGE;
        	buf[pos++] = 0x12; /* Page length */
        	buf[pos++] = 0; buf[pos++] = 0; /* CD-R methods */
        	buf[pos++] = 1; /* Supports audio play, not multisession */
        	buf[pos++] = 0; /* Some other stuff not supported */
        	buf[pos++] = 0; /* Some other stuff not supported (lock state + eject) */
        	buf[pos++] = 0; /* Some other stuff not supported */
        	buf[pos++] = (uint8_t) (CDROM_SPEED >> 8);
        	buf[pos++] = (uint8_t) CDROM_SPEED; /* Maximum speed */
        	buf[pos++] = 0; buf[pos++] = 2; /* Number of audio levels - on and off only */
        	buf[pos++] = 0; buf[pos++] = 0; /* Buffer size - none */
        	buf[pos++] = (uint8_t) (CDROM_SPEED >> 8);
        	buf[pos++] = (uint8_t) CDROM_SPEED; /* Current speed */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Drive digital format */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        }

	return pos;
}

uint32_t atapi_get_cd_channel(int channel)
{
	return (page_flags[GPMODE_CDROM_AUDIO_PAGE] & PAGE_CHANGED) ? mode_pages_in[GPMODE_CDROM_AUDIO_PAGE][channel ? 8 : 6] : (channel + 1);
}

uint32_t atapi_get_cd_volume(int channel)
{
	return (page_flags[GPMODE_CDROM_AUDIO_PAGE] & PAGE_CHANGED) ? mode_pages_in[GPMODE_CDROM_AUDIO_PAGE][channel ? 9 : 7] : 0xFF;
}

/*
 * Return the sector offset for the current register values
 */
static off64_t ide_get_sector(IDE *ide)
{
        if (ide->lba)
        {
                return (off64_t)ide->lba_addr + ide->skip512;
        }
        else
        {
#ifdef RPCEMU_IDE
        	int heads = ide->hpc;
        	int sectors = ide->spt;
#else
        	int heads = ide->hdd_file.hpc;
        	int sectors = ide->hdd_file.spt;
#endif
        	return ((((off64_t) ide->cylinder * heads) + ide->head) *
        	          sectors) + (ide->sector - 1) + ide->skip512;
        }
}

/**
 * Move to the next sector using CHS addressing
 */
static void ide_next_sector(IDE *ide)
{
        if (ide->lba)
        {
                ide->lba_addr++;
        }
        else
        {
        	ide->sector++;
#ifdef RPCEMU_IDE
        	if (ide->sector == (ide->spt + 1)) {
#else
        	if (ide->sector == (ide->hdd_file.spt + 1)) {
#endif
        		ide->sector = 1;
        		ide->head++;
#ifdef RPCEMU_IDE
        		if (ide->head == ide->hpc) {
#else
        		if (ide->head == ide->hdd_file.hpc) {
#endif
        			ide->head = 0;
        			ide->cylinder++;
                        }
		}
	}
}

#ifdef RPCEMU_IDE
static void loadhd(IDE *ide, int d, const char *fn)
{
	char pathname[512];

	append_filename(pathname, rpcemu_get_datadir(), fn, 512);

        rpclog("Loading %s\n",pathname);
	if (ide->hdfile == NULL) {
		/* Try to open existing hard disk image */
		ide->hdfile = fopen64(pathname, "rb+");
		if (ide->hdfile == NULL) {
			/* Failed to open existing hard disk image */
			if (errno == ENOENT) {
				/* Failed because it does not exist,
				   so try to create new file */
				ide->hdfile = fopen64(pathname, "wb+");
				if (ide->hdfile == NULL) {
					fatal("Cannot create file '%s': %s",
					      pathname, strerror(errno));
				}
			} else {
				/* Failed for another reason */
				fatal("Cannot open file '%s': %s",
				      pathname, strerror(errno));
			}
		}
	}

        fseek(ide->hdfile, 0xfc1, SEEK_SET);
        ide->spt = getc(ide->hdfile);
        ide->hpc = getc(ide->hdfile);
        ide->skip512 = 1;
//        rpclog("First check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
        if (!ide->spt || !ide->hpc)
        {
                fseek(ide->hdfile, 0xdc1, SEEK_SET);
                ide->spt = getc(ide->hdfile);
                ide->hpc = getc(ide->hdfile);
//                rpclog("Second check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                ide->skip512 = 0;
                if (!ide->spt || !ide->hpc)
                {
                        ide->spt=63;
                        ide->hpc=16;
                        ide->skip512 = 1;
//        rpclog("Final check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                }
        }

        ide->type = IDE_HDD;
        
        rpclog("%i %i %i\n",ide->spt,ide->hpc,ide->skip512);
}
#else
static void loadhd(IDE *ide, int d, const char *fn)
{
        hdd_load(&ide->hdd_file, d, fn);
        
        if (ide->hdd_file.f)
                ide->type = IDE_HDD;
        else
                ide->type = IDE_NONE;
}
#endif

void ide_set_signature(IDE *ide)
{
	ide->secount=1;
	ide->sector=1;
	ide->head=0;
	ide->cylinder=(IDE_DRIVE_IS_CDROM(ide) ? 0xEB14 : ((ide->type == IDE_HDD) ? 0 : 0xFFFF));
//	if (ide->type == IDE_HDD)  ide->drive = 0;
}

void resetide(void)
{
        int d;

        /* Close hard disk image files (if previously open) */
        for (d = 0; d < 4; d++) {
                ide_drives[d].type = IDE_NONE;
#ifdef RPCEMU_IDE
                if (ide_drives[d].hdfile != NULL) {
                        fclose(ide_drives[d].hdfile);
                        ide_drives[d].hdfile = NULL;
                }
#else
                hdd_close(&ide_drives[d].hdd_file);
#endif
                ide_drives[d].atastat = READY_STAT | DSC_STAT;
                ide_drives[d].service = 0;
                ide_drives[d].board = (d & 2) ? 1 : 0;
        }
		
	page_flags[GPMODE_CDROM_AUDIO_PAGE] &= 0xFD;		/* Clear changed flag for CDROM AUDIO mode page. */
	memset(mode_pages_in[GPMODE_CDROM_AUDIO_PAGE], 0, 256);	/* Clear the page itself. */

        idecallback[0]=idecallback[1]=0;
#ifdef RPCEMU_IDE
	loadhd(&ide_drives[0], 0, "hd4.hdf");
	if (!config.cdromenabled) {
		loadhd(&ide_drives[1], 1, "hd5.hdf");
	}
	else
           ide_drives[1].type = IDE_CDROM;
#else
	for (d = 0; d < 4; d++)
	{
	        ide_drives[d].drive = d;
		ide_drives[d].packetstatus = 0xFF;

		if ((cdrom_channel == d) && cdrom_enabled)
		{
			ide_drives[d].type = IDE_CDROM;
		}
		else
		{
			loadhd(&ide_drives[d], d, ide_fn[d]);
		}
			
		ide_set_signature(&ide_drives[d]);
	}
#endif

        cur_ide[0] = 0;
        cur_ide[1] = 2;
        
//        ide_drives[1].type = IDE_CDROM;

	page_flags[GPMODE_CDROM_AUDIO_PAGE] &= ~PAGE_CHANGED;
}

int idetimes=0;
void writeidew(int ide_board, uint16_t val)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        
        /*Some software issue excess writes after the 12 bytes required by the command, this will have all of them ignored*/
	if (ide->packetstatus && (ide->packetstatus != ATAPI_STATUS_PACKET_REQ))
                return;
#ifndef RPCEMU_IDE
/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed write IDE %04X:%08X\n",CS,pc);
                return;
        }*/
#endif
#ifdef _RPCEMU_BIG_ENDIAN
		val=(val>>8)|(val<<8);
#endif
//        pclog("Write IDEw %04X\n",val);
        ide->buffer[ide->pos >> 1] = val;
        ide->pos+=2;

        if (ide->packetstatus == ATAPI_STATUS_PACKET_REQ)
        {
		if ((ide->pos>=prefix_len+4) && (page_flags[page_current] & PAGE_CHANGEABLE))
		{
			mode_pages_in[page_current][ide->pos - prefix_len - 4] = ((uint8_t *) ide->buffer)[ide->pos - 2];
			mode_pages_in[page_current][ide->pos - prefix_len - 3] = ((uint8_t *) ide->buffer)[ide->pos - 1];
		}
                if (ide->pos>=(ide->packlen+2))
                {
                        ide->packetstatus = ATAPI_STATUS_PACKET_RECEIVED;
                        timer_process();
                        idecallback[ide_board]=6*IDE_TIME;
                        timer_update_outstanding();
//                        pclog("Packet over!\n");
                        ide_irq_lower(ide);
                }
                return;
        }
        else if (ide->packetstatus == ATAPI_STATUS_PACKET_RECEIVED)
                return;
        else if (ide->command == WIN_PACKETCMD && ide->pos>=0xC)
        {
                ide->pos=0;
                ide->atastat = BUSY_STAT;
                ide->packetstatus = ATAPI_STATUS_COMMAND;
/*                idecallback[ide_board]=6*IDE_TIME;*/
		timer_process();
                callbackide(ide_board);
                timer_update_outstanding();
//                idecallback[ide_board]=60*IDE_TIME;
//                if ((ide->buffer[0]&0xFF)==0x43) idecallback[ide_board]=1*IDE_TIME;
//                pclog("Packet now waiting!\n");
/*                if (ide->buffer[0]==0x243)
                {
                        idetimes++;
                        output=3;
                }*/
        }
        else if (ide->pos>=512)
        {
                ide->pos=0;
                ide->atastat = BUSY_STAT;
                timer_process();
                if (ide->command == WIN_WRITE_MULTIPLE)
                   callbackide(ide_board);
                else
              	   idecallback[ide_board]=6*IDE_TIME;
                timer_update_outstanding();
        }
}

void writeidel(int ide_board, uint32_t val)
{
//        pclog("WriteIDEl %08X\n", val);
        writeidew(ide_board, val);
        writeidew(ide_board, val >> 16);
}

void writeide(int ide_board, uint16_t addr, uint8_t val)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        IDE *ide_other = &ide_drives[cur_ide[ide_board] ^ 1];
#ifndef RPCEMU_IDE
/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed write IDE %04X:%08X\n",CS,pc);
                return;
        }*/
#endif
//        if ((cr0&1) && !(eflags&VM_FLAG))
//         pclog("WriteIDE %04X %02X from %04X(%08X):%08X %i\n", addr, val, CS, cs, pc, ins);
//        return;
        addr|=0x80;
//        if (ide_board) pclog("Write IDEb %04X %02X %04X(%08X):%04X %i  %02X %02X\n",addr,val,CS,cs,pc,ins,ide->atastat,ide_drives[0].atastat);
        /*if (idedebug) */
//        pclog("Write IDE %08X %02X %04X:%08X\n",addr,val,CS,pc);
//        int c;
//      rpclog("Write IDE %08X %02X %08X %08X\n",addr,val,PC,armregs[12]);

        if (ide->type == IDE_NONE && (addr == 0x1f0 || addr == 0x1f7)) return;
        
        switch (addr)
        {
        case 0x1F0: /* Data */
                writeidew(ide_board, val | (val << 8));
                return;

        case 0x1F1: /* Features */
                ide->cylprecomp = val;
                ide_other->cylprecomp = val;
                return;

        case 0x1F2: /* Sector count */
                ide->secount = val;
                ide_other->secount = val;
                return;

        case 0x1F3: /* Sector */
                ide->sector = val;
                ide->lba_addr = (ide->lba_addr & 0xFFFFF00) | val;
                ide_other->sector = val;
                ide_other->lba_addr = (ide_other->lba_addr & 0xFFFFF00) | val;
                return;

        case 0x1F4: /* Cylinder low */
                ide->cylinder = (ide->cylinder & 0xFF00) | val;
                ide->lba_addr = (ide->lba_addr & 0xFFF00FF) | (val << 8);
                ide_other->cylinder = (ide_other->cylinder&0xFF00) | val;
                ide_other->lba_addr = (ide_other->lba_addr&0xFFF00FF) | (val << 8);
//                pclog("Write cylinder low %02X\n",val);
                return;

        case 0x1F5: /* Cylinder high */
                ide->cylinder = (ide->cylinder & 0xFF) | (val << 8);
                ide->lba_addr = (ide->lba_addr & 0xF00FFFF) | (val << 16);
                ide_other->cylinder = (ide_other->cylinder & 0xFF) | (val << 8);
                ide_other->lba_addr = (ide_other->lba_addr & 0xF00FFFF) | (val << 16);
                return;

        case 0x1F6: /* Drive/Head */
/*        if (val==0xB0)
        {
                dumpregs();
                exit(-1);
        }*/
        
                if (cur_ide[ide_board] != ((val>>4)&1)+(ide_board<<1))
                {
                        cur_ide[ide_board]=((val>>4)&1)+(ide_board<<1);

                        if (ide->reset || ide_other->reset)
                        {
                                ide->atastat = ide_other->atastat = READY_STAT | DSC_STAT;
                                ide->error = ide_other->error = 1;
                                ide->secount = ide_other->secount = 1;
                                ide->sector = ide_other->sector = 1;
                                ide->head = ide_other->head = 0;
                                ide->cylinder = ide_other->cylinder = 0;
                                ide->reset = ide_other->reset = 0;
                                // ide->blocksize = ide_other->blocksize = 0;
                                if (IDE_DRIVE_IS_CDROM(ide))
                                        ide->cylinder=0xEB14;
                                if (IDE_DRIVE_IS_CDROM(ide_other))
                                        ide_other->cylinder=0xEB14;

                                idecallback[ide_board] = 0;
                                timer_update_outstanding();
                                return;
                        }

                        ide = &ide_drives[cur_ide[ide_board]];
                }
                                
                ide->head = val & 0xF;
                ide->lba = val & 0x40;
                ide_other->head = val & 0xF;
                ide_other->lba = val & 0x40;
                
                ide->lba_addr = (ide->lba_addr & 0x0FFFFFF) | ((val & 0xF) << 24);
                ide_other->lba_addr = (ide_other->lba_addr & 0x0FFFFFF)|((val & 0xF) << 24);
                                
                ide_irq_update(ide);
                return;

        case 0x1F7: /* Command register */
        if (ide->type == IDE_NONE) return;
//                pclog("IDE command %02X drive %i\n",val,ide.drive);
        ide_irq_lower(ide);
                ide->command=val;
                
//                pclog("New IDE command - %02X %i %i\n",ide->command,cur_ide[ide_board],ide_board);
                ide->error=0;
                switch (val)
                {
                case WIN_SRST: /* ATAPI Device Reset */
                        if (IDE_DRIVE_IS_CDROM(ide)) ide->atastat = BUSY_STAT;
                        else                         ide->atastat = READY_STAT;
                        timer_process();
                        idecallback[ide_board]=100*IDE_TIME;
                        timer_update_outstanding();
                        return;

                case WIN_RESTORE:
                case WIN_SEEK:
//                        pclog("WIN_RESTORE start\n");
                        ide->atastat = READY_STAT;
                        timer_process();
                        idecallback[ide_board]=100*IDE_TIME;
                        timer_update_outstanding();
                        return;

                case WIN_READ_MULTIPLE:
			if (!ide->blocksize && (ide->type != IDE_CDROM))
                           fatal("READ_MULTIPLE - blocksize = 0\n");
#if 0
                        if (ide->lba) pclog("Read Multiple %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Read Multiple %i sectors from sector %i cylinder %i head %i  %i\n",ide->secount,ide->sector,ide->cylinder,ide->head,ins);
#endif
                        ide->blockcount = 0;
                        
                case WIN_READ:
                case WIN_READ_NORETRY:
                case WIN_READ_DMA:
/*                        if (ide.secount>1)
                        {
                                fatal("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        }*/
#if 0
                        if (ide->lba) pclog("Read %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Read %i sectors from sector %i cylinder %i head %i  %i\n",ide->secount,ide->sector,ide->cylinder,ide->head,ins);
#endif
                        ide->atastat = BUSY_STAT;
                        timer_process();
                        idecallback[ide_board]=200*IDE_TIME;
                        timer_update_outstanding();
                        ide->do_initial_read = 1;
                        return;
                        
                case WIN_WRITE_MULTIPLE:
                        if (!ide->blocksize && (ide->type != IDE_CDROM))
                           fatal("Write_MULTIPLE - blocksize = 0\n");
#if 0
                        if (ide->lba) pclog("Write Multiple %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Write Multiple %i sectors to sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->blockcount = 0;
                        
                case WIN_WRITE:
                case WIN_WRITE_NORETRY:
                /*                        if (ide.secount>1)
                        {
                                fatal("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        }*/
#if 0
                        if (ide->lba) pclog("Write %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Write %i sectors to sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->atastat = DRQ_STAT | DSC_STAT | READY_STAT;
                        ide->pos=0;
                        return;

                case WIN_WRITE_DMA:
#if 0
                        if (ide->lba) pclog("Write %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Write %i sectors to sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->atastat = BUSY_STAT;
                        timer_process();
                        idecallback[ide_board]=200*IDE_TIME;
                        timer_update_outstanding();
                        return;

                case WIN_VERIFY:
                case WIN_VERIFY_ONCE:
#if 0
                        if (ide->lba) pclog("Read verify %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Read verify %i sectors from sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->atastat = BUSY_STAT;
                        timer_process();
                        idecallback[ide_board]=200*IDE_TIME;
                        timer_update_outstanding();
                        return;

                case WIN_FORMAT:
//                        pclog("Format track %i head %i\n",ide.cylinder,ide.head);
                        ide->atastat = DRQ_STAT;
//                        idecallback[ide_board]=200;
                        ide->pos=0;
                        return;

                case WIN_SPECIFY: /* Initialize Drive Parameters */
                        ide->atastat = BUSY_STAT;
                        timer_process();
                        idecallback[ide_board]=30*IDE_TIME;
                        timer_update_outstanding();
//                        pclog("SPECIFY\n");
//                        output=1;
                        return;

                case WIN_DRIVE_DIAGNOSTICS: /* Execute Drive Diagnostics */
                        ide->atastat = BUSY_STAT;
                        timer_process();
                        idecallback[ide_board]=200*IDE_TIME;
                        timer_update_outstanding();
                        return;

                case WIN_PIDENTIFY: /* Identify Packet Device */
                case WIN_SET_MULTIPLE_MODE: /*Set Multiple Mode*/
//                output=1;
                case WIN_SETIDLE1: /* Idle */
                        ide->atastat = BUSY_STAT;
                        timer_process();
                        callbackide(ide_board);
//                        idecallback[ide_board]=200*IDE_TIME;
                        timer_update_outstanding();
                        return;

                case WIN_IDENTIFY: /* Identify Device */
                case 0xEF:
//                        output=3;
//                        timetolive=500;
                        ide->atastat = BUSY_STAT;
                        timer_process();
                        idecallback[ide_board]=200*IDE_TIME;
                        timer_update_outstanding();
                        return;

                case WIN_PACKETCMD: /* ATAPI Packet */
                        ide->packetstatus = ATAPI_STATUS_IDLE;
                        ide->atastat = BUSY_STAT;
                        timer_process();
                        idecallback[ide_board]=1;//30*IDE_TIME;
                        timer_update_outstanding();
                        ide->pos=0;
                        return;
                        
                case 0xF0:
                        default:
                	ide->atastat = READY_STAT | ERR_STAT | DSC_STAT;
                	ide->error = ABRT_ERR;
                        ide_irq_raise(ide);
/*                        fatal("Bad IDE command %02X\n", val);*/
                        return;
                }
                
                return;

        case 0x3F6: /* Device control */
                if ((ide->fdisk&4) && !(val&4) && (ide->type != IDE_NONE || ide_other->type != IDE_NONE))
                {
			timer_process();
                        idecallback[ide_board]=500*IDE_TIME;
                        timer_update_outstanding();
                        ide->reset = ide_other->reset = 1;
                        ide->atastat = ide_other->atastat = BUSY_STAT;
//                        pclog("IDE Reset %i\n", ide_board);
                }
                if (val & 4)
                {
                        /*Drive held in reset*/
			timer_process();
                        idecallback[ide_board] = 0;
                        timer_update_outstanding();
                        ide->atastat = ide_other->atastat = BUSY_STAT;
                }
                ide->fdisk = ide_other->fdisk = val;
                ide_irq_update(ide);
                return;
        }
//        fatal("Bad IDE write %04X %02X\n", addr, val);
}

uint8_t readide(int ide_board, uint16_t addr)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        uint8_t temp = 0xff;
        uint16_t tempw;

        addr|=0x80;
#ifndef RPCEMU_IDE
/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed read IDE %04X:%08X\n",CS,pc);
                return 0xFF;
        }*/
#endif
//        if ((cr0&1) && !(eflags&VM_FLAG))
//         pclog("ReadIDE %04X  from %04X(%08X):%08X\n", addr, CS, cs, pc);
//        return 0xFF;

        if (ide->type == IDE_NONE && (addr == 0x1f0 || addr == 0x1f7)) return 0;
//        /*if (addr!=0x1F7 && addr!=0x3F6) */pclog("Read IDEb %04X %02X %02X %i %04X:%04X %i  %04X\n",addr,ide->atastat,(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0),cur_ide[ide_board],CS,pc,ide_board, BX);
//rpclog("Read IDE %08X %08X %02X\n",addr,PC,iomd.irqb.mask);
        switch (addr)
        {
        case 0x1F0: /* Data */
                tempw = readidew(ide_board);
//                pclog("Read IDEW %04X\n", tempw);                
                temp = tempw & 0xff;
                break;
                
        case 0x1F1: /* Error */
//        pclog("Read error %02X\n",ide.error);
                temp = ide->error;
                break;

        case 0x1F2: /* Sector count */
//        pclog("Read sector count %02X\n",ide->secount);
                temp = (uint8_t)ide->secount;
                break;

        case 0x1F3: /* Sector */
                temp = (uint8_t)ide->sector;
                break;

        case 0x1F4: /* Cylinder low */
//        pclog("Read cyl low %02X\n",ide.cylinder&0xFF);
                temp = (uint8_t)(ide->cylinder&0xFF);
                break;

        case 0x1F5: /* Cylinder high */
//        pclog("Read cyl low %02X\n",ide.cylinder>>8);
                temp = (uint8_t)(ide->cylinder>>8);
                break;

        case 0x1F6: /* Drive/Head */
                temp = (uint8_t)(ide->head | ((cur_ide[ide_board] & 1) ? 0x10 : 0) | (ide->lba ? 0x40 : 0) | 0xa0);
                break;

        case 0x1F7: /* Status */
                if (ide->type == IDE_NONE)
                {
//                        pclog("Return status 00\n");
                        temp = 0;
                        break;
                }
                ide_irq_lower(ide);
                if (ide->type == IDE_CDROM)
                {
//                        pclog("Read CDROM status %02X\n",(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0));
                        temp = (ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0);
                }
                else
                {
//                 && ide->service) return ide.atastat[ide.board]|SERVICE_STAT;
//                pclog("Return status %02X %04X:%04X %02X %02X\n",ide->atastat, CS ,pc, AH, BH);
                        temp = ide->atastat;
                }
                break;

        case 0x3F6: /* Alternate Status */
//        pclog("3F6 read %02X\n",ide.atastat[ide.board]);
//        if (output) output=0;
                if (ide->type == IDE_NONE)
                {
//                        pclog("Return status 00\n");
                        temp = 0;
                        break;
                }
                if (ide->type == IDE_CDROM)
                {
//                        pclog("Read CDROM status %02X\n",(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0));
                        temp = (ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0);
                }
                else
                {
//                 && ide->service) return ide.atastat[ide.board]|SERVICE_STAT;
//                pclog("Return status %02X\n",ide->atastat);
                        temp = ide->atastat;
                }
                break;
        }
//        if (ide_board) pclog("Read IDEb %04X %02X   %02X %02X %i %04X:%04X %i\n", addr, temp, ide->atastat,(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0),cur_ide[ide_board],CS,pc,ide_board);
        return temp;
//        fatal("Bad IDE read %04X\n", addr);
}

uint16_t readidew(int ide_board)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        uint16_t temp;
#ifndef RPCEMU_IDE
/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed read IDEw %04X:%08X\n",CS,pc);
                return 0xFFFF;
        }*/
#endif
//        return 0xFFFF;
//        pclog("Read IDEw %04X %04X:%04X %02X %i %i\n",ide->buffer[ide->pos >> 1],CS,pc,opcode,ins, ide->pos);
        
//if (idedebug) pclog("Read IDEW %08X\n",PC);

        temp = ide->buffer[ide->pos >> 1];
	#ifdef _RPCEMU_BIG_ENDIAN
		temp=(temp>>8)|(temp<<8);
	#endif
        ide->pos+=2;
	if ((ide->command == WIN_PACKETCMD) && ((ide->packetstatus == ATAPI_STATUS_REQ_SENSE) || (ide->packetstatus==8)))
	{
		callnonreadcd(ide);
		return temp;
	}
        if ((ide->pos>=512 && ide->command != WIN_PACKETCMD) || (ide->command == WIN_PACKETCMD && ide->pos>=ide->packlen))
        {
//                pclog("Over! packlen %i %i\n",ide->packlen,ide->pos);
                ide->pos=0;
                if (ide->command == WIN_PACKETCMD)// && ide.packetstatus==6)
                {
//                        pclog("Call readCD\n");
                        callreadcd(ide);
                }
                else
                {
                        ide->atastat = READY_STAT | DSC_STAT;
                        ide->packetstatus = ATAPI_STATUS_IDLE;
                        if (ide->command == WIN_READ || ide->command == WIN_READ_NORETRY || ide->command == WIN_READ_MULTIPLE)
                        {
                                ide->secount = (ide->secount - 1) & 0xff;
                                if (ide->secount)
                                {
                                        ide_next_sector(ide);
                                        ide->atastat = BUSY_STAT;
                                        timer_process();
                                        if (ide->command == WIN_READ_MULTIPLE)
                                           callbackide(ide_board);
                                        else
                                           idecallback[ide_board]=6*IDE_TIME;
                                        timer_update_outstanding();
//                                        pclog("set idecallback\n");
//                                        callbackide(ide_board);
                                }
//                                else
//                                   pclog("readidew done %02X\n", ide->atastat);
                        }
                }
        }
//        pclog("Read IDEw %04X\n",temp);
        return temp;
}

uint32_t readidel(int ide_board)
{
        uint16_t temp;
//        pclog("Read IDEl %i\n", ide_board);
        temp = readidew(ide_board);
        return temp | (readidew(ide_board) << 16);
}

int times30=0;
void callbackide(int ide_board)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        IDE *ide_other = &ide_drives[cur_ide[ide_board] ^ 1];
#if RPCEMU_IDE
        off64_t addr;
        int c;
#endif
        ext_ide = ide;
//        return;
        if (ide->command==0x30) times30++;
//        if (times30==2240) output=1;
        //if (times30==2471 && ide->command==0xA0) output=1;
///*if (ide_board) */pclog("CALLBACK %02X %i %i  %i\n",ide->command,times30,ide->reset,cur_ide[ide_board]);
//        if (times30==1294)
//                output=1;
        if (ide->reset)
        {
                ide->atastat = ide_other->atastat = READY_STAT | DSC_STAT;
                ide->error = ide_other->error = 1;
                ide->secount = ide_other->secount = 1;
                ide->sector = ide_other->sector = 1;
                ide->head = ide_other->head = 0;
                ide->cylinder = ide_other->cylinder = 0;
                ide->reset = ide_other->reset = 0;
                if (IDE_DRIVE_IS_CDROM(ide))
                {
                        ide->cylinder=0xEB14;
                        atapi->stop();
                }
                if (ide->type == IDE_NONE)
                {
                        ide->cylinder=0xFFFF;
                        atapi->stop();
                }
                if (IDE_DRIVE_IS_CDROM(ide_other))
                {
                        ide_other->cylinder=0xEB14;
                        atapi->stop();
                }
                if (ide_other->type == IDE_NONE)
                {
                        ide_other->cylinder=0xFFFF;
                        atapi->stop();
                }
//                pclog("Reset callback\n");
                return;
        }
        switch (ide->command)
        {
                //Initialize the Task File Registers as follows: Status = 00h, Error = 01h, Sector Count = 01h, Sector Number = 01h, Cylinder Low = 14h, Cylinder High =EBh and Drive/Head = 00h.
        case WIN_SRST: /*ATAPI Device Reset */
                ide->atastat = READY_STAT | DSC_STAT;
                ide->error=1; /*Device passed*/
                ide->secount = ide->sector = 1;
		ide_set_signature(ide);
		if (IDE_DRIVE_IS_CDROM(ide))
			ide->atastat = 0;
                ide_irq_raise(ide);
                if (IDE_DRIVE_IS_CDROM(ide))
                   ide->service = 0;
                return;

        case WIN_RESTORE:
        case WIN_SEEK:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        pclog("WIN_RESTORE callback on CD-ROM\n");
                        goto abort_cmd;
                }
//                pclog("WIN_RESTORE callback\n");
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);
                return;

        case WIN_READ:
        case WIN_READ_NORETRY:
                if (IDE_DRIVE_IS_CDROM(ide)) {
			ide_set_signature(ide);
                        goto abort_cmd;
                }
                if (ide->do_initial_read)
                {
                        ide->do_initial_read = 0;
                        ide->sector_pos = 0;
#ifdef RPCEMU_IDE
                        addr = ide_get_sector(ide) * 512;
                        fseeko64(ide->hdfile, addr, SEEK_SET);
                        if (ide->secount)
                                fread(ide->sector_buffer, ide->secount*512, 1, ide->hdfile);
                        else
                                fread(ide->sector_buffer, 256*512, 1, ide->hdfile);
#else
                        hdd_read_sectors(&ide->hdd_file, ide_get_sector(ide), ide->secount ? ide->secount : 256, ide->sector_buffer);
#endif
                }                        
                memcpy(ide->buffer, &ide->sector_buffer[ide->sector_pos*512], 512);
                ide->sector_pos++;
//                pclog("Read %i %i %i %08X\n",ide.cylinder,ide.head,ide.sector,addr);
                /*                if (ide.cylinder || ide.head)
                {
                        fatal("Read from other cylinder/head");
                }*/
                ide->pos=0;
                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
//                pclog("Read sector callback %i %i %i offset %08X %i left %i %02X\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt,ide.atastat[ide.board]);
//                if (addr) output=3;
                ide_irq_raise(ide);
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                return;

        case WIN_READ_DMA:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                if (ide->do_initial_read)
                {
                        ide->do_initial_read = 0;
                        ide->sector_pos = 0;
#ifdef RPCEMU_IDE
                        addr = ide_get_sector(ide) * 512;
                        fseeko64(ide->hdfile, addr, SEEK_SET);
                        if (ide->secount)
                                fread(ide->sector_buffer, ide->secount*512, 1, ide->hdfile);
                        else
                                fread(ide->sector_buffer, 256*512, 1, ide->hdfile);
#else
                        hdd_read_sectors(&ide->hdd_file, ide_get_sector(ide), ide->secount ? ide->secount : 256, ide->sector_buffer);
#endif
                }                        
                ide->pos=0;
                
                if (ide_bus_master_read_sector)
                {
                        if (ide_bus_master_read_sector(ide_board, &ide->sector_buffer[ide->sector_pos*512]))
                                idecallback[ide_board]=6*IDE_TIME;           /*DMA not performed, try again later*/
                        else
                        {
                                /*DMA successful*/
                                ide->sector_pos++;                                
                                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;

                                ide->secount = (ide->secount - 1) & 0xff;
                                if (ide->secount)
                                {
                                        ide_next_sector(ide);
                                        ide->atastat = BUSY_STAT;
                                        idecallback[ide_board]=6*IDE_TIME;
                                }
                                else
                                {
                                        ide_irq_raise(ide);
                                }
                        }
                }
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                return;

        case WIN_READ_MULTIPLE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                if (ide->do_initial_read)
                {
                        ide->do_initial_read = 0;
                        ide->sector_pos = 0;
#ifdef RPCEMU_IDE
                        addr = ide_get_sector(ide) * 512;
                        fseeko64(ide->hdfile, addr, SEEK_SET);
                        if (ide->secount)
                                fread(ide->sector_buffer, ide->secount*512, 1, ide->hdfile);
                        else
                                fread(ide->sector_buffer, 256*512, 1, ide->hdfile);
#else
                        hdd_read_sectors(&ide->hdd_file, ide_get_sector(ide), ide->secount ? ide->secount : 256, ide->sector_buffer);
#endif
                }                        
                memcpy(ide->buffer, &ide->sector_buffer[ide->sector_pos*512], 512);
                ide->sector_pos++;
                ide->pos=0;
                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                if (!ide->blockcount)// || ide->secount == 1)
                {
//                        pclog("Read multiple int\n");
                        ide_irq_raise(ide);
                }                        
                ide->blockcount++;
                if (ide->blockcount >= ide->blocksize)
                   ide->blockcount = 0;
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                return;

        case WIN_WRITE:
        case WIN_WRITE_NORETRY:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
#ifdef RPCEMU_IDE
                addr = ide_get_sector(ide) * 512;
//                pclog("Write sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                fseeko64(ide->hdfile, addr, SEEK_SET);
                fwrite(ide->buffer, 512, 1, ide->hdfile);
#else
                hdd_write_sectors(&ide->hdd_file, ide_get_sector(ide), 1, ide->buffer);
#endif
                ide_irq_raise(ide);
                ide->secount = (ide->secount - 1) & 0xff;
                if (ide->secount)
                {
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                        ide->pos=0;
                        ide_next_sector(ide);
                }
                else
                   ide->atastat = READY_STAT | DSC_STAT;
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                return;
                
        case WIN_WRITE_DMA:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }

                if (ide_bus_master_write_sector)
                {
                        if (ide_bus_master_write_sector(ide_board, (uint8_t *)ide->buffer))
                           idecallback[ide_board]=6*IDE_TIME;           /*DMA not performed, try again later*/
                        else
                        {
                                /*DMA successful*/
#ifdef RPCEMU_IDE
                                addr = ide_get_sector(ide) * 512;
                                fseeko64(ide->hdfile, addr, SEEK_SET);
                                fwrite(ide->buffer, 512, 1, ide->hdfile);
#else
                                hdd_write_sectors(&ide->hdd_file, ide_get_sector(ide), 1, ide->buffer);
#endif
                                
                                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;

                                ide->secount = (ide->secount - 1) & 0xff;
                                if (ide->secount)
                                {
                                        ide_next_sector(ide);
                                        ide->atastat = BUSY_STAT;
                                        idecallback[ide_board]=6*IDE_TIME;
                                }
                                else
                                {
                                        ide_irq_raise(ide);
                                }
                        }
                }
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                return;

        case WIN_WRITE_MULTIPLE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
#ifdef RPCEMU_IDE
                addr = ide_get_sector(ide) * 512;
//                pclog("Write sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                fseeko64(ide->hdfile, addr, SEEK_SET);
                fwrite(ide->buffer, 512, 1, ide->hdfile);
#else
                hdd_write_sectors(&ide->hdd_file, ide_get_sector(ide), 1, ide->buffer);
#endif
                ide->blockcount++;
                if (ide->blockcount >= ide->blocksize || ide->secount == 1)
                {
                        ide->blockcount = 0;
                        ide_irq_raise(ide);
                }
                ide->secount = (ide->secount - 1) & 0xff;
                if (ide->secount)
                {
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                        ide->pos=0;
                        ide_next_sector(ide);
                }
                else
                   ide->atastat = READY_STAT | DSC_STAT;
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                return;

        case WIN_VERIFY:
        case WIN_VERIFY_ONCE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                ide->pos=0;
                ide->atastat = READY_STAT | DSC_STAT;
//                pclog("Read verify callback %i %i %i offset %08X %i left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                ide_irq_raise(ide);
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                return;

        case WIN_FORMAT:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
#ifdef RPCEMU_IDE
                addr = ide_get_sector(ide) * 512;
//                pclog("Format cyl %i head %i offset %08X %08X %08X secount %i\n",ide.cylinder,ide.head,addr,addr>>32,addr,ide.secount);
                fseeko64(ide->hdfile, addr, SEEK_SET);
                memset(ide->buffer, 0, 512);
                for (c=0;c<ide->secount;c++)
                {
                        fwrite(ide->buffer, 512, 1, ide->hdfile);
                }
#else
                hdd_format_sectors(&ide->hdd_file, ide_get_sector(ide), ide->secount);
#endif
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                return;

        case WIN_DRIVE_DIAGNOSTICS:
		ide_set_signature(ide);
                ide->error=1; /*No error detected*/
		if (IDE_DRIVE_IS_CDROM(ide))
		{
			ide->atastat = 0;
		}
		else
		{
			ide->atastat = READY_STAT | DSC_STAT;
			ide_irq_raise(ide);
		}
                return;

        case WIN_SPECIFY: /* Initialize Drive Parameters */
                if (IDE_DRIVE_IS_CDROM(ide)) {
#ifndef RPCEMU_IDE
                        pclog("IS CDROM - ABORT\n");
#endif
                        goto abort_cmd;
                }
#if RPCEMU_IDE
                ide->spt = ide->secount;
                ide->hpc = ide->head+1;
#else
                ide->hdd_file.spt = ide->secount;
                ide->hdd_file.hpc = ide->head+1;
#endif
                ide->atastat = READY_STAT | DSC_STAT;
#ifndef RPCEMU_IDE
//                pclog("SPECIFY - %i sectors per track, %i heads per cylinder  %i %i\n",ide->spt,ide->hpc,cur_ide[ide_board],ide_board);
#endif
                ide_irq_raise(ide);
                return;

        case WIN_PIDENTIFY: /* Identify Packet Device */
                if (IDE_DRIVE_IS_CDROM(ide)) {
//                        pclog("ATAPI identify\n");
                        ide_atapi_identify(ide);
                        ide->pos=0;
                        ide->error=0;
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                        ide_irq_raise(ide);
                        return;
                }
//                pclog("Not ATAPI\n");
                goto abort_cmd;

        case WIN_SET_MULTIPLE_MODE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
#ifndef RPCEMU_IDE
                        pclog("IS CDROM - ABORT\n");
#endif
                        goto abort_cmd;
                }
                ide->blocksize = ide->secount;
                ide->atastat = READY_STAT | DSC_STAT;
#ifndef RPCEMU_IDE
                pclog("Set multiple mode - %i\n", ide->blocksize);
#endif
                ide_irq_raise(ide);
                return;
                
        case WIN_SETIDLE1: /* Idle */
        case 0xEF:
                goto abort_cmd;

        case WIN_IDENTIFY: /* Identify Device */
		if (ide->type == IDE_NONE)
		{
			goto abort_cmd;
		}
                if (IDE_DRIVE_IS_CDROM(ide))
		{
			ide_set_signature(ide);
			goto abort_cmd;
		}
		else
		{
                        ide_identify(ide);
                        ide->pos=0;
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
//                pclog("ID callback\n");
                        ide_irq_raise(ide);
                }
                return;

        case WIN_PACKETCMD: /* ATAPI Packet */
                if (!IDE_DRIVE_IS_CDROM(ide)) goto abort_cmd;
//                pclog("Packet callback! %i %08X\n",ide->packetstatus,ide);

                if (ide->packetstatus == ATAPI_STATUS_IDLE)
                {
			readcdmode=0;
                        ide->pos=0;
                        ide->secount = (uint8_t)((ide->secount&0xF8)|1);
                        ide->atastat = READY_STAT | DRQ_STAT |(ide->atastat&ERR_STAT);
                        //ide_irq_raise(ide);
//                        pclog("1 Preparing to recieve packet max DRQ count %04X\n",ide->cylinder);
                }
                else if (ide->packetstatus == ATAPI_STATUS_COMMAND)
                {
                        ide->atastat = BUSY_STAT|(ide->atastat&ERR_STAT);
//                        pclog("Running ATAPI command 2\n");
                        atapicommand(ide_board);
//                        exit(-1);
                }
                else if (ide->packetstatus == ATAPI_STATUS_COMPLETE)
                {
//                        pclog("packetstatus==2\n");
                        ide->atastat = READY_STAT;
                        ide->secount=3;
                        ide_irq_raise(ide);
//                        if (iomd.irqb.mask&2) output=1;
                }
                else if (ide->packetstatus == ATAPI_STATUS_DATA)
                {
                        ide->atastat = READY_STAT|DRQ_STAT|(ide->atastat&ERR_STAT);
//                        rpclog("Recieve data packet 3! %02X\n",ide->atastat);
                        ide_irq_raise(ide);
                        ide->packetstatus=0xFF;
                }
                else if (ide->packetstatus == ATAPI_STATUS_PACKET_REQ)
                {
                        ide->atastat = 0x58 | (ide->atastat & ERR_STAT);
//                        pclog("Send data packet 4!\n");
                        ide_irq_raise(ide);
//                        ide.packetstatus=5;
                        ide->pos=2;
                }
                else if (ide->packetstatus == ATAPI_STATUS_PACKET_RECEIVED)
                {
//                        pclog("Packetstatus 5 !\n");
                        atapicommand(ide_board);
                }
                else if (ide->packetstatus == ATAPI_STATUS_READCD) /*READ CD callback*/
                {
                        ide->atastat = DRQ_STAT|(ide->atastat&ERR_STAT);
//                        pclog("Recieve data packet 6!\n");
                        ide_irq_raise(ide);
//                        ide.packetstatus=0xFF;
                }
		else if (ide->packetstatus == ATAPI_STATUS_REQ_SENSE)	/*REQUEST SENSE callback #1*/
		{
			pclog("REQUEST SENSE callback #1: setting status to 0x5A\n");
			ide->atastat = 0x58 | (ide->atastat & ERR_STAT);
			ide_irq_raise(ide);
		}
                else if (ide->packetstatus == ATAPI_STATUS_ERROR) /*Error callback*/
                {
//                        pclog("Packet error\n");
                        ide->atastat = READY_STAT | ERR_STAT;
                        ide_irq_raise(ide);
                }
                else if (ide->packetstatus == ATAPI_STATUS_ERROR_2) /*Error callback with atastat already set - needed for the disc change stuff.*/
                {
                        //pclog("Packet check status\n");
                        ide->atastat = ERR_STAT;
                        ide_irq_raise(ide);
                }
                return;
        }

abort_cmd:
	ide->command = 0;
	ide->atastat = READY_STAT | ERR_STAT | DSC_STAT;
	ide->error = ABRT_ERR;
	ide->pos = 0;
	ide_irq_raise(ide);
}

void ide_callback_pri()
{
	idecallback[0] = 0;
	callbackide(0);
}

void ide_callback_sec()
{
	idecallback[1] = 0;
	callbackide(1);
}

/*ATAPI CD-ROM emulation*/

struct
{
        int sensekey,asc,ascq;
} atapi_sense;

static uint32_t atapi_event_status(IDE *ide, uint8_t *buffer)
{
	uint8_t event_code, media_status = 0;

	if (buffer[5])
	{
		media_status = MS_TRAY_OPEN;
		atapi->stop();
	}
        else
	{
		media_status = MS_MEDIA_PRESENT;
	}
	
	event_code = MEC_NO_CHANGE;
	if (media_status != MS_TRAY_OPEN)
	{
		if (!buffer[4])
		{
			event_code = MEC_NEW_MEDIA;
			atapi->load();
		}
		else if (buffer[4]==2)
		{
			event_code = MEC_EJECT_REQUESTED;
			atapi->eject();
		}
	}
	
	buffer[4] = event_code;
	buffer[5] = media_status;
	buffer[6] = 0;
	buffer[7] = 0;
	
	return 8;
}

void atapi_cmd_error(IDE *ide, uint8_t sensekey, uint8_t asc)
{
	ide->error = (sensekey << 4);
	ide->atastat = READY_STAT | ERR_STAT;
	ide->secount = (ide->secount & ~7) | 3;
	ide->packetstatus = 0x80;
	idecallback[ide->board]=50*IDE_TIME;
}

uint8_t atapi_prev;
int toctimes=0;

void atapi_insert_cdrom()
{
	atapi_sense.sensekey=SENSE_UNIT_ATTENTION;
	atapi_sense.asc=ASC_MEDIUM_MAY_HAVE_CHANGED;
	atapi_sense.ascq=0;
}

void atapi_command_send_init(IDE *ide, uint8_t command, int req_length, int alloc_length)
{
	if (ide->cylinder == 0xffff)
		ide->cylinder = 0xfffe;

	if ((ide->cylinder & 1) && !(alloc_length <= ide->cylinder))
	{
		pclog("Odd byte count (0x%04x) to ATAPI command 0x%02x, using 0x%04x\n", ide->cylinder, command, ide->cylinder - 1);
		ide->cylinder--;
	}
	
	if (alloc_length < 0)
		fatal("Allocation length < 0\n");
	if (alloc_length == 0)
		alloc_length = ide->cylinder;
		
	// Status: 0x80 (busy), 0x08 (drq), 0x01 (err)
	// Interrupt: 0x02 (i_o), 0x01 (c_d)
	/* No atastat setting: PCem actually emulates the callback cycle. */
	ide->secount = 2;
	
	// no bytes transferred yet
	ide->pos = 0;

	if ((ide->cylinder > req_length) || (ide->cylinder == 0))
		ide->cylinder = req_length;
	if (ide->cylinder > alloc_length)
		ide->cylinder = alloc_length;

	//pclog("atapi_command_send_init(ide, %02X, %04X, %04X)\n", command, req_length, alloc_length);
	//pclog("IDE settings: Pos=%08X, Secount=%08X, Cylinder=%08X\n", ide->pos, ide->secount, ide->cylinder);
}

static void atapi_command_ready(int ide_board, int packlen)
{
	IDE *ide = &ide_drives[cur_ide[ide_board]];
	ide->packetstatus = ATAPI_STATUS_REQ_SENSE;
	idecallback[ide_board]=60*IDE_TIME;
	ide->packlen=packlen;
}

static void atapi_sense_clear(int command, int ignore_ua)
{
	if ((atapi_sense.sensekey == SENSE_UNIT_ATTENTION) || ignore_ua)
	{
		atapi_prev=command;
		atapi_sense.sensekey=0;
		atapi_sense.asc=0;
		atapi_sense.ascq=0;
	}
}

int cd_status = CD_STATUS_EMPTY;
int prev_status;

static void atapicommand(int ide_board)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        uint8_t *idebufferb = (uint8_t *) ide->buffer;
	uint8_t rcdmode = 0;
        int c;
        int len;
        int msf;
        int pos=0;
        unsigned char temp;
        uint32_t size;
	int is_error;
	uint8_t page_code;
	int max_len;
	unsigned idx = 0;
	unsigned size_idx;
	unsigned preamble_len;
	int toc_format;
	int temp_command;
	int alloc_length;
	int completed;

#ifndef RPCEMU_IDE
        pclog("New ATAPI command %02X %i\n",idebufferb[0],ins);
#endif
//        readflash_set(READFLASH_HDD, ide->drive, 1);
        msf=idebufferb[1]&2;
        ide->cdlen=0;

	is_error = 0;

	if (atapi->medium_changed())
	{
		atapi_insert_cdrom();
	}
	/*If UNIT_ATTENTION is set, error out with NOT_READY.
	  VIDE-CDD.SYS will then issue a READ_TOC, which can pass through UNIT_ATTENTION and will clear sense.
	  NT 3.1 / AZTIDECD.SYS will then issue a REQUEST_SENSE, which can also pass through UNIT_ATTENTION but will clear sense AFTER sending it back.
	  In any case, if the command cannot pass through, set our state to errored.*/
	if (!(atapi_cmd_table[idebufferb[0]] & ALLOW_UA) && atapi_sense.sensekey == SENSE_UNIT_ATTENTION)
	{
		atapi_cmd_error(ide, atapi_sense.sensekey, atapi_sense.asc);
		is_error = 1;
	}
	/*Unless the command issued was a REQUEST_SENSE or TEST_UNIT_READY, clear sense.
          This is important because both VIDE-CDD.SYS and NT 3.1 / AZTIDECD.SYS rely on this behaving VERY specifically.
          VIDE-CDD.SYS will clear sense through READ_TOC, while NT 3.1 / AZTIDECD.SYS will issue a REQUEST_SENSE.*/
        if ((idebufferb[0]!=GPCMD_REQUEST_SENSE) && (idebufferb[0]!=GPCMD_TEST_UNIT_READY))
	{
		/* GPCMD_TEST_UNIT_READY is NOT supposed to clear sense! */
		atapi_sense_clear(idebufferb[0], 1);
	}

	/*If our state has been set to errored, clear it, and return.*/
	if (is_error)
	{
		is_error = 0;
		return;
	}
		
	if ((atapi_cmd_table[idebufferb[0]] & CHECK_READY) && !atapi->ready())
	{
		atapi_cmd_error(ide, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
		atapi_sense.sensekey = SENSE_NOT_READY;
		atapi_sense.asc = ASC_MEDIUM_NOT_PRESENT;
		atapi_sense.ascq = 0;
		return;
	}

	prev_status = cd_status;
	cd_status = atapi->status();
	if (((prev_status == CD_STATUS_PLAYING) || (prev_status == CD_STATUS_PAUSED)) && ((cd_status != CD_STATUS_PLAYING) && (cd_status != CD_STATUS_PAUSED)))
	{
		completed = 1;
	}
	else
	{
		completed = 0;
	}

        switch (idebufferb[0])
        {
                case GPCMD_TEST_UNIT_READY:
		ide->packetstatus = ATAPI_STATUS_COMPLETE;
		idecallback[ide_board]=50*IDE_TIME;
                break;

                case GPCMD_REQUEST_SENSE: /* Used by ROS 4+ */
		alloc_length = idebufferb[4];
		temp_command = idebufferb[0];
		atapi_command_send_init(ide, temp_command, 18, alloc_length);
				
                /*Will return 18 bytes of 0*/
                memset(idebufferb,0,512);

		idebufferb[0]=0x80|0x70;

		if ((atapi_sense.sensekey > 0) || (cd_status < CD_STATUS_PLAYING))
		{
			if (completed)
			{
				idebufferb[2]=SENSE_ILLEGAL_REQUEST;
				idebufferb[12]=ASC_AUDIO_PLAY_OPERATION;
				idebufferb[13]=ASCQ_AUDIO_PLAY_OPERATION_COMPLETED;
			}
			else
			{
				idebufferb[2]=atapi_sense.sensekey;
				idebufferb[12]=atapi_sense.asc;
				idebufferb[13]=atapi_sense.ascq;
			}
		}
		else
		{
			idebufferb[2]=SENSE_ILLEGAL_REQUEST;
			idebufferb[12]=ASC_AUDIO_PLAY_OPERATION;
			idebufferb[13]=(cd_status == CD_STATUS_PLAYING) ? ASCQ_AUDIO_PLAY_OPERATION_IN_PROGRESS : ASCQ_AUDIO_PLAY_OPERATION_PAUSED;
		}

		idebufferb[7]=10;

		// pclog("REQUEST SENSE start\n");
		atapi_command_ready(ide_board, 18);

		/* Clear the sense stuff as per the spec. */
		atapi_sense_clear(temp_command, 0);
                break;

                case GPCMD_SET_SPEED:
                ide->packetstatus = ATAPI_STATUS_COMPLETE;
                idecallback[ide_board]=50*IDE_TIME;
                break;

		case GPCMD_MECHANISM_STATUS: /*0xbd*/
		alloc_length = (idebufferb[7]<<16)|(idebufferb[8]<<8)|idebufferb[9];

		if (alloc_length == 0)
			fatal("Zero allocation length to MECHANISM STATUS not impl.\n");

		atapi_command_send_init(ide, idebufferb[0], 8, alloc_length);
		
		idebufferb[0] = 0;
		idebufferb[1] = 0;
		idebufferb[2] = 0;
		idebufferb[3] = 0;
		idebufferb[4] = 0;
		idebufferb[5] = 1;
		idebufferb[6] = 0;
		idebufferb[7] = 0;
		// len = 8;
			
		atapi_command_ready(ide_board, 8);
		break;	
		
                case GPCMD_READ_TOC_PMA_ATIP:
//                pclog("Read TOC ready? %08X\n",ide);
                toctimes++;
//                if (toctimes==2) output=3;
//                pclog("Read TOC %02X\n",idebufferb[9]);
		toc_format = idebufferb[2] & 0xf;
		if (toc_format == 0)
			toc_format = (idebufferb[9]>>6) & 3;
                switch (toc_format)
                {
                        case 0: /*Normal*/
			// pclog("ATAPI: READ TOC type requested: Normal\n");
                        len=idebufferb[8]+(idebufferb[7]<<8);
                        len=atapi->readtoc(idebufferb,idebufferb[6],msf,len,0);
                        break;
                        case 1: /*Multi session*/
			// pclog("ATAPI: READ TOC type requested: Multi-session\n");
                        len=idebufferb[8]+(idebufferb[7]<<8);
                        len=atapi->readtoc_session(idebufferb,msf,len);
                        idebufferb[0]=0; idebufferb[1]=0xA;
                        break;
			case 2: /*Raw*/
			// pclog("ATAPI: READ TOC type requested: Raw TOC\n");
			len=idebufferb[8]+(idebufferb[7]<<8);
			len=atapi->readtoc_raw(idebufferb,len);
			break;
                        default:
			// pclog("ATAPI: Unknown READ TOC type requested: %i\n", (idebufferb[9]>>6));
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (atapi_sense.sensekey == SENSE_UNIT_ATTENTION)
                                ide->error |= MCR_ERR;
                        ide->packetstatus = ATAPI_STATUS_ERROR;
                        idecallback[ide_board]=50*IDE_TIME;
                        return;
/*                        pclog("Bad read TOC format\n");
                        pclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            pclog("%02X ",idebufferb[c]);
                        pclog("\n");
                        exit(-1);*/
                }
//                pclog("ATAPI buffer len %i\n",len);
                ide->packetstatus = ATAPI_STATUS_DATA;
                ide->cylinder=len;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=len;
                return;
                
        case GPCMD_READ_CD:
//                pclog("Read CD : start LBA %02X%02X%02X%02X Length %02X%02X%02X Flags %02X\n",idebufferb[2],idebufferb[3],idebufferb[4],idebufferb[5],idebufferb[6],idebufferb[7],idebufferb[8],idebufferb[9]);
		rcdmode = idebufferb[9] & 0xF8;
                if ((rcdmode != 0x10) && (rcdmode != 0xF8))
                {
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (atapi_sense.sensekey == SENSE_UNIT_ATTENTION)
                                ide->error |= MCR_ERR;
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus = ATAPI_STATUS_ERROR;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
//                        pclog("Bad flags bits %02X\n",idebufferb[9]);
//                        exit(-1);
                }
/*                if (idebufferb[6] || idebufferb[7] || (idebufferb[8]!=1))
                {
                        pclog("More than 1 sector!\n");
                        exit(-1);
                }*/
                ide->cdlen=(idebufferb[6]<<16)|(idebufferb[7]<<8)|idebufferb[8];
                ide->cdpos=(idebufferb[2]<<24)|(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
//                pclog("Read at %08X %08X\n",ide.cdpos,ide.cdpos*2048);
		if (rcdmode == 0x10)
                        atapi->readsector(idebufferb,ide->cdpos);
		else
	                atapi->readsector_raw(idebufferb,ide->cdpos);
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
		readcdmode = (rcdmode == 0xF8);
                ide->cdpos++;
                ide->cdlen--;
                if (ide->cdlen >= 0)
                        ide->packetstatus = ATAPI_STATUS_READCD;
                else
                        ide->packetstatus = ATAPI_STATUS_DATA;
                ide->cylinder=(idebufferb[9] == 0x10) ? 2048 : 2352;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=(idebufferb[9] == 0x10) ? 2048 : 2352;
                return;

		case GPCMD_READ_6:
		case GPCMD_READ_10:
		case GPCMD_READ_12:
//                pclog("Read 10 : start LBA %02X%02X%02X%02X Length %02X%02X%02X Flags %02X\n",idebufferb[2],idebufferb[3],idebufferb[4],idebufferb[5],idebufferb[6],idebufferb[7],idebufferb[8],idebufferb[9]);

		readcdmode = 0;

		if (idebufferb[0] == GPCMD_READ_6)
		{
			ide->cdlen=idebufferb[4];
			ide->cdpos=((((uint32_t) idebufferb[1]) & 0x1f)<<16)|(((uint32_t) idebufferb[2])<<8)|((uint32_t) idebufferb[3]);
		}
		else if (idebufferb[0] == GPCMD_READ_10)
		{
			ide->cdlen=(idebufferb[7]<<8)|idebufferb[8];
			ide->cdpos=(idebufferb[2]<<24)|(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
		}
		else
		{
			ide->cdlen=(((uint32_t) idebufferb[6])<<24)|(((uint32_t) idebufferb[7])<<16)|(((uint32_t) idebufferb[8])<<8)|((uint32_t) idebufferb[9]);
			ide->cdpos=(((uint32_t) idebufferb[2])<<24)|(((uint32_t) idebufferb[3])<<16)|(((uint32_t) idebufferb[4])<<8)|((uint32_t) idebufferb[5]);
		}
                if (!ide->cdlen)
                {
//                        pclog("All done - callback set\n");
                        ide->packetstatus = ATAPI_STATUS_COMPLETE;
                        idecallback[ide_board]=20*IDE_TIME;
                        break;
                }

                if (atapi->readsector(idebufferb,ide->cdpos))
                {
//                        pclog("Read sector failed\n");
        		atapi_sense.sensekey = SENSE_ILLEGAL_REQUEST;
        		atapi_sense.asc = ASC_LBA_OUT_OF_RANGE;
        		atapi_sense.ascq = 0;

                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (atapi_sense.sensekey == SENSE_UNIT_ATTENTION)
                        {
                                ide->error |= MCR_ERR;
                        }
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus = ATAPI_STATUS_ERROR;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
                }
#ifndef RPCEMU_IDE
                readflash_set(READFLASH_HDC, ide->drive);
#endif
                ide->cdpos++;
                ide->cdlen--;
                if (ide->cdlen >= 0)
                        ide->packetstatus = ATAPI_STATUS_READCD;
                else
                        ide->packetstatus = ATAPI_STATUS_DATA;
                ide->cylinder=2048;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=2048;
                return;

        case GPCMD_READ_HEADER:
                if (msf)
                {
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (atapi_sense.sensekey == SENSE_UNIT_ATTENTION)
                        {
                                ide->error |= MCR_ERR;
                        }
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus = ATAPI_STATUS_ERROR;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
//                        pclog("Read Header MSF!\n");
//                        exit(-1);
                }
                for (c=0;c<4;c++) idebufferb[c+4]=idebufferb[c+2];
                idebufferb[0]=1; /*2048 bytes user data*/
                idebufferb[1]=idebufferb[2]=idebufferb[3]=0;
                
                ide->packetstatus = ATAPI_STATUS_DATA;
                ide->cylinder=8;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=8;
                return;
                
		case GPCMD_MODE_SENSE_6:
		case GPCMD_MODE_SENSE_10:
		temp_command = idebufferb[0];
		
		if (temp_command == GPCMD_MODE_SENSE_6)
			len=idebufferb[4];
		else
                        len=(idebufferb[8]|(idebufferb[7]<<8));

                temp=idebufferb[2] & 0x3F;

		memset(idebufferb, 0, len);
		alloc_length = len;
		// for (c=0;c<len;c++) idebufferb[c]=0;
		if (!(mode_sense_pages[temp] & IMPLEMENTED))
		{
                        // ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        idecallback[ide_board]=50*IDE_TIME;
			atapi_cmd_error(ide, atapi_sense.sensekey, atapi_sense.asc);
			ide->atastat = 0x53;
                        ide->packetstatus = ATAPI_STATUS_ERROR;
			atapi_sense.sensekey = SENSE_ILLEGAL_REQUEST;
                        atapi_sense.asc = ASC_INV_FIELD_IN_CMD_PACKET;
			atapi_sense.ascq = 0;
                        return;
		}
			
		if (temp_command == GPCMD_MODE_SENSE_6)
		{
			len = ide_atapi_mode_sense(ide,4,temp);
			idebufferb[0] = len - 1;
			idebufferb[1]=3; /*120mm data CD-ROM*/
		}
        	else
		{
			len = ide_atapi_mode_sense(ide,8,temp);
			idebufferb[0]=(len - 2)>>8;
			idebufferb[1]=(len - 2)&255;
			idebufferb[2]=3; /*120mm data CD-ROM*/
		}				

		atapi_command_send_init(ide, temp_command, len, alloc_length);

		atapi_command_ready(ide_board, len);
		return;

		case GPCMD_MODE_SELECT_6:
                case GPCMD_MODE_SELECT_10:
                if (ide->packetstatus == ATAPI_STATUS_PACKET_RECEIVED)
                {
                        ide->atastat = READY_STAT;
                        ide->secount=3;
//                        pclog("Recieve data packet!\n");
                        ide_irq_raise(ide);
                        ide->packetstatus=0xFF;
                        ide->pos=0;
  //                      pclog("Length - %02X%02X\n",idebufferb[0],idebufferb[1]);
//                        pclog("Page %02X length %02X\n",idebufferb[8],idebufferb[9]);
                }
                else
                {
			if (idebufferb[0] == GPCMD_MODE_SELECT_6)
			{
				len=idebufferb[4];
				prefix_len = 6;
			}
			else
			{
				len=(idebufferb[7]<<8)|idebufferb[8];
				prefix_len = 10;
			}
			page_current = idebufferb[2];
			if (page_flags[page_current] & PAGE_CHANGEABLE)
                                page_flags[GPMODE_CDROM_AUDIO_PAGE] |= PAGE_CHANGED;
                        ide->packetstatus = ATAPI_STATUS_PACKET_REQ;
                        ide->cylinder=len;
                        ide->secount=0;
                        ide->pos=0;
                        idecallback[ide_board]=60*IDE_TIME;
                        ide->packlen=len;
/*                        pclog("Waiting for ARM to send packet %i\n",len);
                pclog("Packet data :\n");
                for (c=0;c<12;c++)
                    pclog("%02X ",idebufferb[c]);
                    pclog("\n");*/
                }
                return;

                case GPCMD_GET_EVENT_STATUS_NOTIFICATION: /*0x4a*/
                temp_command = idebufferb[0];
                alloc_length = (idebufferb[7]<<16)|(idebufferb[8]<<8)|idebufferb[9];
                
                {
                        struct __attribute__((__packed__))
                        {
                                uint8_t opcode;
                                uint8_t polled;
                                uint8_t reserved2[2];
                                uint8_t class;
                                uint8_t reserved3[2];
                                uint16_t len;
                                uint8_t control;
                        } *gesn_cdb;
            
                        struct __attribute__((__packed__))
                        {
                                uint16_t len;
                                uint8_t notification_class;
                                uint8_t supported_events;
                        } *gesn_event_header;
                        unsigned int used_len;
            
                        gesn_cdb = (void *)idebufferb;
                        gesn_event_header = (void *)idebufferb;
            
                        /* It is fine by the MMC spec to not support async mode operations */
                        if (!(gesn_cdb->polled & 0x01))
                        {   /* asynchronous mode */
                                /* Only pollign is supported, asynchronous mode is not. */
                                ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                                idecallback[ide_board]=50*IDE_TIME;
                                atapi_cmd_error(ide, atapi_sense.sensekey, atapi_sense.asc);
                                ide->atastat = 0x53;
                                ide->packetstatus=0x80;
                                atapi_sense.sensekey = SENSE_ILLEGAL_REQUEST;
                                atapi_sense.asc = ASC_INV_FIELD_IN_CMD_PACKET;
                                atapi_sense.ascq = 0;
                                return;
                        }
            
                        /* polling mode operation */
            
                        /*
                         * These are the supported events.
                         *
                         * We currently only support requests of the 'media' type.
                         * Notification class requests and supported event classes are bitmasks,
                         * but they are built from the same values as the "notification class"
                         * field.
                         */
                        gesn_event_header->supported_events = 1 << GESN_MEDIA;
            
                        /*
                         * We use |= below to set the class field; other bits in this byte
                         * are reserved now but this is useful to do if we have to use the
                         * reserved fields later.
                         */
                        gesn_event_header->notification_class = 0;
            
                        /*
                         * Responses to requests are to be based on request priority.  The
                         * notification_class_request_type enum above specifies the
                         * priority: upper elements are higher prio than lower ones.
                         */
                        if (gesn_cdb->class & (1 << GESN_MEDIA))
                        {
                                gesn_event_header->notification_class |= GESN_MEDIA;
                                used_len = atapi_event_status(ide, idebufferb);
                        }
                        else
                        {
                                gesn_event_header->notification_class = 0x80; /* No event available */
                                used_len = sizeof(*gesn_event_header);
                        }
                        len = used_len;
                        gesn_event_header->len = used_len - sizeof(*gesn_event_header);
                }

                atapi_command_send_init(ide, temp_command, len, alloc_length);

                atapi_command_ready(ide_board, len);
                break;
                
		case GPCMD_READ_DISC_INFORMATION:
		idebufferb[1] = 32;
		idebufferb[2] = 0xe; /* last session complete, disc finalized */
		idebufferb[3] = 1; /* first track on disc */
		idebufferb[4] = 1; /* # of sessions */
		idebufferb[5] = 1; /* first track of last session */
		idebufferb[6] = 1; /* last track of last session */
		idebufferb[7] = 0x20; /* unrestricted use */
		idebufferb[8] = 0x00; /* CD-ROM */
			
		len=34;
		ide->packetstatus = ATAPI_STATUS_DATA;
		ide->cylinder=len;
		ide->secount=2;
		ide->pos=0;
		idecallback[ide_board]=60*IDE_TIME;
		ide->packlen=len;			
		break;

                case GPCMD_PLAY_AUDIO_10:
                case GPCMD_PLAY_AUDIO_12:
                case GPCMD_PLAY_AUDIO_MSF:
		/*This is apparently deprecated in the ATAPI spec, and apparently
                  has been since 1995 (!). Hence I'm having to guess most of it*/
		if (idebufferb[0] == GPCMD_PLAY_AUDIO_10)
		{
			pos=(idebufferb[2]<<24)|(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
			len=(idebufferb[7]<<8)|idebufferb[8];
		}
		else if (idebufferb[0] == GPCMD_PLAY_AUDIO_MSF)
		{
			pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
			len=(idebufferb[6]<<16)|(idebufferb[7]<<8)|idebufferb[8];
		}
		else
		{
			pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
			len=(idebufferb[7]<<16)|(idebufferb[8]<<8)|idebufferb[9];
		}


		if ((cdrom_drive < 1) || (cd_status <= CD_STATUS_DATA_ONLY) ||
                    !atapi->is_track_audio(pos, (idebufferb[0] == GPCMD_PLAY_AUDIO_MSF) ? 1 : 0))
                {
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
						atapi_sense.sensekey = SENSE_ILLEGAL_REQUEST;
                        atapi_sense.asc = ASC_ILLEGAL_MODE_FOR_THIS_TRACK;
						atapi_sense.ascq = 0;
                        ide->packetstatus = ATAPI_STATUS_ERROR;
                        idecallback[ide_board]=50*IDE_TIME;
						atapi_cmd_error(ide, atapi_sense.sensekey, atapi_sense.asc);
                        break;
                }
				
                atapi->playaudio(pos, len, (idebufferb[0] == GPCMD_PLAY_AUDIO_MSF) ? 1 : 0);
                ide->packetstatus = ATAPI_STATUS_COMPLETE;
                idecallback[ide_board]=50*IDE_TIME;
                break;

                case GPCMD_READ_SUBCHANNEL:
                temp=idebufferb[2]&0x40;
                if (idebufferb[3]!=1)
                {
//                        pclog("Read subchannel check condition %02X\n",idebufferb[3]);
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (atapi_sense.sensekey == SENSE_UNIT_ATTENTION)
                                ide->error |= MCR_ERR;
                        // ide->discchanged=1;			/* Fixes some bugs with NT 3.1. */
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus = ATAPI_STATUS_ERROR;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
/*                        pclog("Bad read subchannel!\n");
                        pclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            pclog("%02X\n",idebufferb[c]);
                        dumpregs();
                        exit(-1);*/
                }
                pos=0;
                idebufferb[pos++]=0;
                idebufferb[pos++]=0; /*Audio status*/
                idebufferb[pos++]=0; idebufferb[pos++]=0; /*Subchannel length*/
                idebufferb[pos++]=1; /*Format code*/
                idebufferb[1]=atapi->getcurrentsubchannel(&idebufferb[5],msf);
//                pclog("Read subchannel complete - audio status %02X\n",idebufferb[1]);
                len=11+5;
                if (!temp) len=4;
                ide->packetstatus = ATAPI_STATUS_DATA;
                ide->cylinder=len;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=1000*IDE_TIME;
                ide->packlen=len;
                break;

                case GPCMD_START_STOP_UNIT:
                if (idebufferb[4]!=2 && idebufferb[4]!=3 && idebufferb[4])
                {
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (atapi_sense.sensekey == SENSE_UNIT_ATTENTION)
                                ide->error |= MCR_ERR;
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus = ATAPI_STATUS_ERROR;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
/*                        pclog("Bad start/stop unit command\n");
                        pclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            pclog("%02X\n",idebufferb[c]);
                        exit(-1);*/
                }
                if (!idebufferb[4])        atapi->stop();
                else if (idebufferb[4]==2) atapi->eject();
                else                       atapi->load();
                ide->packetstatus = ATAPI_STATUS_COMPLETE;
                idecallback[ide_board]=50*IDE_TIME;
                break;
                
                case GPCMD_INQUIRY:
		page_code = idebufferb[2];
		max_len = idebufferb[4];
		alloc_length = max_len;
		temp_command = idebufferb[0];
				
		if (idebufferb[1] & 1)
		{
			preamble_len = 4;
			size_idx = 3;
					
			idebufferb[idx++] = 05;
			idebufferb[idx++] = page_code;
			idebufferb[idx++] = 0;
					
			idx++;
			
			switch (page_code)
			{
				case 0x00:
				idebufferb[idx++] = 0x00;
				idebufferb[idx++] = 0x83;
				break;
        			case 0x83:
				if (idx + 24 > max_len)
				{
					atapi_cmd_error(ide, SENSE_ILLEGAL_REQUEST, ASC_DATA_PHASE_ERROR);
					atapi_sense.sensekey = SENSE_ILLEGAL_REQUEST;
					atapi_sense.asc = ASC_DATA_PHASE_ERROR;
					return;
				}
				idebufferb[idx++] = 0x02;
				idebufferb[idx++] = 0x00;
				idebufferb[idx++] = 0x00;
				idebufferb[idx++] = 20;
				ide_padstr8(idebufferb + idx, 20, "53R141");	/* Serial */
				idx += 20;

				if (idx + 72 > max_len)
				{
					goto atapi_out;
				}
				idebufferb[idx++] = 0x02;
				idebufferb[idx++] = 0x01;
				idebufferb[idx++] = 0x00;
				idebufferb[idx++] = 68;
				ide_padstr8(idebufferb + idx, 8, "PCem"); /* Vendor */
				idx += 8;
				ide_padstr8(idebufferb + idx, 40, "PCemCD v1.0"); /* Product */
				idx += 40;
				ide_padstr8(idebufferb + idx, 20, "53R141"); /* Product */
				idx += 20;
				
				break;
				default:
				atapi_cmd_error(ide, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
				atapi_sense.sensekey = SENSE_ILLEGAL_REQUEST;
				atapi_sense.asc = ASC_INV_FIELD_IN_CMD_PACKET;
				return;
        		}
		}
		else
		{
			preamble_len = 5;
			size_idx = 4;

			idebufferb[0] = 5; /*CD-ROM*/
			idebufferb[1] = 0x80; /*Removable*/
			idebufferb[2] = 0;
			idebufferb[3] = 0x21;
			idebufferb[4] = 31;
			idebufferb[5] = 0;
			idebufferb[6] = 0;
			idebufferb[7] = 0;
#ifdef RPCEMU_IDE
                        ide_padstr8(idebufferb + 8, 8, "RPCemu"); /* Vendor */
                        ide_padstr8(idebufferb + 16, 16, "RPCemuCD"); /* Product */
#else
                        ide_padstr8(idebufferb + 8, 8, "PCem"); /* Vendor */
                        ide_padstr8(idebufferb + 16, 16, "PCemCD"); /* Product */
#endif
                        ide_padstr8(idebufferb + 32, 4, "1.0"); /* Revision */
					
                        idx = 36;
		}

atapi_out:
		idebufferb[size_idx] = idx - preamble_len;
                len=idx;
				
		atapi_command_send_init(ide, temp_command, len, alloc_length);

		atapi_command_ready(ide_board, len);				
                break;

                case GPCMD_PREVENT_REMOVAL:
                ide->packetstatus = ATAPI_STATUS_COMPLETE;
                idecallback[ide_board]=50*IDE_TIME;
                break;

                case GPCMD_PAUSE_RESUME:
                if (idebufferb[8]&1) atapi->resume();
                else                 atapi->pause();
                ide->packetstatus = ATAPI_STATUS_COMPLETE;
                idecallback[ide_board]=50*IDE_TIME;
                break;

                case GPCMD_SEEK:
                pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                atapi->seek(pos);
                ide->packetstatus = ATAPI_STATUS_COMPLETE;
                idecallback[ide_board]=50*IDE_TIME;
                break;

                case GPCMD_READ_CDROM_CAPACITY:
		atapi_command_send_init(ide, 0, 8, 8);
                size = atapi->size();
                idebufferb[0] = (size >> 24) & 0xff;
                idebufferb[1] = (size >> 16) & 0xff;
                idebufferb[2] = (size >> 8) & 0xff;
                idebufferb[3] = size & 0xff;
                idebufferb[4] = (2048 >> 24) & 0xff;
                idebufferb[5] = (2048 >> 16) & 0xff;
                idebufferb[6] = (2048 >> 8) & 0xff;
                idebufferb[7] = 2048 & 0xff;
                len=8;
		atapi_command_ready(ide_board, len);
                break;
                
                case GPCMD_SEND_DVD_STRUCTURE:
                default:
		ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                if (atapi_sense.sensekey == SENSE_UNIT_ATTENTION)
                        ide->error |= MCR_ERR;
		atapi_sense.sensekey = SENSE_ILLEGAL_REQUEST;
                atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                ide->packetstatus = ATAPI_STATUS_ERROR;
                idecallback[ide_board]=50*IDE_TIME;
                break;

                case GPCMD_STOP_PLAY_SCAN:
                atapi->stop();
                ide->packetstatus = ATAPI_STATUS_COMPLETE;
                idecallback[ide_board]=50*IDE_TIME;
                break;
/*                default:
                pclog("Bad ATAPI command %02X\n",idebufferb[0]);
                pclog("Packet data :\n");
                for (c=0;c<12;c++)
                    pclog("%02X\n",idebufferb[c]);
                exit(-1);*/
        }
}

static void callnonreadcd(IDE *ide)		/* Callabck for non-Read CD commands */
{
	ide_irq_lower(ide);
	if (ide->pos >= ide->packlen)
	{
		// pclog("Command finished, setting callback\n");
		ide->packetstatus = ATAPI_STATUS_COMPLETE;
		idecallback[ide->board]=20*IDE_TIME;
	}
	else
	{
		// pclog("Command not finished, keep sending data\n");
		ide->atastat = BUSY_STAT;
		ide->packetstatus = ATAPI_STATUS_REQ_SENSE;
		ide->cylinder=2;
		ide->secount=2;
		idecallback[ide->board]=60*IDE_TIME;
	}
}

static void callreadcd(IDE *ide)
{
        ide_irq_lower(ide);
        if (ide->cdlen<=0)
        {
//                pclog("All done - callback set\n");
                ide->packetstatus = ATAPI_STATUS_COMPLETE;
                idecallback[ide->board]=20*IDE_TIME;
                ide->atastat = BUSY_STAT;
                return;
        }
//        pclog("Continue readcd! %i blocks left\n",ide->cdlen);
        ide->atastat = BUSY_STAT;
        
	if (readcdmode)
		atapi->readsector_raw((uint8_t *) ide->buffer, ide->cdpos);
	else
		atapi->readsector((uint8_t *) ide->buffer, ide->cdpos);
#ifndef RPCEMU_IDE
        readflash_set(READFLASH_HDC, ide->drive);
#endif
        ide->cdpos++;
        ide->cdlen--;
        ide->packetstatus = ATAPI_STATUS_READCD;
        ide->cylinder=readcdmode ? 2352 : 2048;
        ide->secount=2;
        ide->pos=0;
        idecallback[ide->board]=60*IDE_TIME;
        ide->packlen=readcdmode ? 2352 : 2048;
}



void ide_write_pri(uint16_t addr, uint8_t val, void *priv)
{
        writeide(0, addr, val);
}
void ide_write_pri_w(uint16_t addr, uint16_t val, void *priv)
{
        writeidew(0, val);
}
void ide_write_pri_l(uint16_t addr, uint32_t val, void *priv)
{
        writeidel(0, val);
}
uint8_t ide_read_pri(uint16_t addr, void *priv)
{
        return readide(0, addr);
}
uint16_t ide_read_pri_w(uint16_t addr, void *priv)
{
        return readidew(0);
}
uint32_t ide_read_pri_l(uint16_t addr, void *priv)
{
        return readidel(0);
}

void ide_write_sec(uint16_t addr, uint8_t val, void *priv)
{
        writeide(1, addr, val);
}
void ide_write_sec_w(uint16_t addr, uint16_t val, void *priv)
{
        writeidew(1, val);
}
void ide_write_sec_l(uint16_t addr, uint32_t val, void *priv)
{
        writeidel(1, val);
}
uint8_t ide_read_sec(uint16_t addr, void *priv)
{
        return readide(1, addr);
}
uint16_t ide_read_sec_w(uint16_t addr, void *priv)
{
        return readidew(1);
}
uint32_t ide_read_sec_l(uint16_t addr, void *priv)
{
        return readidel(1);
}

void ide_pri_enable()
{
        io_sethandler(0x01f0, 0x0008, ide_read_pri, ide_read_pri_w, ide_read_pri_l, ide_write_pri, ide_write_pri_w, ide_write_pri_l, NULL);
        io_sethandler(0x03f6, 0x0001, ide_read_pri, NULL,           NULL,           ide_write_pri, NULL,            NULL           , NULL);
}

void ide_pri_disable()
{
        io_removehandler(0x01f0, 0x0008, ide_read_pri, ide_read_pri_w, ide_read_pri_l, ide_write_pri, ide_write_pri_w, ide_write_pri_l, NULL);
        io_removehandler(0x03f6, 0x0001, ide_read_pri, NULL,           NULL,           ide_write_pri, NULL,            NULL           , NULL);
}

void ide_sec_enable()
{
        io_sethandler(0x0170, 0x0008, ide_read_sec, ide_read_sec_w, ide_read_sec_l, ide_write_sec, ide_write_sec_w, ide_write_sec_l, NULL);
        io_sethandler(0x0376, 0x0001, ide_read_sec, NULL,           NULL,           ide_write_sec, NULL,            NULL           , NULL);
}

void ide_sec_disable()
{
        io_removehandler(0x0170, 0x0008, ide_read_sec, ide_read_sec_w, ide_read_sec_l, ide_write_sec, ide_write_sec_w, ide_write_sec_l, NULL);
        io_removehandler(0x0376, 0x0001, ide_read_sec, NULL,           NULL,           ide_write_sec, NULL,            NULL           , NULL);
}

void ide_init()
{
        ide_pri_enable();
        ide_sec_enable();
        ide_bus_master_read_sector = ide_bus_master_write_sector = NULL;
        
        timer_add(ide_callback_pri, &idecallback[0], &idecallback[0],  NULL);
        timer_add(ide_callback_sec, &idecallback[1], &idecallback[1],  NULL);
}

void ide_set_bus_master(int (*read_sector)(int channel, uint8_t *data), int (*write_sector)(int channel, uint8_t *data), void (*set_irq)(int channel))
{
        ide_bus_master_read_sector = read_sector;
        ide_bus_master_write_sector = write_sector;
        ide_bus_master_set_irq = set_irq;
}
