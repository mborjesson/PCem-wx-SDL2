#ifndef CDROM_IMAGE_H
#define CDROM_IMAGE_H

#include <stdint.h>

extern char image_path[1024];

#ifdef __cplusplus
extern "C" {
#endif

int image_open(char *fn);
void image_reset();
void image_close();

void image_audio_callback(int16_t *output, int len);
void image_audio_stop();

#ifdef __cplusplus
}
#endif

#endif /* ! CDROM_IMAGE_H */
