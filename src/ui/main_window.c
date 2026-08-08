#include "ui/main_window.h"

#include <gtk/gtk.h>

struct _LrMainWindow
{
    GtkWidget *window;
    LrTreePane *tree;
    LrValuePane *value;
    GtkWidget *location_entry;
    char *current_path;       /* 当前打开/选中的路径 */
    char *pending_path;       /* 恢复状态时待定位的路径 */
    gint saved_w;             /* 非最大化时的窗口宽度 */
    gint saved_h;             /* 非最大化时的窗口高度 */
    gint saved_x;             /* 窗口位置 X */
    gint saved_y;             /* 窗口位置 Y */
    gboolean saved_maximized; /* 是否最大化 */
    guint save_timeout;       /* 节流保存定时器 */
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
        g_free(mw->current_path);
        mw->current_path = NULL;
        return;
    }

    gtk_entry_set_text(GTK_ENTRY(mw->location_entry), path);

    g_free(mw->current_path);
    mw->current_path = g_strdup(path);

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

/* 将窗口状态与上次路径保存到 /run（全部使用缓存值，不依赖窗口实时状态） */
static void
lr_main_window_save_state(LrMainWindow *self)
{
    const char *runtime = g_get_user_runtime_dir();
    gchar *dir, *file, *data;
    GKeyFile *kf;

    if (runtime == NULL)
        return;

    dir = g_build_filename(runtime, "linux-regedit", NULL);
    g_mkdir_with_parents(dir, 0755);
    file = g_build_filename(dir, "state.ini", NULL);

    kf = g_key_file_new();
    if (self->current_path != NULL)
        g_key_file_set_string(kf, "main", "path", self->current_path);

    g_key_file_set_integer(kf, "window", "x", self->saved_x);
    g_key_file_set_integer(kf, "window", "y", self->saved_y);
    g_key_file_set_integer(kf, "window", "width", self->saved_w);
    g_key_file_set_integer(kf, "window", "height", self->saved_h);
    g_key_file_set_boolean(kf, "window", "maximized",
                           self->saved_maximized);

    data = g_key_file_to_data(kf, NULL, NULL);
    if (data != NULL)
    {
        g_file_set_contents(file, data, -1, NULL);
        g_free(data);
    }
    g_key_file_free(kf);
    g_free(file);
    g_free(dir);
}

/* 节流保存：窗口移动/缩放后延迟写入 */
static gboolean
on_save_timeout(gpointer user_data)
{
    LrMainWindow *self = user_data;
    self->save_timeout = 0;
    lr_main_window_save_state(self);
    return G_SOURCE_REMOVE;
}

/* 窗口位置/尺寸变化时记录并节流保存 */
static gboolean
on_configure_event(GtkWidget *widget, GdkEventConfigure *event,
                   gpointer user_data)
{
    LrMainWindow *self = user_data;
    (void)widget;

    self->saved_x = event->x;
    self->saved_y = event->y;

    if (self->save_timeout != 0)
        g_source_remove(self->save_timeout);
    self->save_timeout = g_timeout_add(500, on_save_timeout, self);
    return FALSE;
}

/* 窗口状态变化（最大化等）时记录尺寸与最大化标志 */
static gboolean
on_window_state_event(GtkWidget *widget, GdkEventWindowState *event,
                      gpointer user_data)
{
    LrMainWindow *self = user_data;
    (void)widget;

    self->saved_maximized =
        (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;

    /* 非最大化时记录真实尺寸（最大化时窗口尺寸无参考意义） */
    if (!self->saved_maximized)
    {
        gint w = 0, h = 0;
        gtk_window_get_size(GTK_WINDOW(self->window), &w, &h);
        if (w > 0 && h > 0)
        {
            self->saved_w = w;
            self->saved_h = h;
        }
    }
    return FALSE;
}

/* 窗口显示后延迟定位上次路径 */
static gboolean
on_reveal_path_idle(gpointer user_data)
{
    LrMainWindow *self = user_data;

    if (self->pending_path != NULL)
        lr_tree_pane_reveal_path(self->tree, self->pending_path);
    g_clear_pointer(&self->pending_path, g_free);
    return G_SOURCE_REMOVE;
}

static void
on_window_destroy(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *self = user_data;
    (void)widget;

    if (self->save_timeout != 0)
    {
        g_source_remove(self->save_timeout);
        self->save_timeout = 0;
    }
    lr_main_window_save_state(self);
}

void lr_main_window_restore_state(LrMainWindow *self)
{
    const char *runtime = g_get_user_runtime_dir();
    gchar *dir, *file;
    GKeyFile *kf = NULL;
    GError *error = NULL;

    if (runtime == NULL)
        return;

    dir = g_build_filename(runtime, "linux-regedit", NULL);
    file = g_build_filename(dir, "state.ini", NULL);

    kf = g_key_file_new();
    if (g_key_file_load_from_file(kf, file, G_KEY_FILE_NONE, &error))
    {
        gint w, h, x, y;
        gboolean maximized;

        w = g_key_file_get_integer(kf, "window", "width", NULL);
        h = g_key_file_get_integer(kf, "window", "height", NULL);
        if (w > 0 && h > 0)
            gtk_window_resize(GTK_WINDOW(self->window), w, h);

        x = g_key_file_get_integer(kf, "window", "x", NULL);
        y = g_key_file_get_integer(kf, "window", "y", NULL);
        gtk_window_move(GTK_WINDOW(self->window), x, y);

        maximized = g_key_file_get_boolean(kf, "window", "maximized", NULL);
        if (maximized)
            gtk_window_maximize(GTK_WINDOW(self->window));

        {
            gchar *path = g_key_file_get_string(kf, "main", "path", NULL);
            if (path != NULL)
            {
                g_free(self->pending_path);
                self->pending_path = g_strdup(path);
                g_free(path);
            }
        }
    }
    else
    {
        g_clear_error(&error);
    }

    if (self->pending_path != NULL)
        g_idle_add(on_reveal_path_idle, self);

    g_key_file_free(kf);
    g_free(file);
    g_free(dir);
}

LrMainWindow *
lr_main_window_new(void)
{
    LrMainWindow *mw = g_new0(LrMainWindow, 1);
    GtkWidget *vbox, *menubar, *location_bar, *paned;

    /* 与默认窗口大小一致，首次关闭前缓存有效 */
    mw->saved_w = 920;
    mw->saved_h = 600;

    mw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mw->window), "注册表编辑器");
    gtk_window_set_default_size(GTK_WINDOW(mw->window), 920, 600);
    gtk_window_set_position(GTK_WINDOW(mw->window), GTK_WIN_POS_CENTER);
    g_signal_connect(mw->window, "destroy",
                     G_CALLBACK(on_window_destroy), mw);
    g_signal_connect(mw->window, "configure-event",
                     G_CALLBACK(on_configure_event), mw);
    g_signal_connect(mw->window, "window-state-event",
                     G_CALLBACK(on_window_state_event), mw);

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
    g_free(self->current_path);
    g_clear_pointer(&self->pending_path, g_free);
    lr_tree_pane_free(self->tree);
    lr_value_pane_free(self->value);
    g_free(self);
}
