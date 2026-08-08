#include "core/parsers/ini.h"
#include "core/parsers/common.h"

gboolean
lr_parse_ini(const char *content, gsize len, LrConfigFile *file)
{
    return lr_parse_section_kv(content, len, file);
}
