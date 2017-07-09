#include "allegro-main.h"
#include "ibm.h"
#include "midi.h"
#include "plat-midi.h"

//#define USE_ALLEGRO_MIDI

void* plat_midi_init()
{
#ifdef USE_ALLEGRO_MIDI
	install_sound(DIGI_NONE, MIDI_AUTODETECT, NULL);
#endif
	return 0;
}

void plat_midi_close(void* p)
{
#ifdef USE_ALLEGRO_MIDI
	remove_sound();
#endif
}

void plat_midi_play_msg(midi_device_t* device, uint8_t* val)
{
}

void plat_midi_play_sysex(midi_device_t* device, uint8_t* data, unsigned int len)
{
}

int plat_midi_get_num_devs()
{
#ifdef USE_ALLEGRO_MIDI
        return 1;
#else
        return 0;
#endif
}

void plat_midi_get_dev_name(int num, char *s)
{
#ifdef USE_ALLEGRO_MIDI
        strcpy(s, "Allegro");
#endif
}

static int midi_cmd_pos, midi_len;
static uint8_t midi_command[3];
static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 0};

int plat_midi_write(midi_device_t* device, uint8_t val)
{
        if (val & 0x80)
        {
                midi_cmd_pos = 0;
                midi_len = midi_lengths[(val >> 4) & 7];
                midi_command[0] = midi_command[1] = midi_command[2] = 0;
        }

        if (midi_len && midi_cmd_pos < 3)
        {                
                midi_command[midi_cmd_pos] = val;
                
                midi_cmd_pos++;
                
#ifdef USE_ALLEGRO_MIDI
                if (midi_cmd_pos == midi_len)
                        midi_out(midi_command, midi_len);
#endif
        }
        return 1;
}

void plat_midi_add_status_info(char *s, int max_len, struct midi_device_t* device)
{
}
