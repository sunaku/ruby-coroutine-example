require 'mkmf'

have_library('ruby-static', 'ruby_init') ||
have_library('ruby', 'ruby_init')

have_func('ruby_sysinit')

have_header('sys/ucontext.h') ||
have_header('ucontext.h')

create_makefile('main')
