#include "core/parsers/systemd.h"
#include "core/parsers/common.h"

gboolean
lr_parse_systemd(const char *path, LrConfigFile *file)
{
    return lr_parse_section_kv(path, file);
}
