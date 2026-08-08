#include "ui/main_window.h"
#include "ui/window_state.h"

#include <gtk/gtk.h>
#include <stdio.h>

struct _LrMainWindow
{
    GtkWidget *window;
    LrTreePane *tree;
    LrValuePane *value;
    GtkWidget *location_entry;
    GtkWidget *location_bar;  /* 地址栏容器（查看→地址栏 切换） */
    char *current_path;       /* 当前打开/选中的路径 */
    char *pending_path;       /* 恢复状态时待定位的路径 */
    LrWindowState *win_state; /* 窗口几何与上次路径状态 */
};

/* 独立子窗口工具（实现在收藏夹区）：相对主窗口定位、响应后销毁 */
static void center_dialog_on(GtkWidget *dialog, GtkWindow *parent);
static void destroy_on_response(GtkDialog *dialog, gint response_id,
                                gpointer user_data);

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
    g_signal_connect(dialog, "response", G_CALLBACK(destroy_on_response), NULL);
    center_dialog_on(dialog, GTK_WINDOW(window));
}

/* ========== 收藏夹（数据存 /run 临时目录，重启清空） ========== */

/* 独立子窗口：显示并把位置定到主窗口中心（之后可自由拖动，不与主窗口联动） */
static void
center_dialog_on(GtkWidget *dialog, GtkWindow *parent)
{
    gint px, py, pw, ph, dw, dh;

    gtk_window_get_position(parent, &px, &py);
    gtk_window_get_size(parent, &pw, &ph);
    gtk_widget_show_all(dialog);
    gtk_window_get_size(GTK_WINDOW(dialog), &dw, &dh);
    gtk_window_move(GTK_WINDOW(dialog),
                    MAX(px + (pw - dw) / 2, 0),
                    MAX(py + (ph - dh) / 2, 0));
}

/* 对话框任何响应（含关闭）都销毁自身 */
static void
destroy_on_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    (void)response_id;
    (void)user_data;
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

/* 收藏夹数据目录：$XDG_RUNTIME_DIR/linux-regedit/favorites（/run/user/<uid>） */
static char *
favorites_dir(void)
{
    const char *rt = g_get_user_runtime_dir();
    if (rt == NULL)
        rt = "/tmp";
    return g_build_filename(rt, "linux-regedit", "favorites", NULL);
}

/* 收藏夹名称转安全文件名（替换 / 等） */
static char *
favorite_file_name(const char *name)
{
    return g_strdelimit(g_strdup(name), "/", '_');
}

/* 返回收藏夹名称列表（排序），需 g_list_free_full(list, g_free) */
static GList *
favorites_list(void)
{
    GList *list = NULL;
    gchar *dir = favorites_dir();
    GDir *gd = g_dir_open(dir, 0, NULL);
    const char *name;

    if (gd == NULL)
    {
        g_free(dir);
        return NULL;
    }
    while ((name = g_dir_read_name(gd)) != NULL)
        list = g_list_append(list, g_strdup(name));
    g_dir_close(gd);
    g_free(dir);
    return g_list_sort(list, (GCompareFunc)g_utf8_collate);
}

static gboolean
favorites_add(const char *name, const char *path)
{
    gchar *dir = favorites_dir();
    gchar *file = g_build_filename(dir, favorite_file_name(name), NULL);
    gboolean ok;

    g_mkdir_with_parents(dir, 0700);
    ok = g_file_set_contents(file, path, -1, NULL);
    g_free(file);
    g_free(dir);
    return ok;
}

static gboolean
favorites_remove(const char *name)
{
    gchar *dir = favorites_dir();
    gchar *file = g_build_filename(dir, favorite_file_name(name), NULL);
    gboolean ok = (remove(file) == 0);
    g_free(file);
    g_free(dir);
    return ok;
}

