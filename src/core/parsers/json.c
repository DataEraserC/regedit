#include "core/parsers/json.h"

#include <json-glib/json-glib.h>

gboolean
lr_parse_json(const char *content, gsize length, LrConfigFile *file)
{
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    gboolean ok = FALSE;

    if (content != NULL &&
        json_parser_load_from_data(parser, content, (gssize)length, &error))
    {
        ok = TRUE;
        file->parsed = TRUE;
    }
    else
    {
        file->parsed = FALSE;
        if (error != NULL)
            file->error = g_strdup(error->message);
    }

    if (error != NULL)
        g_error_free(error);
    g_object_unref(parser);
    return ok;
}
