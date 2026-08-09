/* 全局共享的常量 */
#ifndef LR_CORE_LIMITS_H
#define LR_CORE_LIMITS_H

/* 目录树中视为过大的文件大小阈值（超过则不显示，导出时也跳过） */
#define LR_MAX_FILE_SIZE (128 * 1024)

/* 格式嗅探/目录扫描一次读取的头部字节数（远超配置文件典型大小） */
#define LR_SNIFF_MAX (64 * 1024)

#endif /* LR_CORE_LIMITS_H */
