#include "paths.h"
#include "config.h"
#include <string.h>
#include "ibm.h"

char default_roms_paths[4096];
char default_nvr_path[512];
char default_configs_path[512];
char default_logs_path[512];

int num_roms_paths;
char roms_paths[4096];
char pcem_path[512];
char nvr_path[512];
char configs_path[512];
char logs_path[512];

char get_path_separator()
{
#ifdef __WINDOWS__
        return ';';
#else
        return ':';
#endif
}

int get_roms_path(int pos, char* s, int size)
{
        int j, i, z, len;
        char path_separator;
        path_separator = get_path_separator();
        len = strlen(roms_paths);
        j = 0;
        for (i = 0; i < len; ++i)
        {
                if (roms_paths[i] == path_separator || i == len-1)
                {
                        if ((pos--) == 0)
                        {
                                z = (i-j) + ((i == len-1) ? 1 : 0);
                                strncpy(s, roms_paths+j, (size < z ? size : z));
                                s[z] = 0;
                                return 1;
                        }
                        j = i+1;
                }
        }
        return 0;
}

void set_roms_paths(char* path)
{
        char s[512];
        int j, i, z, len;
        char path_separator[2];
        roms_paths[0] = 0;
        path_separator[0] = get_path_separator();
        path_separator[1] = 0;
        len = strlen(path);
        j = 0;
        num_roms_paths = 0;
        for (i = 0; i < len; ++i)
        {
                if (path[i] == path_separator[0] || i == len-1)
                {
                        z = (i-j) + ((i == len-1) ? 1 : 0);
                        strncpy(s, path+j, z);
                        s[z] = 0;
                        append_slash(s);
                        if (dir_exists(s))
                        {
                                if (num_roms_paths > 0)
                                        strcat(roms_paths, path_separator);
                                strcat(roms_paths, s);
                                ++num_roms_paths;
                        }
                        j = i+1;
                }
        }
}

void set_nvr_path(char *s)
{
        strncpy(nvr_path, s, 511);
}

void set_logs_path(char *s)
{
        strncpy(logs_path, s, 511);
}

void set_configs_path(char *s)
{
        strncpy(configs_path, s, 511);
}

void set_default_roms_paths(char *s)
{
        strncpy(default_roms_paths, s, 1023);
        set_roms_paths(s);
}

void set_default_nvr_path(char *s)
{
        strncpy(default_nvr_path, s, 511);
        set_nvr_path(s);
}

void set_default_logs_path(char *s)
{
        strncpy(default_logs_path, s, 511);
        set_logs_path(s);
}

void set_default_configs_path(char *s)
{
        strncpy(default_configs_path, s, 511);
        set_configs_path(s);
}

void paths_loadconfig()
{
        char* cfg_roms_paths = config_get_string(CFG_GLOBAL, "Paths", "roms_paths", 0);
        if (cfg_roms_paths)
                strncpy(default_roms_paths, cfg_roms_paths, 4095);
        char* cfg_nvr_path = config_get_string(CFG_GLOBAL, "Paths", "nvr_path", 0);
        if (cfg_nvr_path)
                strncpy(default_nvr_path, cfg_nvr_path, 511);
        char* cfg_configs_path = config_get_string(CFG_GLOBAL, "Paths", "configs_path", 0);
        if (cfg_configs_path)
                strncpy(default_configs_path, cfg_configs_path, 511);
        char* cfg_logs_path = config_get_string(CFG_GLOBAL, "Paths", "logs_path", 0);
        if (cfg_logs_path)
                strncpy(default_logs_path, cfg_logs_path, 511);

}

void paths_saveconfig()
{
        config_set_string(CFG_GLOBAL, "Paths", "roms_paths", default_roms_paths);
        config_set_string(CFG_GLOBAL, "Paths", "nvr_path", default_nvr_path);
        config_set_string(CFG_GLOBAL, "Paths", "configs_path", default_configs_path);
        config_set_string(CFG_GLOBAL, "Paths", "logs_path", default_logs_path);
}

void paths_onconfigloaded()
{
        if (strlen(default_roms_paths) > 0)
                set_roms_paths(default_roms_paths);

        if (strlen(default_nvr_path) > 0)
                strncpy(nvr_path, default_nvr_path, 511);

        if (strlen(default_configs_path) > 0)
                strncpy(configs_path, default_configs_path, 511);

        if (strlen(default_logs_path) > 0)
                strncpy(logs_path, default_logs_path, 511);

        pclog("path = %s\n", pcem_path);
}

void paths_init()
{
        char s[512];
        char *p;

        /* config path may be path to executable */
        get_pcem_path(pcem_path, 511);
        p=get_filename(pcem_path);
        *p=0;

        /* set up default paths for this session */
        append_filename(s, pcem_path, "roms/", 511);
        set_roms_paths(s);
        append_filename(s, pcem_path, "nvr/", 511);
        set_nvr_path(s);
        append_filename(s, pcem_path, "configs/", 511);
        set_configs_path(s);
        set_logs_path(pcem_path);

        add_config_callback(paths_loadconfig, paths_saveconfig, paths_onconfigloaded);

}
