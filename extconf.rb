require 'mkmf'

have_library('ruby-static', 'ruby_init') ||
have_library('ruby', 'ruby_init')

create_makefile('main')
