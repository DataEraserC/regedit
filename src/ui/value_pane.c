#include "ui/value_pane.h"
#include "core/format.h"

#include <string.h>
#include <json-glib/json-glib.h>

/* 底部 man 说明面板的固定高度（像素） */
#define LR_INFO_HEIGHT 200

enum
{
    COL_ENABLED = 0,
    COL_NAME,
    COL_TYPE,
    COL_DATA,
    COL_COMMENT,
    N_COLS
};

/* JSON 树形列表的列 */
enum
{
    COL_J_NAME = 0,
    COL_J_TYPE,
    COL_J_DATA,
    COL_J_N
};

struct _LrValuePane
{
    GtkWidget *widget; /* 根容器：GtkBox（页面栈） */
    GtkWidget *stack;  /* GtkStack：empty / table / text */
    GtkWidget *table_page;
    GtkWidget *text_page;
    GtkTreeView *view;
    GtkTreeStore *store;
    GtkTextView *text;
    GtkLabel *info_title;   /* 底部说明面板标题 */
    GtkTextView *info_text; /* 底部说明面板内容 */
    GtkWidget *info_page;   /* 底部说明面板容器 */
    GtkWidget *json_page;   /* JSON 树形页 */
    GtkTreeView *json_view;
    GtkTreeStore *json_store;
    char *current_basename; /* 当前配置文件 basename（用于 man 5 查询） */
    char *current_name;     /* 当前选中配置项名（用于过滤段落） */
    GHashTable *man_pages;  /* basename → ManPage*（整页文本 + 是否有页） */
};

/* 一个配置文件的 man 页缓存（整页文本 + 是否存在该页） */
typedef struct
{
    gchar *text;    /* man 5 输出整页文本（found 为 TRUE 时） */
    gboolean found; /* 该文件是否有 man 页 */
} ManPage;

/* 一次 man 查询请求（携带发起时的文件名与配置项名，避免异步竞态） */
typedef struct
{
    LrValuePane *self;
    gchar *basename;
    gchar *name;
} ManRequest;

static void
man_page_free(gpointer p)
{
    ManPage *mp = p;
    if (mp == NULL)
        return;
    g_free(mp->text);
    g_free(mp);
}

/* 从 man 页文本中提取以配置项名开头的段落；返回新分配文本，未找到返回 NULL */
static gchar *
filter_man_paragraphs(const char *page, const char *name)
{
    gchar **paras = g_strsplit(page, "\n\n", -1);
    GString *result = g_string_new(NULL);
    gchar *out = NULL;
    guint i;
    gsize n = strlen(name);

    for (i = 0; paras[i] != NULL; i++)
    {
        const char *p = paras[i];

        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, name, n) == 0 &&
            (p[n] == '\0' || p[n] == '\n' || p[n] == ' ' || p[n] == '\t'))
        {
            g_string_append(result, paras[i]);
            g_string_append(result, "\n\n");
        }
    }
    g_strfreev(paras);

    if (result->len > 0)
    {
        g_strstrip(result->str);
        out = g_strdup(result->str);
    }
    g_string_free(result, TRUE);
    return out;
}

