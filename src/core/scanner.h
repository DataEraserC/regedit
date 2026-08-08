/* 目录扫描：为左侧文件树提供条目列表 */
#ifndef LR_CORE_SCANNER_H
#define LR_CORE_SCANNER_H

#include <glib.h>
#include "core/format.h"

typedef enum {
    LR_SCAN_DIR = 0,         /* 目录 */
    LR_SCAN_SUPPORTED_FILE,  /* 受支持的配置文件 */
    LR_SCAN_OTHER_FILE,      /* 其他文件（以文本兜底展示） */
} LrScanKind;

typedef struct {
    char          *name;     /* 条目名 */
    char          *path;     /* 完整路径 */
    LrScanKind     kind;
    LrConfigFormat format;   /* 仅 kind == LR_SCAN_SUPPORTED_FILE 时有效 */
} LrScanEntry;

/* 扫描目录，返回 LrScanEntry* 数组（排序：目录优先，再按名称），调用方释放 */
GPtrArray *lr_scanner_list_dir(const char *path);

void lr_scan_entry_free(LrScanEntry *entry);

#endif /* LR_CORE_SCANNER_H */
