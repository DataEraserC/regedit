#include "core/parsers/json.h"

#include <json-glib/json-glib.h>

gboolean
lr_parse_json(const char *path, LrConfigFile *file)
{
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    gchar *content = NULL;
    gboolean ok = FALSE;

    if (g_file_get_contents(path, &content, NULL, NULL) &&
        json_parser_load_from_data(parser, content, -1, &error))
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
    g_free(content);
    g_object_unref(parser);
    return ok;
}
