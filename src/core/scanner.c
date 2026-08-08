#include "core/scanner.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

/* 目录树中视为过大的文件大小阈值（超过则不显示） */
#define LR_MAX_FILE_SIZE (128 * 1024)
/* 判断文本文件时探测的字节数 */
#define LR_TEXT_PROBE_SIZE 1024

void lr_scan_entry_free(LrScanEntry *entry)
{
    if (entry == NULL)
        return;
    g_free(entry->name);
    g_free(entry->path);
    g_free(entry);
}

static gint
compare_entries(gconstpointer a, gconstpointer b)
{
    const LrScanEntry *ea = *((LrScanEntry **)a);
    const LrScanEntry *eb = *((LrScanEntry **)b);
    gint ad = (ea->kind == LR_SCAN_DIR);
    gint bd = (eb->kind == LR_SCAN_DIR);

    if (ad != bd)
        return bd - ad; /* 目录优先 */
    return g_ascii_strcasecmp(ea->name, eb->name);
}

/* 文件大小超过阈值则跳过 */
static gboolean
is_oversized(const char *path)
{
    GStatBuf st;

    if (g_stat(path, &st) != 0)
        return FALSE;
    return st.st_size > LR_MAX_FILE_SIZE;
}

/* 判断是否为文本文件：读取前 LR_TEXT_PROBE_SIZE 字节，含 NUL 视为非文本 */
static gboolean
is_text_file(const char *path)
{
    GFile *file;
    GFileInputStream *stream;
    guchar buf[LR_TEXT_PROBE_SIZE];
    gsize n = 0;
    gboolean text = TRUE;
    GError *error = NULL;

    file = g_file_new_for_path(path);
    stream = g_file_read(file, NULL, &error);
    if (stream == NULL)
    {
        g_clear_error(&error);
        g_object_unref(file);
        return TRUE; /* 无法读取时仍显示 */
    }

    {
        gssize r = g_input_stream_read(G_INPUT_STREAM(stream), buf,
                                       sizeof(buf), NULL, NULL);
        if (r >= 0)
        {
            gsize i;
            n = (gsize)r;
            for (i = 0; i < n; i++)
            {
                if (buf[i] == '\0')
                {
                    text = FALSE;
                    break;
                }
            }
        }
    }

    g_input_stream_close(G_INPUT_STREAM(stream), NULL, NULL);
    g_object_unref(stream);
    g_object_unref(file);
    return text;
}

/* 目录为空（无任何条目）则跳过 */
static gboolean
is_empty_dir(const char *path)
{
    GDir *dir = g_dir_open(path, 0, NULL);
    gboolean empty;

    if (dir == NULL)
        return TRUE;
    empty = (g_dir_read_name(dir) == NULL);
    g_dir_close(dir);
    return empty;
}

GPtrArray *
lr_scanner_list_dir(const char *path)
{
    GPtrArray *arr = g_ptr_array_new_with_free_func(
        (GDestroyNotify)lr_scan_entry_free);
    GDir *dir;
    const char *name;

    dir = g_dir_open(path, 0, NULL);
    if (dir == NULL)
        return arr;

    while ((name = g_dir_read_name(dir)) != NULL)
    {
        char *full = g_build_filename(path, name, NULL);
        LrScanEntry *entry;
        gboolean is_dir;

        if (g_str_equal(name, ".") || g_str_equal(name, ".."))
        {
            g_free(full);
            continue;
        }

        is_dir = g_file_test(full, G_FILE_TEST_IS_DIR);

        /* 过滤：空文件夹、超大文件、非文本文件均不显示 */
        if (is_dir)
        {
            if (is_empty_dir(full))
            {
                g_free(full);
                continue;
            }
        }
        else
        {
            if (is_oversized(full) || !is_text_file(full))
            {
                g_free(full);
                continue;
            }
        }

        entry = g_new0(LrScanEntry, 1);
        entry->name = g_strdup(name);
        entry->path = full;

        if (is_dir)
        {
            entry->kind = LR_SCAN_DIR;
        }
        else
        {
            LrConfigFormat fmt = lr_format_detect(full);
            if (lr_format_supported(fmt))
            {
                entry->kind = LR_SCAN_SUPPORTED_FILE;
                entry->format = fmt;
            }
            else
            {
                entry->kind = LR_SCAN_OTHER_FILE;
            }
        }
        g_ptr_array_add(arr, entry);
    }

    g_dir_close(dir);
    g_ptr_array_sort(arr, compare_entries);
    return arr;
}
