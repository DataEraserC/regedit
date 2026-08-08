project('linux-regedit', 'c',
  version: '0.1.0',
  license: 'GPL-3.0-or-later',
  meson_version: '>=0.60',
  default_options: ['c_std=c11', 'warning_level=3', 'buildtype=debugoptimized'])

glibdep = dependency('glib-2.0')
giodep = dependency('gio-2.0')
gtkdep = dependency('gtk+-3.0')

# 核心库：纯 GLib，不含 UI，便于单元测试
lr_core_sources = files(
  'src/core/value.c',
  'src/core/format.c',
  'src/core/scanner.c',
  'src/core/parsers/common.c',
  'src/core/parsers/ini.c',
  'src/core/parsers/kv.c',
  'src/core/parsers/systemd.c',
  'src/core/parsers/keyword.c',
)

lr_core_inc = include_directories('src')

lr_core = static_library('lr_core', lr_core_sources,
  dependencies: [glibdep, giodep],
  include_directories: lr_core_inc,
)

# 主程序
executable('linux-regedit',
  'src/main.c',
  'src/app.c',
  'src/ui/main_window.c',
  'src/ui/tree_pane.c',
  'src/ui/value_pane.c',
  link_with: lr_core,
  dependencies: [gtkdep, giodep],
  include_directories: lr_core_inc,
  install: true,
)

# 单元测试
if get_option('tests')
  test_runner = executable('test_runner',
    'tests/test_value.c',
    'tests/test_parsers.c',
    'tests/test_runner.c',
    link_with: lr_core,
    dependencies: [glibdep, giodep],
    include_directories: [lr_core_inc, include_directories('tests')],
  )
  test('core-tests', test_runner)
endif
