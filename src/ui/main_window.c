#include "ui/main_window.h"
#include "ui/window_state.h"
#include "ui/favorites.h"
#include "ui/export.h"
#include "ui/dialog_utils.h"

#include <gtk/gtk.h>

static void
open_path(LrMainWindow *mw, const char *path, gboolean is_dir)
{
    /* 计算机虚拟根（空路径） */
    if (path == NULL || *path == '\0')
    {
        gtk_entry_set_text(GTK_ENTRY(mw->location_entry), "计算机");
        lr_value_pane_clear(mw->value);
        g_free(mw->current_path);
        mw->current_path = NULL;
        lr_window_state_set_path(mw->win_state, NULL);
        return;
    }

    gtk_entry_set_text(GTK_ENTRY(mw->location_entry), path);

    g_free(mw->current_path);
    mw->current_path = g_strdup(path);
    lr_window_state_set_path(mw->win_state, path);

    if (is_dir)
        lr_value_pane_clear(mw->value);
    else
        lr_value_pane_load_file(mw->value, path);
}

static void
on_tree_select(const char *path, gboolean is_dir, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    open_path(mw, path, is_dir);
}

/* 命令行 / 外部调用：直接打开指定文件（目录则仅定位到树中） */
void lr_main_window_open_file(LrMainWindow *mw, const char *path)
{
    gboolean is_dir = g_file_test(path, G_FILE_TEST_IS_DIR);
    open_path(mw, path, is_dir);
    lr_tree_pane_reveal_path(mw->tree, path);
}

static void
on_refresh(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    (void)widget;
    lr_tree_pane_refresh(mw->tree);
}

/* 查看→地址栏：勾选切换地址栏显示/隐藏 */
static void
on_toggle_location_bar(GtkCheckMenuItem *item, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    gboolean active = gtk_check_menu_item_get_active(item);
    if (mw->location_bar != NULL)
        gtk_widget_set_visible(mw->location_bar, active);
}

/* 编辑→复制项名称：把当前路径复制到剪贴板 */
static void
on_copy_item_name(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    GtkClipboard *clip;
    (void)widget;

    if (mw->current_path == NULL)
        return;
    clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clip, mw->current_path, -1);
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

    /* 独立可移动子窗口：相对主窗口定位，不与主窗口联动 */
    g_signal_connect(dialog, "response",
                     G_CALLBACK(lr_dialog_destroy_on_response), NULL);
    lr_dialog_center_on(dialog, GTK_WINDOW(window));
}

