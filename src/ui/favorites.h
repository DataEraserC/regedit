/* 收藏夹：/run 临时目录持久化的快速跳转（重启清空） */
#ifndef LR_UI_FAVORITES_H
#define LR_UI_FAVORITES_H

#include <gtk/gtk.h>
#include "ui/main_window.h"

/* 填充“收藏夹”菜单内容（在菜单 show 时重建，动态反映当前收藏项） */
void lr_favorites_fill_menu(GtkWidget *menu, gpointer user_data);

#endif /* LR_UI_FAVORITES_H */
