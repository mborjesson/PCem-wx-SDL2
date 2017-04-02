#include "wx-sdl2.h"
#include "wx-sdl2-video.h"
#include "ibm.h"
#include "device.h"
#include "video.h"
#include "x86_ops.h"
#include "mem.h"
#include "codegen.h"
#include "cpu.h"
#include "model.h"
#include "ide.h"
#include "cdrom-iso.h"

int status_is_open = 0;

extern int sreadlnum, swritelnum, segareads, segawrites, scycles_lost;
extern int render_fps, fps;

extern uint64_t main_time;
extern uint64_t render_time;
static uint64_t status_time;

int get_machine_info(char* s) {
        sprintf(s,
                        "Model: %s\n"
                        "CPU: %s\n"
                        "Emulation speed: %d%%",
                        model_getname(),
                        models[model].cpu[cpu_manufacturer].cpus[cpu].name,
                        fps);
        if (strlen(discfns[0]) > 0)
        {
                sprintf(s+strlen(s), "\nA: %s", discfns[0]);
        }
        if (strlen(discfns[1]) > 0)
        {
                sprintf(s+strlen(s), "\nB: %s", discfns[1]);
        }
        if (cdrom_drive == CDROM_ISO && strlen(iso_path) > 0)
        {
                sprintf(s+strlen(s), "\nCD-ROM ISO: %s", iso_path);
        }
        return 1;
}

int get_status(char* machine, char* device)
{
//        if (!status_is_open) {
//                return 0;
//        }
//        char device_s[4096];
        uint64_t new_time = timer_read();
        uint64_t status_diff = new_time - status_time;
        status_time = new_time;
        sprintf(machine,
                "CPU speed : %f MIPS\n"
                "FPU speed : %f MFLOPS\n\n"

        /*                        "Cache misses (read) : %i/sec\n"
                "Cache misses (write) : %i/sec\n\n"*/

                "Video throughput (read) : %i bytes/sec\n"
                "Video throughput (write) : %i bytes/sec\n\n"
                "Effective clockspeed : %iHz\n\n"
                "Timer 0 frequency : %fHz\n\n"
                "CPU time : %f%% (%f%%)\n"
                "Render time : %f%% (%f%%)\n"
                "Renderer: %s\n"
                "Render FPS: %d\n"
                "\n"

                "New blocks : %i\nOld blocks : %i\nRecompiled speed : %f MIPS\nAverage size : %f\n"
                "Flushes : %i\nEvicted : %i\nReused : %i\nRemoved : %i\nReal speed : %f MIPS"
        //                        "\nFully recompiled ins %% : %f%%"
                ,mips,
                flops,
        /*#ifndef DYNAREC
                sreadlnum,
                swritelnum,
        #endif*/
                segareads,
                segawrites,
                clockrate - scycles_lost,
                pit_timer0_freq(),
                ((double)main_time * 100.0) / status_diff,
                ((double)main_time * 100.0) / timer_freq,
                ((double)render_time * 100.0) / status_diff,
                ((double)render_time * 100.0) / timer_freq,
                current_render_driver_name,
                render_fps

                , cpu_new_blocks_latched, cpu_recomp_blocks_latched, (double)cpu_recomp_ins_latched / 1000000.0, (double)cpu_recomp_ins_latched/cpu_recomp_blocks_latched,
                cpu_recomp_flushes_latched, cpu_recomp_evicted_latched,
                cpu_recomp_reuse_latched, cpu_recomp_removed_latched,

                ((double)cpu_recomp_ins_latched / 1000000.0) / ((double)main_time / timer_freq)
        //                        ((double)cpu_recomp_full_ins_latched / (double)cpu_recomp_ins_latched) * 100.0
        //                        cpu_reps_latched, cpu_notreps_latched
        );
        main_time = 0;
        render_time = 0;
        /*#ifndef DYNAREC
        device_add_status_info(device_s, 4096);
        #endif*/

//        device_s[0] = 0;
        device[0] = 0;
        device_add_status_info(device, 4096);

        return 1;
}
