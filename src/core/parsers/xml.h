/* XML 配置解析器（GMarkup）
 * 将嵌套元素树映射为 :: 路径：
 *   - 容器元素（有子元素）→ 路径段，UI 显示为可展开节点
 *   - 叶子元素（含文本）→ 配置项（key=元素名，data=文本）
 *   - 属性 → @属性名 子项 */
#ifndef LR_CORE_PARSERS_XML_H
#define LR_CORE_PARSERS_XML_H

#include "core/value.h"

gboolean lr_parse_xml(const char *content, gsize len, LrConfigFile *file);

#endif /* LR_CORE_PARSERS_XML_H */
