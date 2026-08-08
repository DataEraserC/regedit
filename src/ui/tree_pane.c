#include "ui/tree_pane.h"
#include "core/scanner.h"

enum
{
    COL_ICON = 0,
    COL_NAME,
    COL_PATH,
    COL_KIND,
    COL_FORMAT,
    COL_LOADED,
    N_COLS
};

struct _LrTreePane
{
    GtkWidget *widget;
    GtkTreeView *view;
    GtkTreeStore *store;
    LrTreePaneSelectCb select_cb;
    gpointer select_data;

    GdkPixbuf *icon_folder;
    GdkPixbuf *icon_config;
    GdkPixbuf *icon_other;
};

static GdkPixbuf *
load_icon(const gchar *name, gint size)
{
    GtkIconTheme *theme = gtk_icon_theme_get_default();
    GError *error = NULL;
    GdkPixbuf *pb = gtk_icon_theme_load_icon(theme, name, size, 0, &error);
    if (pb == NULL)
    {
        g_clear_error(&error);
        return NULL;
    }
    return pb;
}

static void
add_dummy_child(LrTreePane *self, GtkTreeIter *parent)
{
    GtkTreeIter child;
    gtk_tree_store_append(self->store, &child, parent);
    gtk_tree_store_set(self->store, &child,
                       COL_ICON, NULL,
                       COL_NAME, "…",
                       COL_PATH, "",
                       COL_KIND, LR_SCAN_DIR,
                       COL_FORMAT, LR_FORMAT_UNKNOWN,
                       COL_LOADED, FALSE,
                       -1);
}

static void
add_root(LrTreePane *self, const char *label, const char *path)
{
    GtkTreeIter root;
    gtk_tree_store_append(self->store, &root, NULL);
    gtk_tree_store_set(self->store, &root,
                       COL_ICON, self->icon_folder,
                       COL_NAME, label,
                       COL_PATH, path,
                       COL_KIND, LR_SCAN_DIR,
                       COL_FORMAT, LR_FORMAT_UNKNOWN,
                       COL_LOADED, FALSE,
                       -1);
    add_dummy_child(self, &root);
}

static GdkPixbuf *
icon_for_kind(LrTreePane *self, LrScanKind kind)
{
    switch (kind)
    {
    case LR_SCAN_DIR:
        return self->icon_folder;
    case LR_SCAN_SUPPORTED_FILE:
        return self->icon_config;
    default:
        return self->icon_other;
    }
}

static void
fill_children(LrTreePane *self, GtkTreeIter *parent, const char *dirpath)
{
    GPtrArray *entries = lr_scanner_list_dir(dirpath);
    guint i;

    for (i = 0; i < entries->len; i++)
    {
        LrScanEntry *e = g_ptr_array_index(entries, i);
        GtkTreeIter child;

        gtk_tree_store_append(self->store, &child, parent);
        gtk_tree_store_set(self->store, &child,
                           COL_ICON, icon_for_kind(self, e->kind),
                           COL_NAME, e->name,
                           COL_PATH, e->path,
                           COL_KIND, e->kind,
                           COL_FORMAT, e->format,
                           COL_LOADED, FALSE,
                           -1);
        if (e->kind == LR_SCAN_DIR)
            add_dummy_child(self, &child);
    }

    g_ptr_array_unref(entries);
    gtk_tree_store_set(self->store, parent, COL_LOADED, TRUE, -1);
}

