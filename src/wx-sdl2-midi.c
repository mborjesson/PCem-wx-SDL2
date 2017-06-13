#include <stdint.h>
#include "plat-midi.h"

void plat_midi_init()
{
}

void plat_midi_close()
{
}

void plat_midi_play_msg(uint8_t* val)
{
}

void plat_midi_play_sysex(uint8_t* data, unsigned int len)
{
}

int plat_midi_write(uint8_t val)
{
        return 0;
}

int plat_midi_get_num_devs()
{
        return 0;
}

void plat_midi_get_dev_name(int num, char *s)
{
}
