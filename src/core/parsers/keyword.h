/* 关键字-参数解析器：keyword argument（空白分隔），如 /etc/ssh/sshd_config */
#ifndef LR_CORE_PARSERS_KEYWORD_H
#define LR_CORE_PARSERS_KEYWORD_H

#include <glib.h>
#include "core/value.h"

/* 解析关键字-参数文件并填充 file->items，成功返回 TRUE */
gboolean lr_parse_keyword(const char *content, gsize len,
                          LrConfigFile *file);

#endif /* LR_CORE_PARSERS_KEYWORD_H */
