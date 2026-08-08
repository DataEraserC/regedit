/* 应用激活回调 */
#ifndef LR_APP_H
#define LR_APP_H

#include <gio/gio.h>
#include <gtk/gtk.h>

void lr_app_activate(GtkApplication *app, gpointer user_data);
void lr_app_open(GApplication *app, GFile **files, gint n_files,
                 const gchar *hint, gpointer user_data);

#endif /* LR_APP_H */
