#include "core/format.h"

#include <string.h>
#include "core/parsers/ini.h"
#include "core/parsers/keyword.h"
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

/* 判断一行是否为「关键字 + 参数」样式（sshd_config 等）：
 * 首词以字母/下划线开头，后跟空白，且空白后有非空参数 */
static gboolean
is_keyword_line(const char *line)
{
    const char *p = line;

    if (!(g_ascii_isalpha(*p) || *p == '_'))
        return FALSE;
    while (g_ascii_isalnum(*p) || *p == '_' || *p == '-')
        p++;
    if (*p != ' ' && *p != '\t')
        return FALSE;
    while (*p == ' ' || *p == '\t')
        p++;
    return *p != '\0';
}

LrConfigFormat
lr_format_detect(const char *path)
{
    char *head;
    gchar **lines, **linep;
    gboolean has_section = FALSE;
    gboolean has_kv = FALSE;
    gboolean has_non_comment = FALSE;
    gboolean keyword_style = TRUE;
    guint keyword_lines = 0;         /* 匹配「关键字-参数」的行数 */
    guint multiword_value_lines = 0; /* 其中参数为多词的行数 */

    /* 1) systemd unit 扩展名最明确 */
    if (has_systemd_extension(path))
        return LR_FORMAT_SYSTEMD;

    /* 2) 依据内容嗅探（Linux 配置无法仅凭后缀判断） */
    head = read_head(path, 65536);
    if (head == NULL)
        return LR_FORMAT_UNKNOWN;

    /* 脚本解释器（shebang #!）：一律以文本形式打开，不做配置解析 */
    if (g_str_has_prefix(head, "#!"))
    {
        g_free(head);
        return LR_FORMAT_UNKNOWN;
    }

    lines = g_strsplit(head, "\n", -1);
    g_free(head);

    for (linep = lines; linep != NULL && *linep != NULL; linep++)
    {
        char *line = g_strstrip(*linep);

        if (*line == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        has_non_comment = TRUE;

        if (line[0] == '[' && strchr(line, ']') != NULL)
        {
            has_section = TRUE;
            continue;
        }
        if (strchr(line, '=') != NULL || strchr(line, ':') != NULL)
            has_kv = TRUE;
        if (is_keyword_line(line))
        {
            /* 参数若含空白（多词），更像自然语言而非配置 */
            char *sp = strchr(line, ' ');
            if (sp == NULL)
                sp = strchr(line, '\t');
            if (sp != NULL && (strchr(sp + 1, ' ') != NULL ||
                               strchr(sp + 1, '\t') != NULL))
                multiword_value_lines++;
            keyword_lines++;
        }
        else
        {
            keyword_style = FALSE;
        }
    }

    g_strfreev(lines);

    if (has_section)
        return LR_FORMAT_INI;
    if (has_kv)
        return LR_FORMAT_KV;
    if (has_non_comment && keyword_style)
    {
        /* 若大多数「关键字-参数」行的参数为多词（自然语言特征，如 /etc/legal
         * 的英文说明），判定为普通文本而非配置 */
        if (keyword_lines > 0 &&
            multiword_value_lines * 100 / keyword_lines > 60)
            return LR_FORMAT_UNKNOWN;
        return LR_FORMAT_KEYWORD;
    }
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
    case LR_FORMAT_KEYWORD:
        return "关键字-参数";
    case LR_FORMAT_UNKNOWN:
    default:
        return "未知格式";
    }
}

gboolean
lr_format_supported(LrConfigFormat fmt)
{
    return fmt == LR_FORMAT_INI || fmt == LR_FORMAT_KV ||
           fmt == LR_FORMAT_SYSTEMD || fmt == LR_FORMAT_KEYWORD;
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
    case LR_FORMAT_KEYWORD:
        ok = lr_parse_keyword(path, file);
        break;
    default:
        file->parsed = FALSE;
        break;
    }

    if (ok)
        file->parsed = TRUE;
    return file;
}
