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
        char *content;
        char *space;
        char *key, *value;
        gboolean commented = FALSE;

        if (*line == '\0')
            continue;

        if (line[0] == '#')
        {
            /* 猜测是否为被注释的配置：
             * 剥离 # 后匹配「关键字 + 单 token 参数」——值不含空白，
             * 从而与说明文字（长句）区分开，如 "#Port 22" 是配置而
             * "# This is the sshd server..." 是说明 */
            char *p = line;
            char *sp;

            while (*p == '#')
                p++;
            content = g_strstrip(p);
            if (*content == '\0')
                continue;
            if (!(g_ascii_isalpha(content[0]) || content[0] == '_'))
                continue;

            sp = strchr(content, ' ');
            if (sp == NULL)
                sp = strchr(content, '\t');
            if (sp == NULL)
                continue;
            /* 参数部分若含空白则视为说明文字，忽略 */
            if (strchr(sp + 1, ' ') != NULL || strchr(sp + 1, '\t') != NULL)
                continue;
            commented = TRUE;
        }
        else
        {
            content = line;
        }

        /* 第一个空白分隔：前为关键字，后为参数 */
        space = strchr(content, ' ');
        if (space == NULL)
            space = strchr(content, '\t');
        if (space == NULL)
            continue;

        *space = '\0';
        key = g_strstrip(content);
        value = g_strstrip(space + 1);
        /* 关键字不应含空白：若含说明分隔异常（如多值配置被误切），跳过 */
        if (*key == '\0' || *value == '\0' || strchr(key, ' ') != NULL ||
            strchr(key, '\t') != NULL)
            continue;

        {
            char *norm = collapse_whitespace(value);
            LrConfigItem *item = lr_config_item_new(
                key, norm, lr_value_detect_type(norm), NULL, NULL);
            item->enabled = !commented;
            g_ptr_array_add(file->items, item);
            g_free(norm);
        }
        file->parsed = TRUE;
    }

    g_strfreev(lines);
    return file->parsed;
}