static void
on_man_done(GObject *source, GAsyncResult *res, gpointer user_data)
{
    ManRequest *req = user_data;
    LrValuePane *self = req->self;
    GSubprocess *proc = G_SUBPROCESS(source);
    GBytes *out = NULL, *err_out = NULL;
    GError *error = NULL;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(self->info_text);
    gboolean is_current = g_strcmp0(self->current_basename, req->basename) == 0 &&
                          g_strcmp0(self->current_name, req->name) == 0;

    if (!g_subprocess_communicate_finish(proc, res, &out, &err_out, &error))
    {
        g_clear_error(&error);
        /* 失败：缓存负面结果，避免反复重试 */
        if (!g_hash_table_contains(self->man_pages, req->basename))
        {
            ManPage *mp = g_new0(ManPage, 1);
            mp->found = FALSE;
            g_hash_table_insert(self->man_pages, g_strdup(req->basename), mp);
        }
        if (is_current)
            gtk_text_buffer_set_text(buf, "man 查询失败。", -1);
    }
    else if (out != NULL && g_bytes_get_size(out) > 0)
    {
        gsize len = g_bytes_get_size(out);
        const gchar *data = g_bytes_get_data(out, &len);
        ManPage *mp;

        /* 整页文本缓存（按 basename）：供该文件所有配置项共享一次拉取 */
        mp = g_new0(ManPage, 1);
        mp->found = TRUE;
        mp->text = g_strndup(data, (gsize)MIN(len, G_MAXINT));
        g_hash_table_replace(self->man_pages, g_strdup(req->basename), mp);

        /* 仅当仍是最新请求时才写面板 */
        if (is_current)
        {
            gchar *filtered = filter_man_paragraphs(mp->text, req->name);
            if (filtered != NULL)
            {
                gtk_text_buffer_set_text(buf, filtered, -1);
                g_free(filtered);
            }
            else
            {
                gchar *msg = g_strdup_printf("未找到「%s」的说明。", req->name);
                gtk_text_buffer_set_text(buf, msg, -1);
                g_free(msg);
            }
        }
    }
    else
    {
        /* 无 man 页：缓存负面结果，下次直接提示不再启动进程 */
        if (!g_hash_table_contains(self->man_pages, req->basename))
        {
            ManPage *mp = g_new0(ManPage, 1);
            mp->found = FALSE;
            g_hash_table_insert(self->man_pages, g_strdup(req->basename), mp);
        }
        if (is_current)
            gtk_text_buffer_set_text(buf, "未找到该名称的 man 手册页。", -1);
    }

    g_free(req->basename);
    g_free(req->name);
    g_free(req);

    g_clear_pointer(&out, g_bytes_unref);
    g_clear_pointer(&err_out, g_bytes_unref);
    g_object_unref(proc);
}

static void
lr_value_pane_show_man(LrValuePane *self, const char *name)
{
    gchar *quoted, *cmd;
    GError *error = NULL;
    GSubprocess *proc;
    ManRequest *req;
    ManPage *mp;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(self->info_text);
    gchar *title = g_strdup_printf("说明：%s（man 5 %s）", name,
                                   self->current_basename != NULL
                                       ? self->current_basename
                                       : "");

    g_free(self->current_name);
    self->current_name = g_strdup(name);

    gtk_label_set_text(self->info_title, title);
    g_free(title);

    if (self->current_basename == NULL)
    {
        gtk_text_buffer_set_text(buf, "无法确定配置文件名。", -1);
        return;
    }

    /* 整页缓存（按 basename）命中：内存过滤显示，不再启动 man 子进程 */
    mp = g_hash_table_lookup(self->man_pages, self->current_basename);
    if (mp != NULL)
    {
        if (mp->found)
        {
            gchar *filtered = filter_man_paragraphs(mp->text, name);
            if (filtered != NULL)
            {
                gtk_text_buffer_set_text(buf, filtered, -1);
                g_free(filtered);
            }
            else
            {
                gchar *msg = g_strdup_printf("未找到「%s」的说明。", name);
                gtk_text_buffer_set_text(buf, msg, -1);
                g_free(msg);
            }
        }
        else
        {
            gtk_text_buffer_set_text(buf, "未找到该名称的 man 手册页。", -1);
        }
        return;
    }

    gtk_text_buffer_set_text(buf, "正在查询 man ...", -1);

    /* --no-hyphenation 禁用断词（避免 LOCAL1 → LO‐+换行+CAL1 的 "‐ 换行"）；
     * --no-justification 禁用两端对齐（避免多余空格）。 */
    quoted = g_shell_quote(self->current_basename);
    cmd = g_strdup_printf("man --no-hyphenation --no-justification 5 %s "
                          "2>/dev/null | col -b",
                          quoted);
    g_free(quoted);

    proc = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error,
                            "sh", "-c", cmd, NULL);
    g_free(cmd);

    if (proc == NULL)
    {
        gtk_text_buffer_set_text(buf, "无法启动 man 查询。", -1);
        g_clear_error(&error);
        return;
    }

    /* 请求携带发起时的文件名与配置项名：回调据此过滤并判断是否仍是最新 */
    req = g_new0(ManRequest, 1);
    req->self = self;
    req->basename = g_strdup(self->current_basename);
    req->name = g_strdup(name);
    g_subprocess_communicate_async(proc, NULL, NULL, on_man_done, req);
}

