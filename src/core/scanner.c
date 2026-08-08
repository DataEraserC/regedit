#include "core/scanner.h"

#include <string.h>

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
        LrScanEntry *entry;
        char *full = g_build_filename(path, name, NULL);

        if (g_str_equal(name, ".") || g_str_equal(name, ".."))
            goto next;

        entry = g_new0(LrScanEntry, 1);
        entry->name = g_strdup(name);
        entry->path = full;

        if (g_file_test(full, G_FILE_TEST_IS_DIR))
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
        continue;

    next:
        g_free(full);
    }

    g_dir_close(dir);
    g_ptr_array_sort(arr, compare_entries);
    return arr;
}
