/* 主窗口：菜单栏 + 左右分栏（文件树 | 配置项面板）+ 状态栏 */
#ifndef LR_UI_MAIN_WINDOW_H
#define LR_UI_MAIN_WINDOW_H

#include <gtk/gtk.h>
#include "ui/tree_pane.h"
#include "ui/value_pane.h"

typedef struct _LrMainWindow LrMainWindow;

LrMainWindow *lr_main_window_new(void);
GtkWidget *lr_main_window_get_window(LrMainWindow *self);
void lr_main_window_free(LrMainWindow *self);

/* 从 /run 会话状态恢复窗口大小、位置与上次路径 */
void lr_main_window_restore_state(LrMainWindow *self);

#endif /* LR_UI_MAIN_WINDOW_H */
