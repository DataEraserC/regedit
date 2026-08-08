/* 配置文件格式识别与解析分发 */
#ifndef LR_CORE_FORMAT_H
#define LR_CORE_FORMAT_H

#include <glib.h>
#include "core/value.h"

typedef enum
{
    LR_FORMAT_UNKNOWN = 0, /* 不支持：以文本编辑器兜底展示 */
    LR_FORMAT_INI,         /* INI：分节键值对 */
    LR_FORMAT_KV,          /* 扁平 key=value */
    LR_FORMAT_SYSTEMD,     /* systemd unit 文件 */
    LR_FORMAT_KEYWORD,     /* 关键字-参数（空白分隔），如 sshd_config */
} LrConfigFormat;

/* 根据文件名与文件内容启发式判断格式 */
LrConfigFormat lr_format_detect(const char *path);

/* 格式的中文显示名 */
const char *lr_format_name(LrConfigFormat fmt);

/* 是否为受支持的格式 */
gboolean lr_format_supported(LrConfigFormat fmt);

/* 解析入口：检测格式并调用对应解析器，返回解析后的文件（调用方释放） */
LrConfigFile *lr_parse_config(const char *path);

#endif /* LR_CORE_FORMAT_H */
