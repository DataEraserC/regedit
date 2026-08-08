/* 扁平 KeyValue 解析器：无节的 key=value / key:value / key value */
#ifndef LR_CORE_PARSERS_KV_H
#define LR_CORE_PARSERS_KV_H

#include <glib.h>
#include "core/value.h"

/* 解析扁平键值对文件并填充 file->items，成功返回 TRUE */
gboolean lr_parse_kv(const char *path, LrConfigFile *file);

#endif /* LR_CORE_PARSERS_KV_H */
