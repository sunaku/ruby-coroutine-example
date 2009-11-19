require 'mkmf'

have_library('ruby-static', 'ruby_init') ||
have_library('ruby', 'ruby_init')

have_func('ruby_sysinit')
have_func('ruby_bind_stack')

have_library('pthread')
have_func('pthread_attr_setstack')

create_makefile('main')
