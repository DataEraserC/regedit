/*
 * 解析器内部共享工具。
 *
 * INI 与 systemd unit 在语法上高度相似（[Section] + key=value + 注释），
 * 因此共享一套"节 + 键值对"逐行解析实现；扁平 KeyValue 复用注释/分隔工具。
 */
#ifndef LR_CORE_PARSERS_COMMON_H
#define LR_CORE_PARSERS_COMMON_H

#include <glib.h>
#include "core/value.h"

/*
 * 从值文本中剥离行内注释（; 或 #）。
 *   - 值整体被双引号包裹时，去掉引号且引号内不剥离注释；
 *   - 返回剥离后的值（新分配）；
 *   - *comment_out 输出行内注释内容（新分配），无则置 NULL。
 */
char *lr_strip_inline_comment(const char *value, char **comment_out);

/*
 * 将 "key=value" 按给定分隔符集合（如 "=:", "=", "=: \t"）拆分为键与值。
 * 均去除两侧空白并新分配。成功返回 TRUE。
 */
gboolean lr_split_key_value(const char *line, const char *delims,
                            char **key_out, char **value_out);

/*
 * 逐行解析"节 + key=value"格式（INI / systemd 通用）。
 * 支持 # 与 ; 注释、[Section]、行内注释、去引号。
 * 成功返回 TRUE 并填充 file->items。
 */
gboolean lr_parse_section_kv(const char *path, LrConfigFile *file);

#endif /* LR_CORE_PARSERS_COMMON_H */
