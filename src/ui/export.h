/* 导出为 .lreg：展示导出对话框 */
#ifndef LR_UI_EXPORT_H
#define LR_UI_EXPORT_H

#include <gtk/gtk.h>
#include "ui/main_window.h"

/* 显示导出对话框（文件→导出…） */
void lr_export_show_dialog(GtkWidget *widget, gpointer user_data);

#endif /* LR_UI_EXPORT_H */
