require 'mkmf'

# ruby
have_library('ruby-static', 'ruby_init') ||
have_library('ruby', 'ruby_init')

have_func('ruby_sysinit')     # ruby 1.9
have_func('ruby_bind_stack')  # http://redmine.ruby-lang.org/issues/show/2294

# pcl
have_library('pcl', 'co_create')

# ucontext
have_header('sys/ucontext.h') ||
have_header('ucontext.h')

# pthread
have_library('pthread')
have_func('pthread_attr_setstack')

create_makefile('main')
