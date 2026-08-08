#include "core/format.h"

#include <string.h>
#include <json-glib/json-glib.h>
#include "core/parsers/ini.h"
#include "core/parsers/json.h"
#include "core/parsers/keyword.h"
#include "core/parsers/kv.h"
#include "core/parsers/systemd.h"
#include "core/parsers/apt.h"

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

/* 嗅探最多读取的字节数：远超配置文件典型大小 */
#define LR_SNIFF_MAX 65536
/* 「关键字-参数」行中参数为多词（自然语言特征）的比例阈值，超过则视为普通文本 */
#define LR_NATURAL_LANG_THRESHOLD 60

/* 取内容前 max 字节用于嗅探（避免扫描超大非文本文件），返回新分配字符串 */
static char *
sniff_head(const char *content, gsize len, gsize max)
{
    return g_strndup(content, MIN(len, max));
}

/* 判断文件内容是否为合法 JSON 数组（[ 开头可能是 INI 节，需整体验证） */
static gboolean
is_json_array_content(const char *content, gsize len)
{
    JsonParser *parser;
    JsonNode *root;
    gboolean ok = FALSE;

    parser = json_parser_new();
    if (json_parser_load_from_data(parser, content, (gssize)len, NULL))
    {
        root = json_parser_get_root(parser);
        ok = root != NULL && JSON_NODE_TYPE(root) == JSON_NODE_ARRAY;
    }
    g_object_unref(parser);
    return ok;
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

/* apt 配置特征：含 { } 嵌套块 + ; 结尾的赋值（Acquire::IndexTargets { ... };） */
static gboolean
is_apt_config(const char *content, gsize len)
{
    gchar **lines, **lp;
    gboolean has_block = FALSE;
    gboolean has_semi = FALSE;
    gchar *head = sniff_head(content, len, LR_SNIFF_MAX);

    lines = g_strsplit(head, "\n", -1);
    g_free(head);
    for (lp = lines; lp != NULL && *lp != NULL; lp++)
    {
        gchar *l = g_strstrip(*lp);
        gsize llen;

        if (*l == '\0' || l[0] == '#' || l[0] == '/')
            continue;
        llen = strlen(l);
        if (llen > 0 && l[llen - 1] == '{')
            has_block = TRUE;
        if (llen > 0 && l[llen - 1] == ';')
            has_semi = TRUE;
        if (strstr(l, "::") != NULL && strchr(l, '{') != NULL)
            has_block = TRUE;
    }
    g_strfreev(lines);
    return has_block && has_semi;
}

/* 行特征统计：一次扫描收集 INI / KV / 关键字判定所需的所有特征 */
typedef struct
{
    gboolean has_section;
    gboolean has_kv;
    gboolean has_non_comment;
    gboolean keyword_style;
    guint keyword_lines;
    guint multiword_value_lines;
} LineStats;

static LineStats
scan_stats(const char *content, gsize len)
{
    gchar **lines, **lp;
    gchar *head = sniff_head(content, len, LR_SNIFF_MAX);
    LineStats st;

    memset(&st, 0, sizeof(st));
    st.keyword_style = TRUE;

    lines = g_strsplit(head, "\n", -1);
    g_free(head);

    for (lp = lines; lp != NULL && *lp != NULL; lp++)
    {
        char *line = g_strstrip(*lp);

        if (*line == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        st.has_non_comment = TRUE;

        if (line[0] == '[' && strchr(line, ']') != NULL)
        {
            st.has_section = TRUE;
            continue;
        }
        if (strchr(line, '=') != NULL || strchr(line, ':') != NULL)
            st.has_kv = TRUE;
        if (is_keyword_line(line))
        {
            char *sp = strchr(line, ' ');
            if (sp == NULL)
                sp = strchr(line, '\t');
            if (sp != NULL && (strchr(sp + 1, ' ') != NULL ||
                               strchr(sp + 1, '\t') != NULL))
                st.multiword_value_lines++;
            st.keyword_lines++;
        }
        else
        {
            st.keyword_style = FALSE;
        }
    }

    g_strfreev(lines);
    return st;
}

/* ---- 各格式嗅探器（数组顺序即优先级） ---- */

static gboolean
sniff_systemd(const char *content, gsize len, const char *path)
{
    (void)content;
    (void)len;
    return has_systemd_extension(path);
}

static gboolean
sniff_json(const char *content, gsize len, const char *path)
{
    const char *p = content;
    (void)path;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p == '{')
        return TRUE;
    if (*p == '[')
        return is_json_array_content(content, len);
    return FALSE;
}

static gboolean
sniff_apt(const char *content, gsize len, const char *path)
{
    (void)path;
    return is_apt_config(content, len);
}

static gboolean
sniff_ini(const char *content, gsize len, const char *path)
{
    (void)path;
    return scan_stats(content, len).has_section;
}

static gboolean
sniff_kv(const char *content, gsize len, const char *path)
{
    (void)path;
    return scan_stats(content, len).has_kv;
}

static gboolean
sniff_keyword(const char *content, gsize len, const char *path)
{
    LineStats st = scan_stats(content, len);
    (void)path;

    if (!st.has_non_comment || !st.keyword_style)
        return FALSE;
    /* 大多数「关键字-参数」行的参数为多词（自然语言特征，如 /etc/legal 的
     * 英文说明），判定为普通文本而非配置 */
    if (st.keyword_lines > 0 &&
        st.multiword_value_lines * 100 / st.keyword_lines >
            LR_NATURAL_LANG_THRESHOLD)
        return FALSE;
    return TRUE;
}

/* ---- 格式注册表：新增格式只需在此追加一个条目 ---- */
typedef struct
{
    const char *name; /* 显示名 */
    LrConfigFormat id;
    gboolean (*sniff)(const char *content, gsize len, const char *path);
    gboolean (*parse)(const char *content, gsize len, LrConfigFile *file);
} LrFormatDriver;

static const LrFormatDriver k_drivers[] = {
    {"systemd unit", LR_FORMAT_SYSTEMD, sniff_systemd, lr_parse_systemd},
    {"JSON", LR_FORMAT_JSON, sniff_json, lr_parse_json},
    {"apt 配置", LR_FORMAT_APT, sniff_apt, lr_parse_apt},
    {"INI", LR_FORMAT_INI, sniff_ini, lr_parse_ini},
    {"键值对", LR_FORMAT_KV, sniff_kv, lr_parse_kv},
    {"关键字-参数", LR_FORMAT_KEYWORD, sniff_keyword, lr_parse_keyword},
};

LrConfigFormat
lr_format_detect_content(const char *path, const char *content, gsize len)
{
    guint i;

    if (content == NULL)
        return LR_FORMAT_UNKNOWN;

    /* 脚本解释器（shebang #!）：一律以文本形式打开，不做配置解析 */
    if (g_str_has_prefix(content, "#!"))
        return LR_FORMAT_UNKNOWN;

    for (i = 0; i < G_N_ELEMENTS(k_drivers); i++)
    {
        if (k_drivers[i].sniff(content, len, path))
            return k_drivers[i].id;
    }
    return LR_FORMAT_UNKNOWN;
}

/* 兼容入口：按路径读取文件内容后检测（供测试与外部调用） */
LrConfigFormat
lr_format_detect(const char *path)
{
    gchar *content = NULL;
    gsize len = 0;
    LrConfigFormat fmt;

    if (!g_file_get_contents(path, &content, &len, NULL))
        return LR_FORMAT_UNKNOWN;
    fmt = lr_format_detect_content(path, content, len);
    g_free(content);
    return fmt;
}

const char *
lr_format_name(LrConfigFormat fmt)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(k_drivers); i++)
        if (k_drivers[i].id == fmt)
            return k_drivers[i].name;
    return "未知格式";
}

gboolean
lr_format_supported(LrConfigFormat fmt)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(k_drivers); i++)
        if (k_drivers[i].id == fmt)
            return TRUE;
    return FALSE;
}

LrConfigFile *
lr_parse_config_content(const char *path, const char *content, gsize len)
{
    LrConfigFile *file = lr_config_file_new(path);
    LrConfigFormat fmt = lr_format_detect_content(path, content, len);
    guint i;
    gboolean ok = FALSE;

    for (i = 0; i < G_N_ELEMENTS(k_drivers); i++)
    {
        if (k_drivers[i].id == fmt)
        {
            ok = k_drivers[i].parse(content, len, file);
            break;
        }
    }
    if (ok)
        file->parsed = TRUE;
    return file;
}

/* 兼容入口：按路径读取文件内容后解析（供测试与外部调用） */
LrConfigFile *
lr_parse_config(const char *path)
{
    gchar *content = NULL;
    gsize len = 0;

    if (!g_file_get_contents(path, &content, &len, NULL))
    {
        LrConfigFile *file = lr_config_file_new(path);
        file->parsed = FALSE;
        return file;
    }

    {
        LrConfigFile *file = lr_parse_config_content(path, content, len);
        g_free(content);
        return file;
    }
}
