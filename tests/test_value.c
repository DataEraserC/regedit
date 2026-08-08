#include "test_runner.h"
#include "core/value.h"

void
test_value_types(void)
{
    /* 数字 */
    TEST_ASSERT(lr_value_detect_type("123") == LR_VALUE_NUMBER);
    TEST_ASSERT(lr_value_detect_type("-42") == LR_VALUE_NUMBER);
    TEST_ASSERT(lr_value_detect_type("+7") == LR_VALUE_NUMBER);
    TEST_ASSERT(lr_value_detect_type("3.14") == LR_VALUE_NUMBER);
    TEST_ASSERT(lr_value_detect_type("0.5") == LR_VALUE_NUMBER);
    TEST_ASSERT(lr_value_detect_type("0x1F") == LR_VALUE_NUMBER);
    TEST_ASSERT(lr_value_detect_type("1e3") == LR_VALUE_NUMBER);

    /* 布尔值 */
    TEST_ASSERT(lr_value_detect_type("true") == LR_VALUE_BOOL);
    TEST_ASSERT(lr_value_detect_type("FALSE") == LR_VALUE_BOOL);
    TEST_ASSERT(lr_value_detect_type("Yes") == LR_VALUE_BOOL);
    TEST_ASSERT(lr_value_detect_type("no") == LR_VALUE_BOOL);
    TEST_ASSERT(lr_value_detect_type("on") == LR_VALUE_BOOL);
    TEST_ASSERT(lr_value_detect_type("off") == LR_VALUE_BOOL);
    TEST_ASSERT(lr_value_detect_type("1") == LR_VALUE_BOOL);
    TEST_ASSERT(lr_value_detect_type("0") == LR_VALUE_BOOL);

    /* 字符串 */
    TEST_ASSERT(lr_value_detect_type("hello") == LR_VALUE_STRING);
    TEST_ASSERT(lr_value_detect_type("") == LR_VALUE_STRING);
    TEST_ASSERT(lr_value_detect_type("abc123") == LR_VALUE_STRING);
    TEST_ASSERT(lr_value_detect_type("on-failure") == LR_VALUE_STRING);
    TEST_ASSERT(lr_value_detect_type("192.168.1.1") == LR_VALUE_STRING);

    /* 类型显示名 */
    TEST_ASSERT_STR_EQ(lr_value_type_name(LR_VALUE_NUMBER), "数字");
    TEST_ASSERT_STR_EQ(lr_value_type_name(LR_VALUE_BOOL), "布尔值");
    TEST_ASSERT_STR_EQ(lr_value_type_name(LR_VALUE_STRING), "字符串");
}
