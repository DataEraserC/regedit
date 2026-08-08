/* INI 解析器：[Section] + key=value；注释 ; 或 # */
#ifndef LR_CORE_PARSERS_INI_H
#define LR_CORE_PARSERS_INI_H

#include <glib.h>
#include "core/value.h"

/* 解析 INI 文件并填充 file->items，成功返回 TRUE */
gboolean lr_parse_ini(const char *content, gsize len, LrConfigFile *file);

#endif /* LR_CORE_PARSERS_INI_H */
