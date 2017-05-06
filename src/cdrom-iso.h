#ifndef CDROM_ISO_H
#define CDROM_ISO_H

/* this header file lists the functions provided by
   various platform specific cdrom-ioctl files */

extern char iso_path[1024];

#ifdef __cplusplus
extern "C" {
#endif

int iso_open(char *fn);
void iso_reset();
void iso_close();

void iso_audio_callback(int16_t *output, int len);
void iso_audio_stop();

#ifdef __cplusplus
}
#endif

#endif /* ! CDROM_ISO_H */