/* 读取收藏夹对应路径（新分配，调用者 g_free） */
static char *
favorites_path(const char *name)
{
    gchar *dir = favorites_dir();
    gchar *file = g_build_filename(dir, favorite_file_name(name), NULL);
    gchar *path = NULL;

    g_file_get_contents(file, &path, NULL, NULL);
    g_free(file);
    g_free(dir);
    return path;
}

/* 点击收藏夹项：跳转到收藏路径 */
static void
on_favorite_activate(GtkMenuItem *item, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    const gchar *name = gtk_menu_item_get_label(item);
    gchar *path;

    if (name == NULL)
        return;
    path = favorites_path(name);
    if (path != NULL)
    {
        open_path(mw, path, g_file_test(path, G_FILE_TEST_IS_DIR));
        /* 树中逐级展开并选中对应节点 */
        lr_tree_pane_reveal_path(mw->tree, path);
        g_free(path);
    }
}

/* 添加到收藏夹对话框 */
typedef struct
{
    LrMainWindow *mw;
    GtkWidget *entry;
} AddFavoriteCtx;

static void
on_add_fav_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    AddFavoriteCtx *ctx = user_data;

    if (response_id == GTK_RESPONSE_ACCEPT)
    {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(ctx->entry));
        if (name != NULL && *name != '\0')
            favorites_add(name, ctx->mw->current_path);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
    g_free(ctx);
}

static void
on_add_favorite(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    AddFavoriteCtx *ctx;
    GtkWidget *dialog, *box, *label, *entry, *area;
    gchar *base;

    (void)widget;
    if (mw->current_path == NULL || *mw->current_path == '\0')
        return;

    dialog = gtk_dialog_new_with_buttons("添加到收藏夹", NULL, 0, "取消",
                                         GTK_RESPONSE_CANCEL, "确定",
                                         GTK_RESPONSE_ACCEPT, NULL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);

    label = gtk_label_new_with_mnemonic("收藏夹名称(_F):");
    entry = gtk_entry_new();
    base = g_path_get_basename(mw->current_path);
    gtk_entry_set_text(GTK_ENTRY(entry), base);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    gtk_widget_set_halign(label, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);

    area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(area), box, TRUE, TRUE, 0);

    ctx = g_new0(AddFavoriteCtx, 1);
    ctx->mw = mw;
    ctx->entry = entry;
    g_signal_connect(dialog, "response", G_CALLBACK(on_add_fav_response), ctx);

    center_dialog_on(dialog, GTK_WINDOW(mw->window));
    g_free(base);
}

/* 删除收藏夹对话框 */
static void
on_remove_fav_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    GtkWidget *combo = user_data;

    if (response_id == GTK_RESPONSE_ACCEPT)
    {
        gchar *name =
            gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
        if (name != NULL)
        {
            favorites_remove(name);
            g_free(name);
        }
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
on_remove_favorite(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    GtkWidget *dialog, *box, *label, *combo, *area;
    GList *favs;

    (void)widget;
    favs = favorites_list();
    if (favs == NULL)
    {
        GtkWidget *d = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO,
                                              GTK_BUTTONS_OK, "还没有收藏夹。");
        g_signal_connect(d, "response", G_CALLBACK(destroy_on_response), NULL);
        center_dialog_on(d, GTK_WINDOW(mw->window));
        return;
    }

    dialog = gtk_dialog_new_with_buttons("删除收藏夹", NULL, 0, "取消",
                                         GTK_RESPONSE_CANCEL, "确定",
                                         GTK_RESPONSE_ACCEPT, NULL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    label = gtk_label_new("选择收藏夹:");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    combo = gtk_combo_box_text_new();
    {
        GList *l;
        for (l = favs; l != NULL; l = l->next)
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
                                           (const gchar *)l->data);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), combo, FALSE, FALSE, 0);

    area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(area), box, TRUE, TRUE, 0);

    g_signal_connect(dialog, "response", G_CALLBACK(on_remove_fav_response),
                     combo);
    center_dialog_on(dialog, GTK_WINDOW(mw->window));
    g_list_free_full(favs, g_free);
}

