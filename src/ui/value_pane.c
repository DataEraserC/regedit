#include "ui/value_pane.h"
#include "core/format.h"

enum
{
    COL_ENABLED = 0,
    COL_NAME,
    COL_TYPE,
    COL_DATA,
    COL_COMMENT,
    N_COLS
};

struct _LrValuePane
{
    GtkWidget *widget; /* 根容器：GtkBox（页面栈） */
    GtkWidget *stack;  /* GtkStack：empty / table / text */
    GtkWidget *table_page;
    GtkWidget *text_page;
    GtkTreeView *view;
    GtkListStore *store;
    GtkTextView *text;
};

static void
show_text(LrValuePane *self, const char *path)
{
    GError *error = NULL;
    gchar *content = NULL;
    gsize length = 0;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(self->text);

    gtk_text_buffer_set_text(buf, "", -1);

    if (!g_file_get_contents(path, &content, &length, &error))
    {
        gchar *msg = g_strdup_printf("无法读取文件：%s",
                                     error != NULL ? error->message
                                                   : "未知错误");
        gtk_text_buffer_set_text(buf, msg, -1);
        g_free(msg);
        g_clear_error(&error);
    }
    else
    {
        gtk_text_buffer_set_text(buf, content, (gint)MIN(length, G_MAXINT));
        g_free(content);
    }

    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "text");
}

void lr_value_pane_load_file(LrValuePane *self, const char *path)
{
    LrConfigFormat fmt = lr_format_detect(path);

    if (!lr_format_supported(fmt))
    {
        show_text(self, path);
        return;
    }

    {
        LrConfigFile *file = lr_parse_config(path);
        guint i;

        if (!file->parsed)
        {
            show_text(self, path);
            lr_config_file_free(file);
            return;
        }

        gtk_list_store_clear(self->store);
        for (i = 0; i < file->items->len; i++)
        {
            LrConfigItem *item = g_ptr_array_index(file->items, i);
            GtkTreeIter iter;

            gtk_list_store_append(self->store, &iter);
            gtk_list_store_set(self->store, &iter,
                               COL_ENABLED, item->enabled ? "true" : "false",
                               COL_NAME, item->key,
                               COL_TYPE, lr_value_type_name(item->type),
                               COL_DATA, item->data,
                               COL_COMMENT, item->comment != NULL ? item->comment : "",
                               -1);
        }

        gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "table");
        lr_config_file_free(file);
    }
}

void lr_value_pane_clear(LrValuePane *self)
{
    gtk_list_store_clear(self->store);
    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "empty");
}

LrValuePane *
lr_value_pane_new(void)
{
    LrValuePane *self = g_new0(LrValuePane, 1);
    GtkWidget *scrolled;
    GtkWidget *label;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    self->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    self->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(self->stack),
                                  GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_box_pack_start(GTK_BOX(self->widget), self->stack, TRUE, TRUE, 0);

    /* --- 占位页（未选择时保持空白，不显示提示） --- */
    label = gtk_label_new("");
    gtk_stack_add_named(GTK_STACK(self->stack), label, "empty");

    /* --- 表格页 --- */
    self->table_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    self->store = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
                                     G_TYPE_STRING, G_TYPE_STRING,
                                     G_TYPE_STRING);
    self->view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(self->store)));
    g_object_unref(self->store);

    gtk_tree_view_set_grid_lines(self->view, GTK_TREE_VIEW_GRID_LINES_VERTICAL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "启用");
    gtk_tree_view_column_set_resizable(column, TRUE);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COL_ENABLED);
    gtk_tree_view_append_column(self->view, column);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "名称");
    gtk_tree_view_column_set_resizable(column, TRUE);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COL_NAME);
    gtk_tree_view_column_set_min_width(column, 160);
    gtk_tree_view_append_column(self->view, column);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "类型");
    gtk_tree_view_column_set_resizable(column, TRUE);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COL_TYPE);
    gtk_tree_view_append_column(self->view, column);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "数据");
    gtk_tree_view_column_set_resizable(column, TRUE);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COL_DATA);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(self->view, column);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "备注");
    gtk_tree_view_column_set_resizable(column, TRUE);
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "foreground", "gray", NULL);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COL_COMMENT);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(self->view, column);

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

    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "empty");
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
    g_free(self);
}
