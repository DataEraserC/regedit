/* 窗口会话状态：保存/恢复窗口几何与上次路径
 * 状态文件：/run/user/<uid>/<app_id>/state.ini（GKeyFile）
 * 全部使用缓存值，不依赖窗口实时状态，避免关闭时查询失效。 */
#ifndef LR_UI_WINDOW_STATE_H
#define LR_UI_WINDOW_STATE_H

#include <glib.h>

typedef struct _LrWindowState LrWindowState;

/* 创建状态管理器（app_id 作为 /run 下的状态目录名） */
LrWindowState *lr_window_state_new(const char *app_id);

void lr_window_state_free(LrWindowState *self);

/* 从磁盘恢复。成功读取到状态返回 TRUE，并将上次路径写入 *last_path
 * （新分配，无则置 NULL）；无状态文件返回 FALSE。 */
gboolean lr_window_state_restore(LrWindowState *self, char **last_path);

/* 记录位置与尺寸（configure 事件中调用）。
 * 尺寸仅在非最大化时更新（最大化时窗口尺寸无参考意义）。 */
void lr_window_state_set_geometry(LrWindowState *self, gint x, gint y,
                                  gint w, gint h);

/* 记录最大化状态 */
void lr_window_state_set_maximized(LrWindowState *self, gboolean maximized);

/* 显式记录非最大化尺寸（window-state 事件中调用） */
void lr_window_state_set_size(LrWindowState *self, gint w, gint h);

/* 读取恢复后的几何与状态 */
void lr_window_state_get_size(LrWindowState *self, gint *w, gint *h);
void lr_window_state_get_pos(LrWindowState *self, gint *x, gint *y);
gboolean lr_window_state_is_maximized(LrWindowState *self);

/* 设置本次会话的当前路径（保存时写入） */
void lr_window_state_set_path(LrWindowState *self, const char *path);

/* 节流保存（延迟 LR_WINDOW_STATE_SAVE_MS 毫秒后写盘） */
void lr_window_state_schedule_save(LrWindowState *self);

/* 立即保存到磁盘（关闭窗口时调用） */
void lr_window_state_save_now(LrWindowState *self);

#endif /* LR_UI_WINDOW_STATE_H */
