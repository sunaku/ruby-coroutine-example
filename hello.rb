puts "~~~~> Hello World!  The time is #{Time.now}."

puts "~~~~> Doing relay from #{__FILE__}:#{__LINE__}"
relay_from_ruby_to_main
puts "~~~~> OMG, back from main!! :-)"

require 'date'
p Date.today

require 'fileutils'
p FileUtils
include FileUtils::Verbose
touch 'foobar'

puts 'GOOD JOB! :-)'