/* 收藏夹菜单显示时重建：添加/删除 + 分割线 + 收藏项 */
static void
on_favorites_menu_show(GtkWidget *menu, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    GList *l;

    for (l = children; l != NULL; l = l->next)
        gtk_container_remove(GTK_CONTAINER(menu), GTK_WIDGET(l->data));
    g_list_free(children);

    GtkWidget *item = gtk_menu_item_new_with_label("添加到收藏夹…");
    g_signal_connect(item, "activate", G_CALLBACK(on_add_favorite), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("删除收藏夹…");
    g_signal_connect(item, "activate", G_CALLBACK(on_remove_favorite), mw);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    GList *favs = favorites_list();
    if (favs != NULL)
    {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                              gtk_separator_menu_item_new());
        for (l = favs; l != NULL; l = l->next)
        {
            item = gtk_menu_item_new_with_label((const gchar *)l->data);
            g_signal_connect(item, "activate", G_CALLBACK(on_favorite_activate),
                             mw);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        }
        g_list_free_full(favs, g_free);
    }
    gtk_widget_show_all(menu);
}

/* ========== 导出（.lreg） ========== */

static void
export_file_into(GString *out, const char *path)
{
    gchar *content = NULL;
    gsize len = 0;

    if (!g_file_get_contents(path, &content, &len, NULL))
        return;
    if (len > 128 * 1024 || !g_utf8_validate(content, len, NULL))
    {
        g_free(content);
        return;
    }
    g_string_append_printf(out, "[文件: %s]\n", path);
    g_string_append(out, content);
    if (len == 0 || content[len - 1] != '\n')
        g_string_append_c(out, '\n');
    g_string_append_c(out, '\n');
    g_free(content);
}

static void
export_path_into(GString *out, const char *path)
{
    if (g_file_test(path, G_FILE_TEST_IS_DIR))
    {
        GDir *gd = g_dir_open(path, 0, NULL);
        const char *name;
        if (gd == NULL)
            return;
        g_string_append_printf(out, "[目录: %s]\n", path);
        while ((name = g_dir_read_name(gd)) != NULL)
        {
            gchar *full = g_build_filename(path, name, NULL);
            export_path_into(out, full);
            g_free(full);
        }
        g_dir_close(gd);
    }
    else
    {
        export_file_into(out, path);
    }
}

static gboolean
do_export(LrMainWindow *mw, gboolean all, const char *dest, GError **err)
{
    GString *out = g_string_new("Linux Registry Export Version 1.0\n");
    GDateTime *now = g_date_time_new_now_local();
    gchar *ts = g_date_time_format(now, "%Y-%m-%d %H:%M:%S");
    g_string_append_printf(out, "生成时间: %s\n\n", ts);
    g_free(ts);
    g_date_time_unref(now);

    if (all)
    {
        export_path_into(out, "/etc");
        gchar *cfg = g_build_filename(g_get_home_dir(), ".config", NULL);
        export_path_into(out, cfg);
        g_free(cfg);
        export_path_into(out, "/boot");
    }
    else
    {
        if (mw->current_path == NULL || *mw->current_path == '\0')
        {
            g_set_error_literal(err, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                "当前没有选中的路径。");
            g_string_free(out, TRUE);
            return FALSE;
        }
        export_path_into(out, mw->current_path);
    }

    gboolean ok = g_file_set_contents(dest, out->str, (gssize)out->len, err);
    g_string_free(out, TRUE);
    return ok;
}

typedef struct
{
    LrMainWindow *mw;
    GtkWidget *radio_all;
    GtkWidget *entry; /* 保存路径输入框 */
} ExportCtx;

