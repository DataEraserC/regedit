#include "app.h"
#include "ui/main_window.h"

/* 确保主窗口已创建并显示，返回主窗口对象 */
static LrMainWindow *
ensure_window(GtkApplication *app)
{
    LrMainWindow *mw = g_object_get_data(G_OBJECT(app), "window");
    GtkWidget *window;

    if (mw != NULL)
        return mw;

    mw = lr_main_window_new();
    window = lr_main_window_get_window(mw);
    gtk_application_add_window(app, GTK_WINDOW(window));

    /* 显示前恢复窗口大小、位置与最大化状态 */
    lr_main_window_restore_state(mw);
    gtk_widget_show_all(window);

    g_object_set_data(G_OBJECT(app), "window", mw);
    return mw;
}

/* GApplication "open"：命令行打开文件（带文件时不会触发 activate） */
void lr_app_open(GApplication *app, GFile **files, gint n_files,
                 const gchar *hint, gpointer user_data)
{
    LrMainWindow *mw;
    gchar *path;

    (void)hint;
    (void)user_data;

    if (n_files < 1)
        return;

    mw = ensure_window(GTK_APPLICATION(app));
    path = g_file_get_path(files[0]);
    lr_main_window_open_file(mw, path);
    g_free(path);
}

void lr_app_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    ensure_window(app);
}