/* 窗口尺寸变化时保持说明面板为固定高度（而非随比例伸缩） */
static void
on_paned_allocate(GtkWidget *widget, GdkRectangle *alloc, gpointer user_data)
{
    LrValuePane *self = user_data;
    gint pos, cur;

    (void)widget;

    pos = alloc->height - LR_INFO_HEIGHT;
    if (pos <= 100)
        return;
    cur = gtk_paned_get_position(GTK_PANED(self->widget));
    if (pos != cur)
        gtk_paned_set_position(GTK_PANED(self->widget), pos);
}

/* 说明面板被显示时：若无选中行则隐藏（避免 show_all 等强制显示） */
static void
on_info_map(GtkWidget *widget, gpointer user_data)
{
    LrValuePane *self = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    (void)widget;

    if (!gtk_tree_selection_get_selected(
            gtk_tree_view_get_selection(self->view), &model, &iter))
        gtk_widget_hide(self->info_page);
}

/* 表格选中行：查询该配置项名称的 man 说明 */
static void
on_table_selection_changed(GtkTreeSelection *sel, gpointer user_data)
{
    LrValuePane *self = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *name = NULL;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter))
    {
        /* 未选中：隐藏说明面板 */
        if (self->info_page != NULL)
            gtk_widget_hide(self->info_page);
        return;
    }

    /* 选中：显示说明面板 */
    if (self->info_page != NULL)
        gtk_widget_show(self->info_page);

    gtk_tree_model_get(model, &iter, COL_NAME, &name, -1);
    if (name != NULL && *name != '\0')
    {
        gchar *type = NULL;
        gtk_tree_model_get(model, &iter, COL_TYPE, &type, -1);
        /* 节行（Section）不查询 man */
        if (g_strcmp0(type, "Section") != 0)
            lr_value_pane_show_man(self, name);
        g_free(type);
    }
    g_free(name);
}

/* 以文本视图展示内容（content 为 NULL 表示读取失败） */
static void
show_text_content(LrValuePane *self, const char *content, gsize length)
{
    GtkTextBuffer *buf = gtk_text_view_get_buffer(self->text);

    gtk_text_buffer_set_text(buf, "", -1);
    if (content == NULL)
        gtk_text_buffer_set_text(buf, "无法读取文件。", -1);
    else
        gtk_text_buffer_set_text(buf, content, (gint)MIN(length, G_MAXINT));

    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "text");
}

