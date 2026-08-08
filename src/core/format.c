#include "core/format.h"

#include <string.h>
#include "core/parsers/ini.h"
#include "core/parsers/kv.h"
#include "core/parsers/systemd.h"

static gboolean
has_systemd_extension(const char *path)
{
    static const char *exts[] = {
        ".service", ".socket", ".timer", ".mount", ".automount",
        ".swap", ".path", ".slice", ".scope", ".target", ".device",
        NULL};
    gint i;
    for (i = 0; exts[i] != NULL; i++)
    {
        if (g_str_has_suffix(path, exts[i]))
            return TRUE;
    }
    return FALSE;
}

/* 读取文件头部（最多 max 字节）用于格式嗅探，失败返回 NULL */
static char *
read_head(const char *path, gsize max)
{
    gchar *content = NULL;
    gsize length = 0;
    GError *error = NULL;

    if (!g_file_get_contents(path, &content, &length, &error))
    {
        g_clear_error(&error);
        return NULL;
    }
    if (length > max)
    {
        char *head = g_strndup(content, max);
        g_free(content);
        return head;
    }
    return content;
}

LrConfigFormat
lr_format_detect(const char *path)
{
    char *head;
    gchar **lines, **linep;
    gboolean has_section = FALSE;
    gboolean has_kv = FALSE;

    /* 1) systemd unit 扩展名最明确 */
    if (has_systemd_extension(path))
        return LR_FORMAT_SYSTEMD;

    /* 2) 依据内容嗅探 */
    head = read_head(path, 65536);
    if (head == NULL)
        return LR_FORMAT_UNKNOWN;

    lines = g_strsplit(head, "\n", -1);
    g_free(head);

    for (linep = lines; linep != NULL && *linep != NULL; linep++)
    {
        char *line = g_strstrip(*linep);

        if (*line == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if (line[0] == '[' && strchr(line, ']') != NULL)
        {
            has_section = TRUE;
            continue;
        }
        if (strchr(line, '=') != NULL || strchr(line, ':') != NULL)
            has_kv = TRUE;
    }

    g_strfreev(lines);

    if (has_section)
        return LR_FORMAT_INI;
    if (has_kv)
        return LR_FORMAT_KV;
    return LR_FORMAT_UNKNOWN;
}

const char *
lr_format_name(LrConfigFormat fmt)
{
    switch (fmt)
    {
    case LR_FORMAT_INI:
        return "INI";
    case LR_FORMAT_KV:
        return "键值对";
    case LR_FORMAT_SYSTEMD:
        return "systemd unit";
    case LR_FORMAT_UNKNOWN:
    default:
        return "未知格式";
    }
}

gboolean
lr_format_supported(LrConfigFormat fmt)
{
    return fmt == LR_FORMAT_INI || fmt == LR_FORMAT_KV ||
           fmt == LR_FORMAT_SYSTEMD;
}

LrConfigFile *
lr_parse_config(const char *path)
{
    LrConfigFile *file = lr_config_file_new(path);
    LrConfigFormat fmt = lr_format_detect(path);
    gboolean ok = FALSE;

    switch (fmt)
    {
    case LR_FORMAT_INI:
        ok = lr_parse_ini(path, file);
        break;
    case LR_FORMAT_KV:
        ok = lr_parse_kv(path, file);
        break;
    case LR_FORMAT_SYSTEMD:
        ok = lr_parse_systemd(path, file);
        break;
    default:
        file->parsed = FALSE;
        break;
    }

    if (ok)
        file->parsed = TRUE;
    return file;
}
