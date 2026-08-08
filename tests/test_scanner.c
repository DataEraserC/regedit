#include "test_runner.h"
#include "core/scanner.h"

#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

void test_scanner(void)
{
    gchar *base = g_dir_make_tmp("lr-test-XXXXXX", NULL);
    gchar *small, *big, *bin, *empty, *fulldir, *inner;
    GPtrArray *arr;
    guint i;
    gboolean saw_small = FALSE, saw_fulldir = FALSE, saw_big = FALSE;
    gboolean saw_bin = FALSE, saw_empty = FALSE;

    TEST_ASSERT(base != NULL);

    /* 1. 小文本文件 → 显示 */
    small = g_build_filename(base, "small.conf", NULL);
    g_file_set_contents(small, "key=value\n", -1, NULL);

    /* 2. 大于 128KB 的文件 → 隐藏 */
    big = g_build_filename(base, "big.bin", NULL);
    {
        FILE *fp = fopen(big, "wb");
        char buf[1024];
        memset(buf, 'x', sizeof(buf));
        for (int k = 0; k < 130; k++)
            fwrite(buf, 1, sizeof(buf), fp);
        fclose(fp);
    }

    /* 3. 含 NUL 的二进制文件 → 隐藏 */
    bin = g_build_filename(base, "data.bin", NULL);
    g_file_set_contents(bin, "abc\0def", 7, NULL);

    /* 4. 空目录 → 隐藏 */
    empty = g_build_filename(base, "emptydir", NULL);
    g_mkdir(empty, 0755);

    /* 5. 非空目录 → 显示 */
    fulldir = g_build_filename(base, "fulldir", NULL);
    g_mkdir(fulldir, 0755);
    inner = g_build_filename(fulldir, "x.conf", NULL);
    g_file_set_contents(inner, "a=1\n", -1, NULL);

    arr = lr_scanner_list_dir(base);
    for (i = 0; i < arr->len; i++)
    {
        LrScanEntry *e = g_ptr_array_index(arr, i);
        if (g_str_equal(e->name, "small.conf"))
            saw_small = TRUE;
        if (g_str_equal(e->name, "fulldir"))
            saw_fulldir = TRUE;
        if (g_str_equal(e->name, "big.bin"))
            saw_big = TRUE;
        if (g_str_equal(e->name, "data.bin"))
            saw_bin = TRUE;
        if (g_str_equal(e->name, "emptydir"))
            saw_empty = TRUE;
    }
    g_ptr_array_unref(arr);

    TEST_ASSERT(saw_small);
    TEST_ASSERT(saw_fulldir);
    TEST_ASSERT(!saw_big);
    TEST_ASSERT(!saw_bin);
    TEST_ASSERT(!saw_empty);

    /* 清理 */
    g_unlink(small);
    g_unlink(big);
    g_unlink(bin);
    g_unlink(inner);
    g_unlink(empty);
    g_unlink(fulldir);
    g_rmdir(base);
    g_free(small);
    g_free(big);
    g_free(bin);
    g_free(inner);
    g_free(fulldir);
    g_free(base);
}
