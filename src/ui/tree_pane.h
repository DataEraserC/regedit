/* 左侧文件树面板：以 /etc 与 ~/.config 为双根，懒加载子目录 */
#ifndef LR_UI_TREE_PANE_H
#define LR_UI_TREE_PANE_H

#include <gtk/gtk.h>

typedef struct _LrTreePane LrTreePane;

/* 树节点被选中时回调：path 为完整路径，is_dir 标记是否为目录 */
typedef void (*LrTreePaneSelectCb)(const char *path, gboolean is_dir,
                                   gpointer user_data);

LrTreePane *lr_tree_pane_new(void);

/* 获取面板顶层 widget（GtkScrolledWindow） */
GtkWidget *lr_tree_pane_get_widget(LrTreePane *self);

/* 设置选中回调 */
void lr_tree_pane_set_select_cb(LrTreePane *self, LrTreePaneSelectCb cb,
                                gpointer user_data);

/* 重新加载根节点（保留选择） */
void lr_tree_pane_refresh(LrTreePane *self);

/* 展开 / 收起全部节点 */
void lr_tree_pane_expand_all(LrTreePane *self);
void lr_tree_pane_collapse_all(LrTreePane *self);

void lr_tree_pane_free(LrTreePane *self);

#endif /* LR_UI_TREE_PANE_H */
