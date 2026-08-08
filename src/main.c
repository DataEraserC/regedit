/*
 * linux-regedit — 程序入口
 *
 * Linux 版 regedit：以注册表编辑器的交互浏览 /etc 与 ~/.config 的配置文件。
 */
#include <gtk/gtk.h>
#include "app.h"

int main(int argc, char **argv)
{
    GtkApplication *app;
    gint status;

    app = gtk_application_new("org.linux-regedit.app",
                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(lr_app_activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
