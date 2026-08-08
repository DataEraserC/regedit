#include "core/parsers/keyword.h"

#include <string.h>

/* 将参数中的连续空白（空格/Tab）压缩为单个空格 */
static char *
collapse_whitespace(const char *s)
{
    GString *out = g_string_new(NULL);
    gboolean last_ws = FALSE;
    const char *p;

    for (p = s; *p != '\0'; p++)
    {
        if (*p == ' ' || *p == '\t')
        {
            if (!last_ws && out->len > 0)
                g_string_append_c(out, ' ');
            last_ws = TRUE;
        }
        else
        {
            g_string_append_c(out, *p);
            last_ws = FALSE;
        }
    }
    return g_string_free(out, FALSE);
}

gboolean
lr_parse_keyword(const char *path, LrConfigFile *file)
{
    gchar *content = NULL;
    gsize length = 0;
    GError *error = NULL;
    gchar **lines = NULL;
    gchar **linep;

    if (!g_file_get_contents(path, &content, &length, &error))
    {
        file->parsed = FALSE;
        file->error = g_strdup(error->message);
        g_clear_error(&error);
        return FALSE;
    }

    lines = g_strsplit(content, "\n", -1);
    g_free(content);

    for (linep = lines; linep != NULL && *linep != NULL; linep++)
    {
        char *line = g_strstrip(*linep);
        char *space;
        char *key, *value;

        if (*line == '\0' || line[0] == '#')
            continue;

        /* 第一个空白分隔：前为关键字，后为参数 */
        space = strchr(line, ' ');
        if (space == NULL)
            space = strchr(line, '\t');
        if (space == NULL)
            continue;

        *space = '\0';
        key = g_strstrip(line);
        value = g_strstrip(space + 1);
        if (*key == '\0' || *value == '\0')
            continue;

        {
            char *norm = collapse_whitespace(value);
            g_ptr_array_add(file->items,
                            lr_config_item_new(key, norm,
                                               lr_value_detect_type(norm),
                                               NULL, NULL));
            g_free(norm);
        }
        file->parsed = TRUE;
    }

    g_strfreev(lines);
    return file->parsed;
}
