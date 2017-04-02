#include "wx-common.h"
#include "config.h"

void wx_loadconfig() {
        show_disc_activity = config_get_int("WX", "show_disc_activity", show_disc_activity);
        show_speed_history = config_get_int("WX", "show_speed_history", show_speed_history);
        show_status = config_get_int("WX", "show_status", show_status);
        wx_window_x = config_get_int("WX", "window_x", wx_window_x);
        wx_window_y = config_get_int("WX", "window_y", wx_window_y);
}

void wx_saveconfig() {
        config_set_int("WX", "show_disc_activity", show_disc_activity);
        config_set_int("WX", "show_speed_history", show_speed_history);
        config_set_int("WX", "show_status", show_status);
        config_set_int("WX", "window_x", wx_window_x);
        config_set_int("WX", "window_y", wx_window_y);
}
