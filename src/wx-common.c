#include "wx-common.h"
#include "config.h"

void wx_loadconfig()
{
        show_machine_info = config_get_int("wxWidgets", "show_machine_info", show_machine_info);
        show_disc_activity = config_get_int("wxWidgets", "show_disc_activity", show_disc_activity);
        show_speed_history = config_get_int("wxWidgets", "show_speed_history", show_speed_history);
        show_mount_paths = config_get_int("wxWidgets", "show_mount_paths", show_mount_paths);
        show_status = config_get_int("wxWidgets", "show_status", show_status);
        wx_window_x = config_get_int("wxWidgets", "window_x", wx_window_x);
        wx_window_y = config_get_int("wxWidgets", "window_y", wx_window_y);
}

void wx_saveconfig()
{
        config_set_int("wxWidgets", "show_machine_info", show_machine_info);
        config_set_int("wxWidgets", "show_disc_activity", show_disc_activity);
        config_set_int("wxWidgets", "show_speed_history", show_speed_history);
        config_set_int("wxWidgets", "show_mount_paths", show_mount_paths);
        config_set_int("wxWidgets", "show_status", show_status);
        config_set_int("wxWidgets", "window_x", wx_window_x);
        config_set_int("wxWidgets", "window_y", wx_window_y);
}