/* 递归将 JSON 节点加入树形列表 */
static void
add_json_node(LrValuePane *self, JsonNode *node, const char *key,
              GtkTreeIter *parent)
{
    JsonNodeType type = JSON_NODE_TYPE(node);
    GtkTreeIter iter;

    if (type == JSON_NODE_OBJECT)
    {
        GtkTreeIter *container = parent;
        GtkTreeIter obj_iter;
        JsonObject *obj;
        GList *members, *l;

        if (key != NULL)
        {
            gtk_tree_store_append(self->json_store, &obj_iter, parent);
            gtk_tree_store_set(self->json_store, &obj_iter,
                               COL_J_NAME, key,
                               COL_J_TYPE, "Object",
                               COL_J_DATA, "",
                               -1);
            container = &obj_iter;
        }

        obj = json_node_get_object(node);
        members = json_object_get_members(obj);
        for (l = members; l != NULL; l = l->next)
        {
            const char *mkey = l->data;
            add_json_node(self, json_object_get_member(obj, mkey), mkey,
                          container);
        }
        g_list_free(members);
        return;
    }

    if (type == JSON_NODE_ARRAY)
    {
        JsonArray *arr = json_node_get_array(node);
        guint len = json_array_get_length(arr);
        guint i;
        gchar *label = g_strdup_printf("%s[Array]",
                                       key != NULL ? key : "root");

        gtk_tree_store_append(self->json_store, &iter, parent);
        gtk_tree_store_set(self->json_store, &iter,
                           COL_J_NAME, label,
                           COL_J_TYPE, "Array",
                           COL_J_DATA, "",
                           -1);
        g_free(label);

        for (i = 0; i < len; i++)
        {
            gchar *ikey = g_strdup_printf("[%u]", i);
            add_json_node(self, json_array_get_element(arr, i), ikey, &iter);
            g_free(ikey);
        }
        return;
    }

    /* 标量值（json-glib 1.10 直接在 JsonNode 上取值） */
    {
        const char *type_name = "";
        gchar *data = g_strdup("");

        if (type == JSON_NODE_VALUE)
        {
            GType vtype = json_node_get_value_type(node);

            if (json_node_is_null(node))
            {
                type_name = "Null";
                g_free(data);
                data = g_strdup("null");
            }
            else if (vtype == G_TYPE_BOOLEAN)
            {
                type_name = "Boolean";
                g_free(data);
                data = g_strdup(json_node_get_boolean(node) ? "true" : "false");
            }
            else if (vtype == G_TYPE_INT64 || vtype == G_TYPE_INT)
            {
                type_name = "Number";
                g_free(data);
                data = g_strdup_printf("%" G_GINT64_FORMAT,
                                       json_node_get_int(node));
            }
            else if (vtype == G_TYPE_DOUBLE)
            {
                type_name = "Number";
                g_free(data);
                data = g_strdup_printf("%g", json_node_get_double(node));
            }
            else
            {
                type_name = "String";
                g_free(data);
                data = g_strdup(json_node_get_string(node) != NULL
                                    ? json_node_get_string(node)
                                    : "");
            }
        }

        gtk_tree_store_append(self->json_store, &iter, parent);
        gtk_tree_store_set(self->json_store, &iter,
                           COL_J_NAME, key != NULL ? key : "",
                           COL_J_TYPE, type_name,
                           COL_J_DATA, data,
                           -1);
        g_free(data);
    }
}

static void
lr_value_pane_load_json(LrValuePane *self, const char *content, gsize length)
{
    JsonParser *parser = json_parser_new();
    GError *error = NULL;

    gtk_tree_store_clear(self->json_store);

    if (content != NULL &&
        json_parser_load_from_data(parser, content, (gssize)length, &error))
    {
        JsonNode *root = json_parser_get_root(parser);
        gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "json");
        add_json_node(self, root, NULL, NULL);
        gtk_tree_view_expand_all(self->json_view);
    }
    else
    {
        show_text_content(self, content, length);
    }

    if (error != NULL)
        g_error_free(error);
    g_object_unref(parser);
}

