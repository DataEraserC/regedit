#include "ui/main_window.h"

#include <gtk/gtk.h>

struct _LrMainWindow
{
    GtkWidget *window;
    LrTreePane *tree;
    LrValuePane *value;
    GtkWidget *location_entry;
};

static void
on_tree_select(const char *path, gboolean is_dir, gpointer user_data)
{
    LrMainWindow *mw = user_data;

    /* 计算机虚拟根（空路径） */
    if (path == NULL || *path == '\0')
    {
        gtk_entry_set_text(GTK_ENTRY(mw->location_entry), "计算机");
        lr_value_pane_clear(mw->value);
        return;
    }

    gtk_entry_set_text(GTK_ENTRY(mw->location_entry), path);

    if (is_dir)
        lr_value_pane_clear(mw->value);
    else
        lr_value_pane_load_file(mw->value, path);
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

static void
on_expand_all(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    (void)widget;
    lr_tree_pane_expand_all(mw->tree);
}

static void
on_collapse_all(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    (void)widget;
    lr_tree_pane_collapse_all(mw->tree);
}

static GtkWidget *
build_menubar(LrMainWindow *mw)
{
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *menu, *menu_item, *item;

    /* 文件 */
    menu_item = gtk_menu_item_new_with_label("文件");
    menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("刷新");
    g_signal_connect(item, "activate", G_CALLBACK(on_refresh), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    item = gtk_menu_item_new_with_label("退出");
    g_signal_connect(item, "activate", G_CALLBACK(on_quit), mw->window);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    /* 编辑 */
    menu_item = gtk_menu_item_new_with_label("编辑");
    menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("查找…");
    gtk_widget_set_sensitive(item, FALSE); /* v0.2 实现 */
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    /* 查看 */
    menu_item = gtk_menu_item_new_with_label("查看");
    menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("展开全部");
    g_signal_connect(item, "activate", G_CALLBACK(on_expand_all), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    item = gtk_menu_item_new_with_label("收起全部");
    g_signal_connect(item, "activate", G_CALLBACK(on_collapse_all), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    /* 收藏夹 */
    menu_item = gtk_menu_item_new_with_label("收藏夹");
    menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("添加到收藏夹…");
    gtk_widget_set_sensitive(item, FALSE); /* v0.5 实现 */
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    /* 帮助 */
    menu_item = gtk_menu_item_new_with_label("帮助");
    menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("关于");
    g_signal_connect(item, "activate", G_CALLBACK(on_about), mw->window);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    return menubar;
}

static void
on_location_activate(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(widget));
    gchar *path;

    if (text == NULL)
        return;
    path = g_strstrip(g_strdup(text));
    if (*path == '\0')
    {
        g_free(path);
        return;
    }

    /* 优先在树中定位（成功则选择回调会同步右侧） */
    if (lr_tree_pane_reveal_path(mw->tree, path))
    {
        g_free(path);
        return;
    }

    gtk_entry_set_text(GTK_ENTRY(mw->location_entry), path);

    if (g_file_test(path, G_FILE_TEST_IS_DIR))
        lr_value_pane_clear(mw->value);
    else
        lr_value_pane_load_file(mw->value, path);
    g_free(path);
}

/* 通过 CSS 降低地址栏输入框高度（更紧凑） */
static void
add_location_css(GtkWidget *entry)
{
    static GtkCssProvider *css = NULL;

    if (css == NULL)
    {
        css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(
            css,
            "#lr-location { min-height: 0; padding: 1px 6px; }",
            -1, NULL);
    }
    gtk_widget_set_name(entry, "lr-location");
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(entry), GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
}

static GtkWidget *
build_location_bar(LrMainWindow *mw)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    gtk_widget_set_margin_start(bar, 8);
    gtk_widget_set_margin_end(bar, 8);
    gtk_widget_set_margin_top(bar, 2);
    gtk_widget_set_margin_bottom(bar, 2);

    mw->location_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(mw->location_entry),
                                   "输入路径后回车跳转");
    add_location_css(mw->location_entry);
    gtk_box_pack_start(GTK_BOX(bar), mw->location_entry, TRUE, TRUE, 0);

    g_signal_connect(mw->location_entry, "activate",
                     G_CALLBACK(on_location_activate), mw);
    return bar;
}

LrMainWindow *
lr_main_window_new(void)
{
    LrMainWindow *mw = g_new0(LrMainWindow, 1);
    GtkWidget *vbox, *menubar, *location_bar, *paned;

    mw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mw->window), "注册表编辑器");
    gtk_window_set_default_size(GTK_WINDOW(mw->window), 920, 600);
    gtk_window_set_position(GTK_WINDOW(mw->window), GTK_WIN_POS_CENTER);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    menubar = build_menubar(mw);
    location_bar = build_location_bar(mw);

    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    mw->tree = lr_tree_pane_new();
    mw->value = lr_value_pane_new();
    gtk_paned_pack1(GTK_PANED(paned), lr_tree_pane_get_widget(mw->tree),
                    FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), lr_value_pane_get_widget(mw->value),
                    TRUE, TRUE);
    gtk_paned_set_position(GTK_PANED(paned), 320);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), location_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);

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
