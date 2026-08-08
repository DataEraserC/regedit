#include "core/parsers/kv.h"
#include "core/parsers/common.h"

gboolean
lr_parse_kv(const char *path, LrConfigFile *file)
{
    gchar *content = NULL;
    gsize length = 0;
    GError *error = NULL;
    gchar **lines = NULL;
    gchar **linep;
    char *pending_comment = NULL;

    if (!g_file_get_contents(path, &content, &length, &error)) {
        file->parsed = FALSE;
        file->error = g_strdup(error->message);
        g_clear_error(&error);
        return FALSE;
    }

    lines = g_strsplit(content, "\n", -1);
    g_free(content);

    for (linep = lines; linep != NULL && *linep != NULL; linep++) {
        char *line = g_strstrip(*linep);

        if (*line == '\0')
            continue;

        if (line[0] == '#' || line[0] == ';') {
            const char *p = line;
            char *stripped;

            while (*p == '#' || *p == ';')
                p++;
            stripped = g_strstrip(g_strdup(p));
            if (pending_comment == NULL) {
                pending_comment = g_strdup(stripped);
            } else {
                char *old = pending_comment;
                pending_comment = g_strconcat(old, "\n", stripped, NULL);
                g_free(old);
            }
            g_free(stripped);
            continue;
        }

        {
            char *key = NULL, *raw_value = NULL;
            /* 扁平格式允许 = 或 : 或空白分隔 */
            if (!lr_split_key_value(line, "=: \t", &key, &raw_value))
                continue;

            char *inline_comment = NULL;
            char *value = lr_strip_inline_comment(raw_value, &inline_comment);

            LrConfigItem *item = lr_config_item_new(
                key, value, lr_value_detect_type(value), NULL,
                pending_comment != NULL ? pending_comment : inline_comment);
            if (pending_comment != NULL && inline_comment != NULL) {
                char *merged = g_strconcat(pending_comment, "\n",
                                           inline_comment, NULL);
                g_free(item->comment);
                item->comment = merged;
            }

            g_ptr_array_add(file->items, item);
            file->parsed = TRUE;

            g_free(pending_comment);
            pending_comment = NULL;
            g_free(key);
            g_free(raw_value);
            g_free(value);
            g_free(inline_comment);
        }
    }

    g_strfreev(lines);
    g_free(pending_comment);
    return file->parsed;
}
