#include "core/parsers/xml.h"

#include <string.h>

/* 一个打开的 XML 元素 */
typedef struct
{
    gchar *name;
    GString *text;      /* 累积的文本 */
    gboolean has_child; /* 是否有子元素 */
} XmlFrame;

/* 解析上下文 */
typedef struct
{
    GQueue *stack;    /* 元素帧栈（XmlFrame*） */
    GString *path;    /* 当前 :: 路径 */
    GPtrArray *items; /* 输出 */
} XmlCtx;

static void
xml_frame_free(gpointer p)
{
    XmlFrame *f = p;
    if (f == NULL)
        return;
    g_free(f->name);
    if (f->text != NULL)
        g_string_free(f->text, TRUE);
    g_free(f);
}

/* 路径移除最后一段 */
static void
xml_path_pop(GString *path)
{
    gchar *p = g_strrstr(path->str, "::");
    if (p != NULL)
        g_string_truncate(path, (gsize)(p - path->str));
    else
        g_string_truncate(path, 0);
}

static void
xml_start_element(GMarkupParseContext *ctx, const gchar *elem,
                  const gchar **attr_names, const gchar **attr_values,
                  gpointer user_data, GError **error)
{
    XmlCtx *c = user_data;
    XmlFrame *f;
    guint i;

    (void)ctx;
    (void)error;

    f = g_new0(XmlFrame, 1);
    f->name = g_strdup(elem);
    f->text = g_string_new(NULL);
    g_queue_push_tail(c->stack, f);

    if (c->path->len > 0)
        g_string_append(c->path, "::");
    g_string_append(c->path, elem);

    /* 属性：作为该元素节点下的子项（@名 = 值） */
    for (i = 0; attr_names[i] != NULL; i++)
    {
        gchar *key = g_strdup_printf("@%s", attr_names[i]);
        LrConfigItem *item = lr_config_item_new(
            key, attr_values[i], lr_value_detect_type(attr_values[i]),
            c->path->str, NULL);
        g_ptr_array_add(c->items, item);
        g_free(key);
    }
}

static void
xml_text(GMarkupParseContext *ctx, const gchar *text, gsize text_len,
         gpointer user_data, GError **error)
{
    XmlCtx *c = user_data;
    XmlFrame *f;

    (void)ctx;
    (void)error;

    f = g_queue_peek_tail(c->stack);
    if (f != NULL)
        g_string_append_len(f->text, text, text_len);
}

static void
xml_end_element(GMarkupParseContext *ctx, const gchar *elem,
                gpointer user_data, GError **error)
{
    XmlCtx *c = user_data;
    XmlFrame *f;
    XmlFrame *parent;
    gchar *text;

    (void)ctx;
    (void)error;
    (void)elem;

    f = g_queue_pop_tail(c->stack);
    if (f == NULL)
        return;

    /* 标记父元素有子元素 */
    parent = g_queue_peek_tail(c->stack);
    if (parent != NULL)
        parent->has_child = TRUE;

    /* 路径回退到父路径（本元素名将成为叶子配置项的 key） */
    xml_path_pop(c->path);

    /* 叶子元素：含非空文本且无子元素 → 配置项行 */
    text = g_strstrip(g_strdup(f->text->str));
    if (text[0] != '\0' && !f->has_child)
    {
        LrConfigItem *item = lr_config_item_new(
            f->name, text, lr_value_detect_type(text),
            c->path->len > 0 ? c->path->str : NULL, NULL);
        g_ptr_array_add(c->items, item);
    }
    g_free(text);

    xml_frame_free(f);
}

gboolean
lr_parse_xml(const char *content, gsize len, LrConfigFile *file)
{
    XmlCtx ctx;
    GMarkupParseContext *pctx;
    GMarkupParser callbacks;
    GError *error = NULL;
    gboolean ok = FALSE;

    if (content == NULL)
    {
        file->parsed = FALSE;
        return FALSE;
    }

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.start_element = xml_start_element;
    callbacks.text = xml_text;
    callbacks.end_element = xml_end_element;

    ctx.stack = g_queue_new();
    ctx.path = g_string_new(NULL);
    ctx.items = file->items;

    pctx = g_markup_parse_context_new(&callbacks, G_MARKUP_TREAT_CDATA_AS_TEXT,
                                      &ctx, NULL);
    if (g_markup_parse_context_parse(pctx, content, (gssize)len, &error) &&
        g_markup_parse_context_end_parse(pctx, &error))
        ok = TRUE;

    g_markup_parse_context_free(pctx);
    if (error != NULL)
    {
        file->parsed = FALSE;
        file->error = g_strdup(error->message);
        g_error_free(error);
    }

    g_queue_free_full(ctx.stack, xml_frame_free);
    g_string_free(ctx.path, TRUE);

    file->parsed = ok && file->items->len > 0;
    return file->parsed;
}