void lr_value_pane_load_file(LrValuePane *self, const char *path)
{
    gchar *content = NULL;
    gsize len = 0;
    GError *error = NULL;
    LrConfigFormat fmt;

    g_free(self->current_basename);
    self->current_basename = g_path_get_basename(path);

    /* 一次性读取内容，检测与解析复用，避免重复读文件 */
    if (!g_file_get_contents(path, &content, &len, &error))
    {
        gchar *msg = g_strdup_printf("无法读取文件：%s",
                                     error != NULL ? error->message
                                                   : "未知错误");
        show_text_content(self, msg, strlen(msg));
        g_free(msg);
        g_clear_error(&error);
        return;
    }

    fmt = lr_format_detect_content(path, content, len);
    if (!lr_format_supported(fmt))
    {
        show_text_content(self, content, len);
        g_free(content);
        return;
    }

    /* JSON：以树形列表展示 */
    if (fmt == LR_FORMAT_JSON)
    {
        lr_value_pane_load_json(self, content, len);
        g_free(content);
        return;
    }

    {
        LrConfigFile *file = lr_parse_config_content(path, content, len);
        guint i;

        if (!file->parsed)
        {
            show_text_content(self, content, len);
            g_free(content);
            lr_config_file_free(file);
            return;
        }
        g_free(content);

        gtk_tree_store_clear(self->store);

        /* 按节路径（:: 分隔）逐级创建可展开节点：
         * INI/systemd 单段节 → [节名]；apt 嵌套路径 → 每段一个节点 */
        {
            GHashTable *nodes = g_hash_table_new_full(
                g_str_hash, g_str_equal, g_free, g_free);

            for (i = 0; i < file->items->len; i++)
            {
                LrConfigItem *item = g_ptr_array_index(file->items, i);
                GtkTreeIter iter;
                GtkTreeIter *parent = NULL;

                if (item->section != NULL && *item->section != '\0')
                {
                    gchar **segs = g_strsplit(item->section, "::", -1);
                    gboolean multi = strstr(item->section, "::") != NULL;
                    GString *cur = g_string_new(NULL);
                    guint j;

                    for (j = 0; segs[j] != NULL; j++)
                    {
                        gchar *seg = g_strstrip(segs[j]);
                        GtkTreeIter *node;
                        gchar *display;

                        if (*seg == '\0')
                            continue;
                        if (cur->len > 0)
                            g_string_append(cur, "::");
                        g_string_append(cur, seg);

                        display = multi
                                      ? g_strdup(seg)
                                      : g_strdup_printf("[%s]", seg);
                        node = g_hash_table_lookup(nodes, cur->str);
                        if (node == NULL)
                        {
                            GtkTreeIter sit;
                            gtk_tree_store_append(self->store, &sit, parent);
                            gtk_tree_store_set(self->store, &sit,
                                               COL_ENABLED, "",
                                               COL_NAME, display,
                                               COL_TYPE, "Section",
                                               COL_DATA, "",
                                               COL_COMMENT, "",
                                               -1);
                            node = g_memdup2(&sit, sizeof(GtkTreeIter));
                            g_hash_table_insert(nodes, g_strdup(cur->str),
                                                node);
                        }
                        parent = node;
                        g_free(display);
                    }
                    g_strfreev(segs);
                    g_string_free(cur, TRUE);
                }

                gtk_tree_store_append(self->store, &iter, parent);
                gtk_tree_store_set(self->store, &iter,
                                   COL_ENABLED,
                                   item->enabled ? "true" : "false",
                                   COL_NAME, item->key,
                                   COL_TYPE, lr_value_type_name(item->type),
                                   COL_DATA, item->data,
                                   COL_COMMENT, item->comment != NULL ? item->comment : "",
                                   -1);
            }

            g_hash_table_destroy(nodes);
        }

        /* 默认展开所有节 */
        gtk_tree_view_expand_all(self->view);

        gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "table");
        lr_config_file_free(file);
    }
}

void lr_value_pane_clear(LrValuePane *self)
{
    gtk_tree_store_clear(self->store);
    gtk_tree_store_clear(self->json_store);
    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "empty");
}

/* 追加一列文本列：title 表头，col 模型列，expand 是否随宽度扩展，
 * gray 是否灰色前景，min_width 最小宽度（0 表示不设） */
static void
append_text_column(GtkTreeView *view, const char *title, gint col,
                   gboolean expand, gboolean gray, gint min_width)
{
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    gtk_tree_view_column_set_title(column, title);
    gtk_tree_view_column_set_resizable(column, TRUE);
    if (gray)
        g_object_set(renderer, "foreground", "gray", NULL);
    gtk_tree_view_column_pack_start(column, renderer, expand);
    gtk_tree_view_column_add_attribute(column, renderer, "text", col);
    if (min_width > 0)
        gtk_tree_view_column_set_min_width(column, min_width);
    gtk_tree_view_append_column(view, column);
}

