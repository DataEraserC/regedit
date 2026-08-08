/* systemd unit 解析器：[Unit]/[Service] 等节 + Key=Value；注释 # 或 ; */
#ifndef LR_CORE_PARSERS_SYSTEMD_H
#define LR_CORE_PARSERS_SYSTEMD_H

#include <glib.h>
#include "core/value.h"

/* 解析 systemd unit 文件并填充 file->items，成功返回 TRUE */
gboolean lr_parse_systemd(const char *path, LrConfigFile *file);

#endif /* LR_CORE_PARSERS_SYSTEMD_H */
