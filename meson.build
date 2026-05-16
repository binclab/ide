project('BIDE', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=gnu11'])

gtkdep = dependency('gtk4')
srcdep = dependency('gtksourceview-5')

subdir('res')

executable(
  'bide',
  resources,
  'src/main.c',
  'src/editor.c',
  'src/file_browser.c',
  dependencies : [gtkdep, srcdep],
  install : true,
)