static GtkWidget *
build_menubar(LrMainWindow *mw)
{
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *menu, *menu_item, *item;

    /* 文件：导入 / 导出 / 打印 / 退出 */
    menu_item = gtk_menu_item_new_with_label("文件");
    menu = gtk_menu_new();

    item = gtk_menu_item_new_with_label("导入…");
    gtk_widget_set_sensitive(item, FALSE); /* 占位 */
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("导出…");
    g_signal_connect(item, "activate", G_CALLBACK(lr_export_show_dialog), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("打印…");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("退出");
    g_signal_connect(item, "activate", G_CALLBACK(on_quit), mw->window);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    /* 编辑：新建 / 权限 / 删除 / 重命名 / 复制项名称 / 查找 / 查找下一个 */
    menu_item = gtk_menu_item_new_with_label("编辑");
    menu = gtk_menu_new();

    item = gtk_menu_item_new_with_label("新建");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("权限…");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("删除");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("重命名");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("复制项名称");
    g_signal_connect(item, "activate", G_CALLBACK(on_copy_item_name), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("查找…");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("查找下一个");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    /* 查看：地址栏 / 拆分 / 刷新 / 字体 */
    menu_item = gtk_menu_item_new_with_label("查看");
    menu = gtk_menu_new();

    item = gtk_check_menu_item_new_with_label("地址栏");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    g_signal_connect(item, "toggled", G_CALLBACK(on_toggle_location_bar), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("拆分");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("刷新");
    g_signal_connect(item, "activate", G_CALLBACK(on_refresh), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("字体…");
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    /* 收藏夹：添加/删除 + 分割线 + 收藏项（菜单显示时动态刷新） */
    menu_item = gtk_menu_item_new_with_label("收藏夹");
    menu = gtk_menu_new();
    g_signal_connect(menu, "show", G_CALLBACK(lr_favorites_fill_menu), mw);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menu_item);

    /* 帮助 */
    menu_item = gtk_menu_item_new_with_label("帮助");
    menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("关于注册表编辑器");
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

    mw->location_bar = bar;
    mw->location_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(mw->location_entry),
                                   "输入路径后回车跳转");
    add_location_css(mw->location_entry);
    gtk_box_pack_start(GTK_BOX(bar), mw->location_entry, TRUE, TRUE, 0);

    g_signal_connect(mw->location_entry, "activate",
                     G_CALLBACK(on_location_activate), mw);
    return bar;
}

/* 窗口位置/尺寸变化时记录并节流保存 */
static gboolean
on_configure_event(GtkWidget *widget, GdkEventConfigure *event,
                   gpointer user_data)
{
    LrMainWindow *self = user_data;
    (void)widget;

    lr_window_state_set_geometry(self->win_state, event->x, event->y,
                                 event->width, event->height);
    lr_window_state_schedule_save(self->win_state);
    return FALSE;
}

/* 窗口状态变化（最大化等）时记录尺寸与最大化标志 */
static gboolean
on_window_state_event(GtkWidget *widget, GdkEventWindowState *event,
                      gpointer user_data)
{
    LrMainWindow *self = user_data;
    gboolean maxed;
    (void)widget;

    maxed = (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    lr_window_state_set_maximized(self->win_state, maxed);

    /* 非最大化时记录真实尺寸（最大化时窗口尺寸无参考意义） */
    if (!maxed)
    {
        gint w = 0, h = 0;
        gtk_window_get_size(GTK_WINDOW(self->window), &w, &h);
        lr_window_state_set_size(self->win_state, w, h);
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

/* 窗口显示后把键盘焦点给树视图（避免默认聚焦到地址栏输入框） */
static gboolean
on_focus_tree_idle(gpointer user_data)
{
    LrMainWindow *self = user_data;
    lr_tree_pane_focus(self->tree);
    return G_SOURCE_REMOVE;
}

static void
on_window_destroy(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *self = user_data;
    (void)widget;
    lr_window_state_save_now(self->win_state);
}

void lr_main_window_restore_state(LrMainWindow *self)
{
    char *last_path = NULL;
    gint w, h, x, y;

    if (!lr_window_state_restore(self->win_state, &last_path))
        return;

    lr_window_state_get_size(self->win_state, &w, &h);
    if (w > 0 && h > 0)
        gtk_window_resize(GTK_WINDOW(self->window), w, h);

    lr_window_state_get_pos(self->win_state, &x, &y);
    gtk_window_move(GTK_WINDOW(self->window), x, y);

    if (lr_window_state_is_maximized(self->win_state))
        gtk_window_maximize(GTK_WINDOW(self->window));

    if (last_path != NULL)
    {
        g_free(self->pending_path);
        self->pending_path = last_path;
        g_idle_add(on_reveal_path_idle, self);
    }
}

LrMainWindow *
lr_main_window_new(void)
{
    LrMainWindow *mw = g_new0(LrMainWindow, 1);
    GtkWidget *vbox, *menubar, *location_bar, *paned;

    mw->win_state = lr_window_state_new("linux-regedit");
    /* 与默认窗口大小一致，首次关闭前缓存有效 */
    lr_window_state_set_size(mw->win_state, 920, 600);

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

    /* 窗口显示（show_all）后主循环首个 idle 即把焦点给树视图 */
    g_idle_add(on_focus_tree_idle, mw);

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
    lr_window_state_free(self->win_state);
    lr_tree_pane_free(self->tree);
    lr_value_pane_free(self->value);
    g_free(self);
}
