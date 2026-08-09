/* 右侧配置项面板：解析后的配置项表格，或文本兜底视图 */
#ifndef LR_UI_VALUE_PANE_H
#define LR_UI_VALUE_PANE_H

#include <gtk/gtk.h>

typedef struct _LrValuePane LrValuePane;

LrValuePane *lr_value_pane_new(void);

/* 获取面板顶层 widget（GtkStack） */
GtkWidget *lr_value_pane_get_widget(LrValuePane *self);

/* 加载并展示一个配置文件（自动检测格式；不支持则文本兜底） */
void lr_value_pane_load_file(LrValuePane *self, const char *path);

/* 清空为占位视图 */
void lr_value_pane_clear(LrValuePane *self);

/* 在表格末尾追加一个配置项（仅内存，不写盘）；type ∈ Section/String/Boolean/Number */
void lr_value_pane_add_value(LrValuePane *self, const char *type);

void lr_value_pane_free(LrValuePane *self);

#endif /* LR_UI_VALUE_PANE_H */
