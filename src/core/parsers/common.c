#include "core/parsers/common.h"

#include <string.h>

char *
lr_strip_inline_comment(const char *value, char **comment_out)
{
    const char *p;
    gboolean quoted = FALSE;
    char *result;
    char *comment = NULL;

    *comment_out = NULL;

    if (value == NULL) {
        return g_strdup("");
    }

    /* 整体被双引号包裹 */
    if (value[0] == '"') {
        const char *close = strchr(value + 1, '"');
        if (close != NULL) {
            /* 引号内的值 */
            result = g_strndup(value + 1, (gsize)(close - value - 1));
            /* 闭合引号之后的行内注释 */
            p = close + 1;
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '#' || *p == ';') {
                comment = g_strstrip(g_strdup(p + 1));
            }
            *comment_out = comment;
            return result;
        }
        /* 引号未闭合，按普通文本处理 */
    }

    /* 普通文本：扫描未加引号的 ; 或 # 作为注释起点 */
    p = value;
    quoted = FALSE;
    while (*p != '\0') {
        if (*p == '"')
            quoted = !quoted;
        if (!quoted && (*p == ';' || *p == '#')) {
            comment = g_strstrip(g_strdup(p + 1));
            break;
        }
        p++;
    }

    result = (comment != NULL)
                 ? g_strndup(value, (gsize)(p - value))
                 : g_strdup(value);
    result = g_strstrip(result);
    *comment_out = comment;
    return result;
}

gboolean
lr_split_key_value(const char *line, const char *delims,
                   char **key_out, char **value_out)
{
    const char *p;
    gboolean in_quotes = FALSE;

    *key_out = NULL;
    *value_out = NULL;

    if (line == NULL)
        return FALSE;

    p = line;
    while (*p != '\0') {
        if (*p == '"')
            in_quotes = !in_quotes;
        if (!in_quotes && strchr(delims, *p) != NULL)
            break;
        p++;
    }

    if (*p == '\0')
        return FALSE; /* 未找到分隔符 */

    *key_out = g_strstrip(g_strndup(line, (gsize)(p - line)));
    *value_out = g_strstrip(g_strdup(p + 1));

    if (**key_out == '\0') {
        g_free(*key_out);
        g_free(*value_out);
        *key_out = NULL;
        *value_out = NULL;
        return FALSE;
    }

    return TRUE;
}

/* 将注释行累积进 pending（剥离行首注释符 # / ;） */
static void
append_pending_comment(char **pending, const char *line)
{
    const char *p = line;
    char *stripped;

    while (*p == '#' || *p == ';')
        p++;
    stripped = g_strstrip(g_strdup(p));

    if (*pending == NULL) {
        *pending = g_strdup(stripped);
    } else {
        char *old = *pending;
        *pending = g_strconcat(old, "\n", stripped, NULL);
        g_free(old);
    }
    g_free(stripped);
}

gboolean
lr_parse_section_kv(const char *path, LrConfigFile *file)
{
    gchar *content = NULL;
    gsize length = 0;
    GError *error = NULL;
    gchar **lines = NULL;
    gchar **linep;
    char *section = NULL;
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

        /* 注释行 */
        if (line[0] == '#' || line[0] == ';') {
            append_pending_comment(&pending_comment, line);
            continue;
        }

        /* 节 */
        if (line[0] == '[') {
            char *close = strchr(line, ']');
            if (close != NULL) {
                g_free(section);
                section = g_strndup(line + 1, (gsize)(close - line - 1));
            }
            continue;
        }

        /* 键值对 */
        {
            char *key = NULL, *raw_value = NULL;
            if (!lr_split_key_value(line, "=:", &key, &raw_value))
                continue;

            char *inline_comment = NULL;
            char *value = lr_strip_inline_comment(raw_value, &inline_comment);

            LrConfigItem *item = lr_config_item_new(
                key, value, lr_value_detect_type(value), section,
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
    g_free(section);
    g_free(pending_comment);
    return file->parsed;
}