static void
on_row_expanded(GtkTreeView *view, GtkTreeIter *iter, GtkTreePath *tpath,
                gpointer user_data)
{
    LrTreePane *self = user_data;
    gboolean loaded = FALSE;
    gchar *dirpath = NULL;
    GtkTreeIter child;

    (void)view;
    (void)tpath;

    gtk_tree_model_get(GTK_TREE_MODEL(self->store), iter,
                       COL_LOADED, &loaded,
                       COL_PATH, &dirpath, -1);
    if (loaded || dirpath == NULL || *dirpath == '\0')
    {
        g_free(dirpath);
        return;
    }

    /* 移除占位的 dummy 子节点（若有） */
    if (gtk_tree_model_iter_children(GTK_TREE_MODEL(self->store), &child,
                                     iter))
    {
        gchar *cpath = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(self->store), &child,
                           COL_PATH, &cpath, -1);
        if (cpath == NULL || *cpath == '\0')
            gtk_tree_store_remove(self->store, &child);
        g_free(cpath);
    }

    fill_children(self, iter, dirpath);
    g_free(dirpath);
}

static void
on_selection_changed(GtkTreeSelection *sel, gpointer user_data)
{
    LrTreePane *self = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *path = NULL;
    gint kind = LR_SCAN_OTHER_FILE;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, COL_PATH, &path, COL_KIND, &kind, -1);
    if (self->select_cb != NULL && path != NULL && *path != '\0')
        self->select_cb(path, kind == LR_SCAN_DIR, self->select_data);
    g_free(path);
}

LrTreePane *
lr_tree_pane_new(void)
{
    LrTreePane *self = g_new0(LrTreePane, 1);
    GtkCellRenderer *pix, *txt;
    GtkTreeViewColumn *col;
    const gchar *home = g_get_home_dir();
    gchar *config = g_build_filename(home, ".config", NULL);

    /* 图标 */
    self->icon_folder = load_icon("folder", 16);
    self->icon_config = load_icon("text-x-generic", 16);
    self->icon_other = load_icon("text-x-generic", 16);

    self->widget = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(self->widget),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    self->store = gtk_tree_store_new(N_COLS,
                                     GDK_TYPE_PIXBUF, G_TYPE_STRING,
                                     G_TYPE_STRING, G_TYPE_INT,
                                     G_TYPE_INT, G_TYPE_BOOLEAN);
    self->view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(self->store)));
    g_object_unref(self->store);

    gtk_tree_view_set_headers_visible(self->view, FALSE);
    gtk_container_add(GTK_CONTAINER(self->widget), GTK_WIDGET(self->view));

    pix = gtk_cell_renderer_pixbuf_new();
    txt = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(col, pix, FALSE);
    gtk_tree_view_column_pack_start(col, txt, TRUE);
    gtk_tree_view_column_add_attribute(col, pix, "pixbuf", COL_ICON);
    gtk_tree_view_column_add_attribute(col, txt, "text", COL_NAME);
    gtk_tree_view_column_set_expand(col, TRUE);
    gtk_tree_view_append_column(self->view, col);

    g_signal_connect(self->view, "row-expanded",
                     G_CALLBACK(on_row_expanded), self);
    g_signal_connect(gtk_tree_view_get_selection(self->view), "changed",
                     G_CALLBACK(on_selection_changed), self);

    /* 双根 */
    add_root(self, "/etc", "/etc");
    add_root(self, "~/.config", config);
    g_free(config);

    return self;
}

GtkWidget *
lr_tree_pane_get_widget(LrTreePane *self)
{
    return self->widget;
}

void lr_tree_pane_set_select_cb(LrTreePane *self, LrTreePaneSelectCb cb,
                                gpointer user_data)
{
    self->select_cb = cb;
    self->select_data = user_data;
}

void lr_tree_pane_refresh(LrTreePane *self)
{
    const gchar *home = g_get_home_dir();
    gchar *config = g_build_filename(home, ".config", NULL);

    gtk_tree_store_clear(self->store);
    add_root(self, "/etc", "/etc");
    add_root(self, "~/.config", config);
    g_free(config);
}

void lr_tree_pane_free(LrTreePane *self)
{
    if (self == NULL)
        return;
    g_clear_object(&self->icon_folder);
    g_clear_object(&self->icon_config);
    g_clear_object(&self->icon_other);
    g_free(self);
}
