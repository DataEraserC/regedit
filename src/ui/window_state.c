#include "ui/window_state.h"

#include <glib/gstdio.h>

#define LR_WINDOW_STATE_SAVE_MS 500

struct _LrWindowState
{
    gchar *dir;         /* /run/user/<uid>/<app_id> */
    gchar *file;        /* state.ini 路径 */
    gint x, y;          /* 非最大化时的位置 */
    gint w, h;          /* 非最大化时的尺寸 */
    gboolean maximized; /* 是否最大化 */
    gchar *path;        /* 上次路径 */
    guint save_timeout; /* 节流保存定时器 */
};

static gboolean
on_save_timeout(gpointer user_data)
{
    LrWindowState *self = user_data;

    self->save_timeout = 0;
    lr_window_state_save_now(self);
    return G_SOURCE_REMOVE;
}

LrWindowState *
lr_window_state_new(const char *app_id)
{
    LrWindowState *self = g_new0(LrWindowState, 1);
    const char *runtime = g_get_user_runtime_dir();

    if (runtime != NULL)
    {
        self->dir = g_build_filename(runtime, app_id, NULL);
        self->file = g_build_filename(self->dir, "state.ini", NULL);
    }
    return self;
}

void lr_window_state_free(LrWindowState *self)
{
    if (self == NULL)
        return;
    if (self->save_timeout != 0)
        g_source_remove(self->save_timeout);
    g_free(self->dir);
    g_free(self->file);
    g_free(self->path);
    g_free(self);
}

gboolean
lr_window_state_restore(LrWindowState *self, char **last_path)
{
    GKeyFile *kf;
    GError *error = NULL;
    gboolean ok;

    *last_path = NULL;
    if (self->file == NULL)
        return FALSE;

    kf = g_key_file_new();
    ok = g_key_file_load_from_file(kf, self->file, G_KEY_FILE_NONE, &error);
    if (!ok)
    {
        g_clear_error(&error);
        g_key_file_free(kf);
        return FALSE;
    }

    self->w = g_key_file_get_integer(kf, "window", "width", NULL);
    self->h = g_key_file_get_integer(kf, "window", "height", NULL);
    self->x = g_key_file_get_integer(kf, "window", "x", NULL);
    self->y = g_key_file_get_integer(kf, "window", "y", NULL);
    self->maximized = g_key_file_get_boolean(kf, "window", "maximized", NULL);

    *last_path = g_key_file_get_string(kf, "main", "path", NULL);
    g_key_file_free(kf);
    return TRUE;
}

void lr_window_state_set_geometry(LrWindowState *self, gint x, gint y,
                                  gint w, gint h)
{
    self->x = x;
    self->y = y;
    /* 非最大化时记录尺寸：纯拖动调整大小也会更新，而不只靠
     * window-state-event（该信号仅在最大化状态切换时触发） */
    if (!self->maximized && w > 0 && h > 0)
    {
        self->w = w;
        self->h = h;
    }
}

void lr_window_state_set_maximized(LrWindowState *self, gboolean maximized)
{
    self->maximized = maximized;
}

void lr_window_state_set_size(LrWindowState *self, gint w, gint h)
{
    if (w > 0 && h > 0)
    {
        self->w = w;
        self->h = h;
    }
}

void lr_window_state_get_size(LrWindowState *self, gint *w, gint *h)
{
    *w = self->w;
    *h = self->h;
}

void lr_window_state_get_pos(LrWindowState *self, gint *x, gint *y)
{
    *x = self->x;
    *y = self->y;
}

gboolean
lr_window_state_is_maximized(LrWindowState *self)
{
    return self->maximized;
}

void lr_window_state_set_path(LrWindowState *self, const char *path)
{
    g_free(self->path);
    self->path = g_strdup(path);
}

void lr_window_state_schedule_save(LrWindowState *self)
{
    if (self->file == NULL)
        return;
    if (self->save_timeout != 0)
        g_source_remove(self->save_timeout);
    self->save_timeout = g_timeout_add(LR_WINDOW_STATE_SAVE_MS,
                                       on_save_timeout, self);
}

void lr_window_state_save_now(LrWindowState *self)
{
    GKeyFile *kf;
    gchar *data;

    if (self->file == NULL)
        return;

    kf = g_key_file_new();
    if (self->path != NULL)
        g_key_file_set_string(kf, "main", "path", self->path);

    g_key_file_set_integer(kf, "window", "x", self->x);
    g_key_file_set_integer(kf, "window", "y", self->y);
    g_key_file_set_integer(kf, "window", "width", self->w);
    g_key_file_set_integer(kf, "window", "height", self->h);
    g_key_file_set_boolean(kf, "window", "maximized", self->maximized);

    data = g_key_file_to_data(kf, NULL, NULL);
    if (data != NULL)
    {
        g_mkdir_with_parents(self->dir, 0755);
        g_file_set_contents(self->file, data, -1, NULL);
        g_free(data);
    }
    g_key_file_free(kf);
}
