project('dotmine', 'c',
  version : '0.2',
  default_options : ['warning_level=2', 'c_std=gnu11'])

name = meson.project_name()
version = meson.project_version()

add_project_arguments(
  f'-DNAME="@name@"',
  f'-DVERSION="@version@"',
  language : ['c']
)

if get_option('buildtype') == 'debug'
  add_project_arguments('-DDEBUG', language: ['c'])
endif

executable(
  name,
  'src/main.c',
  'src/flags.c',
  'src/utils.c',
  include_directories: include_directories('include'),
  install : true
)
