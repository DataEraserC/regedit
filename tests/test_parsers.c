#include "test_runner.h"
#include "core/format.h"

#include <glib/gstdio.h>

static gchar *
write_tmp(const gchar *name, const gchar *content)
{
    gchar *path = g_build_filename(g_get_tmp_dir(), name, NULL);
    g_file_set_contents(path, content, -1, NULL);
    return path;
}

void test_parsers(void)
{
    /* ---------- INI ---------- */
    {
        gchar *p = write_tmp("lr-test-1.ini",
                             "# 顶部注释\n"
                             "[server]\n"
                             "Port = 22  # 端口\n"
                             "Enable = yes\n"
                             "; 第二注释\n"
                             "Name = \"my host\"\n");
        LrConfigFile *f = lr_parse_config(p);

        TEST_ASSERT(lr_format_detect(p) == LR_FORMAT_INI);
        TEST_ASSERT(f->parsed);
        TEST_ASSERT(f->items->len == 3);

        LrConfigItem *it = g_ptr_array_index(f->items, 0);
        TEST_ASSERT_STR_EQ(it->key, "Port");
        TEST_ASSERT(it->type == LR_VALUE_NUMBER);
        TEST_ASSERT_STR_EQ(it->data, "22");
        TEST_ASSERT_STR_EQ(it->section, "server");
        /* 上方注释 + 行内注释合并显示 */
        TEST_ASSERT_STR_EQ(it->comment, "顶部注释\n端口");

        it = g_ptr_array_index(f->items, 1);
        TEST_ASSERT_STR_EQ(it->key, "Enable");
        TEST_ASSERT(it->type == LR_VALUE_BOOL);
        TEST_ASSERT_STR_EQ(it->data, "yes");
        /* 注释未被消费：下方配置项共用上方最近一条注释 */
        TEST_ASSERT_STR_EQ(it->comment, "顶部注释");

        it = g_ptr_array_index(f->items, 2);
        TEST_ASSERT_STR_EQ(it->key, "Name");
        TEST_ASSERT(it->type == LR_VALUE_STRING);
        TEST_ASSERT_STR_EQ(it->data, "my host");
        TEST_ASSERT_STR_EQ(it->comment, "第二注释");

        lr_config_file_free(f);
        g_unlink(p);
        g_free(p);
    }

    /* ---------- 扁平 KeyValue ---------- */
    {
        gchar *p = write_tmp("lr-test-2.conf",
                             "# 环境变量\n"
                             "PATH=/usr/local/bin:/usr/bin\n"
                             "LANG=en_US.UTF-8\n"
                             "DEBUG=false\n");
        LrConfigFile *f = lr_parse_config(p);

        TEST_ASSERT(lr_format_detect(p) == LR_FORMAT_KV);
        TEST_ASSERT(f->parsed);
        TEST_ASSERT(f->items->len == 3);

        LrConfigItem *it = g_ptr_array_index(f->items, 0);
        TEST_ASSERT_STR_EQ(it->key, "PATH");
        TEST_ASSERT(it->type == LR_VALUE_STRING);
        TEST_ASSERT_STR_EQ(it->comment, "环境变量");

        /* 同一条注释说明其下方多个配置项 */
        it = g_ptr_array_index(f->items, 1);
        TEST_ASSERT_STR_EQ(it->key, "LANG");
        TEST_ASSERT_STR_EQ(it->comment, "环境变量");

        it = g_ptr_array_index(f->items, 2);
        TEST_ASSERT_STR_EQ(it->key, "DEBUG");
        TEST_ASSERT(it->type == LR_VALUE_BOOL);
        TEST_ASSERT_STR_EQ(it->comment, "环境变量");

        lr_config_file_free(f);
        g_unlink(p);
        g_free(p);
    }

    /* ---------- systemd unit ---------- */
    {
        gchar *p = write_tmp("lr-test-3.service",
                             "[Unit]\n"
                             "Description=My demo service\n"
                             "After=network.target\n"
                             "\n"
                             "[Service]\n"
                             "Type=simple\n"
                             "Restart=on-failure\n"
                             "# 运行用户\n"
                             "User=www-data\n");
        LrConfigFile *f = lr_parse_config(p);

        TEST_ASSERT(lr_format_detect(p) == LR_FORMAT_SYSTEMD);
        TEST_ASSERT(f->parsed);
        TEST_ASSERT(f->items->len == 5);

        LrConfigItem *it = g_ptr_array_index(f->items, 0);
        TEST_ASSERT_STR_EQ(it->key, "Description");
        TEST_ASSERT_STR_EQ(it->section, "Unit");
        TEST_ASSERT(it->type == LR_VALUE_STRING);

        it = g_ptr_array_index(f->items, 2);
        TEST_ASSERT_STR_EQ(it->key, "Type");
        TEST_ASSERT_STR_EQ(it->section, "Service");

        it = g_ptr_array_index(f->items, 3);
        TEST_ASSERT_STR_EQ(it->key, "Restart");
        TEST_ASSERT(it->type == LR_VALUE_STRING);
        TEST_ASSERT_STR_EQ(it->data, "on-failure");

        /* 最后一项 User 应带注释 */
        lr_config_file_free(f);

        /* 重新解析以检查注释归属 */
        f = lr_parse_config(p);
        LrConfigItem *last = g_ptr_array_index(f->items, f->items->len - 1);
        TEST_ASSERT_STR_EQ(last->key, "User");
        TEST_ASSERT_STR_EQ(last->comment, "运行用户");

        lr_config_file_free(f);
        g_unlink(p);
        g_free(p);
    }

    /* ---------- 未知格式（文本兜底） ---------- */
    {
        gchar *p = write_tmp("lr-test-4.log", "这是一个普通日志\n第二行\n");
        TEST_ASSERT(lr_format_detect(p) == LR_FORMAT_UNKNOWN);
        TEST_ASSERT(!lr_format_supported(LR_FORMAT_UNKNOWN));
        g_unlink(p);
        g_free(p);
    }

    /* ---------- shebang 脚本：以文本形式打开 ---------- */
    {
        gchar *p = write_tmp("lr-test-6.sh",
                             "#!/bin/bash\n"
                             "# 配置脚本\n"
                             "echo hello\n");
        TEST_ASSERT(lr_format_detect(p) == LR_FORMAT_UNKNOWN);
        TEST_ASSERT(!lr_format_supported(LR_FORMAT_UNKNOWN));
        g_unlink(p);
        g_free(p);
    }

    /* ---------- 关键字-参数（sshd_config 风格） ---------- */
    {
        gchar *p = write_tmp("lr-test-5.conf",
                             "# 注释行\n"
                             "#Port 22\n"
                             "Include /etc/ssh/sshd_config.d/*.conf\n"
                             "KbdInteractiveAuthentication no\n"
                             "Port 22\n"
                             "Subsystem\tsftp\t/usr/lib/openssh/sftp-server\n");
        LrConfigFile *f = lr_parse_config(p);

        TEST_ASSERT(lr_format_detect(p) == LR_FORMAT_KEYWORD);
        TEST_ASSERT(f->parsed);
        TEST_ASSERT(f->items->len == 5);

        /* 被注释的配置：#Port 22 → Port 未启用 */
        LrConfigItem *it = g_ptr_array_index(f->items, 0);
        TEST_ASSERT_STR_EQ(it->key, "Port");
        TEST_ASSERT_STR_EQ(it->data, "22");
        TEST_ASSERT(it->type == LR_VALUE_NUMBER);
        TEST_ASSERT(!it->enabled);
        /* 上方最近一条说明文字作为备注 */
        TEST_ASSERT_STR_EQ(it->comment, "注释行");

        it = g_ptr_array_index(f->items, 1);
        TEST_ASSERT_STR_EQ(it->key, "Include");
        TEST_ASSERT_STR_EQ(it->data, "/etc/ssh/sshd_config.d/*.conf");
        TEST_ASSERT(it->type == LR_VALUE_STRING);
        TEST_ASSERT(it->enabled);
        TEST_ASSERT_STR_EQ(it->comment, "注释行");

        it = g_ptr_array_index(f->items, 2);
        TEST_ASSERT_STR_EQ(it->key, "KbdInteractiveAuthentication");
        TEST_ASSERT_STR_EQ(it->data, "no");
        TEST_ASSERT(it->type == LR_VALUE_BOOL);

        it = g_ptr_array_index(f->items, 3);
        TEST_ASSERT_STR_EQ(it->key, "Port");
        TEST_ASSERT_STR_EQ(it->data, "22");
        TEST_ASSERT(it->type == LR_VALUE_NUMBER);

        /* Tab 分隔 */
        it = g_ptr_array_index(f->items, 4);
        TEST_ASSERT_STR_EQ(it->key, "Subsystem");
        TEST_ASSERT_STR_EQ(it->data, "sftp /usr/lib/openssh/sftp-server");

        lr_config_file_free(f);
        g_unlink(p);
        g_free(p);
    }
}