LrValuePane *
lr_value_pane_new(void)
{
    LrValuePane *self = g_new0(LrValuePane, 1);
    GtkWidget *scrolled;
    GtkWidget *label;

    self->man_pages = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            man_page_free);

    self->widget = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

    self->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(self->stack),
                                  GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_paned_pack1(GTK_PANED(self->widget), self->stack, TRUE, FALSE);

    /* --- 占位页（未选择时保持空白，不显示提示） --- */
    label = gtk_label_new("");
    gtk_stack_add_named(GTK_STACK(self->stack), label, "empty");

    /* --- 表格页 --- */
    self->table_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    self->store = gtk_tree_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
                                     G_TYPE_STRING, G_TYPE_STRING,
                                     G_TYPE_STRING);
    self->view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(self->store)));
    g_object_unref(self->store);

    gtk_tree_view_set_grid_lines(self->view, GTK_TREE_VIEW_GRID_LINES_VERTICAL);
    g_signal_connect(gtk_tree_view_get_selection(self->view), "changed",
                     G_CALLBACK(on_table_selection_changed), self);

    append_text_column(self->view, "启用", COL_ENABLED, FALSE, FALSE, 0);
    append_text_column(self->view, "名称", COL_NAME, TRUE, FALSE, 160);
    append_text_column(self->view, "类型", COL_TYPE, FALSE, FALSE, 0);
    append_text_column(self->view, "数据", COL_DATA, TRUE, FALSE, 0);
    append_text_column(self->view, "备注", COL_COMMENT, TRUE, TRUE, 0);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(self->view));
    gtk_box_pack_start(GTK_BOX(self->table_page), scrolled, TRUE, TRUE, 0);
    gtk_stack_add_named(GTK_STACK(self->stack), self->table_page, "table");

    /* --- 文本页 --- */
    self->text_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    self->text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(self->text, FALSE);
    gtk_text_view_set_cursor_visible(self->text, FALSE);
    gtk_text_view_set_wrap_mode(self->text, GTK_WRAP_WORD_CHAR);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(self->text));
    gtk_box_pack_start(GTK_BOX(self->text_page), scrolled, TRUE, TRUE, 0);
    gtk_stack_add_named(GTK_STACK(self->stack), self->text_page, "text");

    /* --- JSON 页（树形列表，可展开；无启用/备注列） --- */
    self->json_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    self->json_store = gtk_tree_store_new(COL_J_N, G_TYPE_STRING,
                                          G_TYPE_STRING, G_TYPE_STRING);
    self->json_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(self->json_store)));
    g_object_unref(self->json_store);

    append_text_column(self->json_view, "名称", COL_J_NAME, TRUE, FALSE, 0);
    append_text_column(self->json_view, "类型", COL_J_TYPE, FALSE, FALSE, 0);
    append_text_column(self->json_view, "数据", COL_J_DATA, TRUE, FALSE, 0);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(self->json_view));
    gtk_box_pack_start(GTK_BOX(self->json_page), scrolled, TRUE, TRUE, 0);
    gtk_stack_add_named(GTK_STACK(self->stack), self->json_page, "json");

    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "empty");

    /* --- 底部信息说明面板（选中表格行时显示 man 说明） --- */
    {
        GtkWidget *info_scrolled;

        self->info_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        self->info_title = GTK_LABEL(gtk_label_new("说明"));
        gtk_widget_set_halign(GTK_WIDGET(self->info_title), GTK_ALIGN_START);
        gtk_widget_set_margin_start(GTK_WIDGET(self->info_title), 6);
        gtk_widget_set_margin_top(GTK_WIDGET(self->info_title), 4);
        gtk_widget_set_margin_bottom(GTK_WIDGET(self->info_title), 2);
        gtk_box_pack_start(GTK_BOX(self->info_page),
                           GTK_WIDGET(self->info_title), FALSE, FALSE, 0);

        self->info_text = GTK_TEXT_VIEW(gtk_text_view_new());
        gtk_text_view_set_editable(self->info_text, FALSE);
        gtk_text_view_set_wrap_mode(self->info_text, GTK_WRAP_WORD_CHAR);

        info_scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(info_scrolled),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(info_scrolled),
                          GTK_WIDGET(self->info_text));
        gtk_box_pack_start(GTK_BOX(self->info_page), info_scrolled, TRUE, TRUE,
                           0);

        gtk_paned_pack2(GTK_PANED(self->widget), self->info_page, FALSE, FALSE);
        g_signal_connect(self->widget, "size-allocate",
                         G_CALLBACK(on_paned_allocate), self);
        g_signal_connect_after(self->info_page, "map",
                               G_CALLBACK(on_info_map), self);
    }

    return self;
}

GtkWidget *
lr_value_pane_get_widget(LrValuePane *self)
{
    return self->widget;
}

void lr_value_pane_free(LrValuePane *self)
{
    if (self == NULL)
        return;
    g_free(self->current_basename);
    g_free(self->current_name);
    g_hash_table_destroy(self->man_pages);
    g_free(self);
}