/* 浏览…：打开保存对话框选择导出位置 */
static void
on_export_browse(GtkWidget *button, gpointer user_data)
{
    ExportCtx *ctx = user_data;
    GtkWidget *dialog;
    gint resp;
    (void)button;

    dialog = gtk_file_chooser_dialog_new(
        "导出到…", NULL, GTK_FILE_CHOOSER_ACTION_SAVE, "取消",
        GTK_RESPONSE_CANCEL, "保存", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                      "linux-regedit.lreg");

    resp = gtk_dialog_run(GTK_DIALOG(dialog));
    if (resp == GTK_RESPONSE_ACCEPT)
    {
        gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (path != NULL)
        {
            gtk_entry_set_text(GTK_ENTRY(ctx->entry), path);
            g_free(path);
        }
    }
    gtk_widget_destroy(dialog);
}

static void
on_export_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    ExportCtx *ctx = user_data;
    gboolean all;
    gchar *dest = NULL;
    GError *err = NULL;

    if (response_id != GTK_RESPONSE_ACCEPT)
    {
        gtk_widget_destroy(GTK_WIDGET(dialog));
        g_free(ctx);
        return;
    }

    all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctx->radio_all));
    dest = g_strdup(gtk_entry_get_text(GTK_ENTRY(ctx->entry)));

    if (dest == NULL || *dest == '\0')
    {
        GtkWidget *d = gtk_message_dialog_new(
            NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "请输入保存位置。");
        g_signal_connect(d, "response", G_CALLBACK(destroy_on_response), NULL);
        center_dialog_on(d, GTK_WINDOW(ctx->mw->window));
    }
    else if (do_export(ctx->mw, all, dest, &err))
    {
        GtkWidget *d = gtk_message_dialog_new(
            NULL, 0, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "导出完成：%s", dest);
        g_signal_connect(d, "response", G_CALLBACK(destroy_on_response), NULL);
        center_dialog_on(d, GTK_WINDOW(ctx->mw->window));
    }
    else
    {
        GtkWidget *d = gtk_message_dialog_new(
            NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "导出失败：%s",
            err != NULL ? err->message : "未知错误");
        g_signal_connect(d, "response", G_CALLBACK(destroy_on_response), NULL);
        center_dialog_on(d, GTK_WINDOW(ctx->mw->window));
        g_clear_error(&err);
    }
    g_free(dest);
    gtk_widget_destroy(GTK_WIDGET(dialog));
    g_free(ctx);
}

static void
on_export(GtkWidget *widget, gpointer user_data)
{
    LrMainWindow *mw = user_data;
    ExportCtx *ctx;
    GtkWidget *dialog, *box, *label, *r1, *r2, *area;
    GtkWidget *hbox, *entry, *browse;

    (void)widget;
    dialog = gtk_dialog_new_with_buttons("导出", NULL, 0, "取消",
                                         GTK_RESPONSE_CANCEL, "确定",
                                         GTK_RESPONSE_ACCEPT, NULL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);

    label = gtk_label_new("导出范围:");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    r1 = gtk_radio_button_new_with_label(NULL,
                                         "全部（/etc、~/.config、/boot）");
    r2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(r1),
                                                     "当前选中的目录");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r1), TRUE);

    /* 保存路径：输入框 + 浏览按钮（GtkFileChooserButton 不支持 SAVE） */
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "linux-regedit.lreg");
    browse = gtk_button_new_with_label("浏览…");

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), browse, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), r1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), r2, FALSE, FALSE, 0);
    gtk_box_pack_start(
        GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
        FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new("保存到:"), FALSE, FALSE,
                       0);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

    area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(area), box, TRUE, TRUE, 0);

    ctx = g_new0(ExportCtx, 1);
    ctx->mw = mw;
    ctx->radio_all = r1;
    ctx->entry = entry;
    g_signal_connect(browse, "clicked", G_CALLBACK(on_export_browse), ctx);
    g_signal_connect(dialog, "response", G_CALLBACK(on_export_response), ctx);
    center_dialog_on(dialog, GTK_WINDOW(mw->window));
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
    g_signal_connect(item, "activate", G_CALLBACK(on_export), mw);
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
    g_signal_connect(menu, "show", G_CALLBACK(on_favorites_menu_show), mw);
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
