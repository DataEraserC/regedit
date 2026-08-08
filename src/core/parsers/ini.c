#include "core/parsers/ini.h"
#include "core/parsers/common.h"

gboolean
lr_parse_ini(const char *path, LrConfigFile *file)
{
    return lr_parse_section_kv(path, file);
}
