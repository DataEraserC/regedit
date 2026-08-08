#include "ui/main_window.h"

#include <gtk/gtk.h>

struct _LrMainWindow
{
    GtkWidget *window;
    LrTreePane *tree;
    LrValuePane *value;
    GtkWidget *statusbar;
    guint status_ctx;
};

static void
on_tree_select(const char *path, gboolean is_dir, gpointer user_data)
{
    LrMainWindow *mw = user_data;

    gtk_statusbar_remove_all(GTK_STATUSBAR(mw->statusbar), mw->status_ctx);

    if (is_dir)
    {
        lr_value_pane_clear(mw->value);
        gtk_statusbar_push(GTK_STATUSBAR(mw->statusbar), mw->status_ctx,
                           path);
    }
    else
    {
        lr_value_pane_load_file(mw->value, path);
        gtk_statusbar_push(GTK_STATUSBAR(mw->statusbar), mw->status_ctx,
                           path);
    }
}

static void
on_refresh(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    (void)widget;
    lr_tree_pane_refresh(mw->tree);
}

static void
on_quit(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *window = user_data;
    (void)widget;
    gtk_widget_destroy(window);
}

static void
on_about(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *window = user_data;
    GtkWidget *dialog = gtk_about_dialog_new();
    GtkAboutDialog *about = GTK_ABOUT_DIALOG(dialog);
    (void)widget;

    gtk_about_dialog_set_program_name(about, "linux-regedit");
    gtk_about_dialog_set_version(about, "0.1.0");
    gtk_about_dialog_set_comments(
        about,
        "Linux 版 regedit —— 以熟悉的注册表编辑器交互浏览 /etc 与 ~/.config "
        "下的配置文件。");
    gtk_about_dialog_set_license_type(about, GTK_LICENSE_GPL_3_0);
    gtk_about_dialog_set_copyright(about, "© 2026 DeepSeek");

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static GtkWidget *
build_menubar(LrMainWindow *mw)
{
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *file_menu, *file_item, *help_menu, *help_item;
    GtkWidget *item;

    /* 文件 */
    file_item = gtk_menu_item_new_with_label("文件");
    file_menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("刷新");
    g_signal_connect(item, "activate", G_CALLBACK(on_refresh), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
    item = gtk_menu_item_new_with_label("退出");
    g_signal_connect(item, "activate", G_CALLBACK(on_quit), mw->window);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);

    /* 帮助 */
    help_item = gtk_menu_item_new_with_label("帮助");
    help_menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("关于");
    g_signal_connect(item, "activate", G_CALLBACK(on_about), mw->window);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_item);

    return menubar;
}

static GtkWidget *
build_toolbar(LrMainWindow *mw)
{
    GtkWidget *toolbar = gtk_toolbar_new();
    GtkToolItem *item;

    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    item = gtk_tool_button_new(
        gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_SMALL_TOOLBAR),
        "刷新");
    g_signal_connect(item, "clicked", G_CALLBACK(on_refresh), mw);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    return toolbar;
}

LrMainWindow *
lr_main_window_new(void)
{
    LrMainWindow *mw = g_new0(LrMainWindow, 1);
    GtkWidget *vbox, *menubar, *toolbar, *paned;

    mw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mw->window), "linux-regedit");
    gtk_window_set_default_size(GTK_WINDOW(mw->window), 920, 600);
    gtk_window_set_position(GTK_WINDOW(mw->window), GTK_WIN_POS_CENTER);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    menubar = build_menubar(mw);
    toolbar = build_toolbar(mw);

    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    mw->tree = lr_tree_pane_new();
    mw->value = lr_value_pane_new();
    gtk_paned_pack1(GTK_PANED(paned), lr_tree_pane_get_widget(mw->tree),
                    FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), lr_value_pane_get_widget(mw->value),
                    TRUE, TRUE);
    gtk_paned_set_position(GTK_PANED(paned), 320);

    mw->statusbar = gtk_statusbar_new();
    mw->status_ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(mw->statusbar),
                                                  "main");
    gtk_statusbar_push(GTK_STATUSBAR(mw->statusbar), mw->status_ctx,
                       "就绪：选择左侧配置文件");

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), mw->statusbar, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(mw->window), vbox);

    lr_tree_pane_set_select_cb(mw->tree, on_tree_select, mw);

    return mw;
}

GtkWidget *
lr_main_window_get_window(LrMainWindow *self)
{
    return self->window;
}

void lr_main_window_free(LrMainWindow *self)
{
    if (self == NULL)
        return;
    lr_tree_pane_free(self->tree);
    lr_value_pane_free(self->value);
    g_free(self);
}
