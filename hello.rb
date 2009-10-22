puts "~~~~> Hello World!  The time is #{Time.now}."

puts "~~~~> Doing relay from #{__FILE__}:#{__LINE__}"
relay_from_ruby_to_main
puts "~~~~> OMG, back from main!! :-)"

##
# the following line causes a
# "[BUG] object allocation during
# garbage collection phase" error
#
require 'date'  # <== ERROR!
