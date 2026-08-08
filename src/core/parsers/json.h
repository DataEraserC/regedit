/* JSON 解析器：使用 json-glib 校验并解析 JSON 文件。
 * JSON 为嵌套结构，树形展示由 UI 层基于 JsonNode 构建；本层仅做合法性校验。 */
#ifndef LR_CORE_PARSERS_JSON_H
#define LR_CORE_PARSERS_JSON_H

#include <glib.h>
#include "core/value.h"

/* 校验并解析 JSON 文件，合法则返回 TRUE 并置 file->parsed */
gboolean lr_parse_json(const char *path, LrConfigFile *file);

#endif /* LR_CORE_PARSERS_JSON_H */
