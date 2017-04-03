#include "wx-common.h"
#include "config.h"

void wx_loadconfig()
{
        show_machine_info = config_get_int("wxWidgets", "show_machine_info", show_machine_info);
        show_disc_activity = config_get_int("wxWidgets", "show_disc_activity", show_disc_activity);
        show_speed_history = config_get_int("wxWidgets", "show_speed_history", show_speed_history);
        show_status = config_get_int("wxWidgets", "show_status", show_status);
        hide_on_close = config_get_int("wxWidgets", "hide_on_close", hide_on_close);
        hide_on_close_first = config_get_int("wxWidgets", "hide_on_close_first", hide_on_close_first);
        hide_on_start = config_get_int("wxWidgets", "hide_on_start", hide_on_start);
        wx_window_x = config_get_int("wxWidgets", "window_x", wx_window_x);
        wx_window_y = config_get_int("wxWidgets", "window_y", wx_window_y);
}

void wx_saveconfig()
{
        config_set_int("wxWidgets", "show_machine_info", show_machine_info);
        config_set_int("wxWidgets", "show_disc_activity", show_disc_activity);
        config_set_int("wxWidgets", "show_speed_history", show_speed_history);
        config_set_int("wxWidgets", "show_status", show_status);
        config_set_int("wxWidgets", "hide_on_close", hide_on_close);
        config_set_int("wxWidgets", "hide_on_close_first", hide_on_close_first);
        config_set_int("wxWidgets", "hide_on_start", hide_on_start);
        config_set_int("wxWidgets", "window_x", wx_window_x);
        config_set_int("wxWidgets", "window_y", wx_window_y);
}
