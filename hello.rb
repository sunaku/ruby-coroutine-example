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

def recurse
  relay_from_ruby_to_main

  n = caller.length
  if n < 500
    puts "recursing at #{n}..."
    Thread.new { recurse }.join
  end
end

recurse

puts 'GOOD JOB! :-)'
