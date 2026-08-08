#include "app.h"
#include "ui/main_window.h"

void lr_app_activate(GtkApplication *app, gpointer user_data)
{
    LrMainWindow *mw;
    GtkWidget *window;

    (void)user_data;

    mw = lr_main_window_new();
    window = lr_main_window_get_window(mw);
    gtk_application_add_window(app, GTK_WINDOW(window));
    gtk_widget_show_all(window);

    /* 恢复本次开机期间保存的窗口状态与上次路径 */
    lr_main_window_restore_state(mw);
}
