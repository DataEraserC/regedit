/* apt.conf 配置格式解析器（树形嵌套块 + :: 命名空间键） */
#ifndef LR_CORE_PARSERS_APT_H
#define LR_CORE_PARSERS_APT_H

#include "core/value.h"

/* 解析 apt 配置（Acquire::IndexTargets { ... }; 风格）。
 * 叶子项的 section 为完整 :: 路径（如 "Acquire::IndexTargets::deb::DEP-11"），
 * 供 UI 按路径逐级构建可展开的树形行。 */
gboolean lr_parse_apt(const char *path, LrConfigFile *file);

#endif /* LR_CORE_PARSERS_APT_H */
