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

def yasume
  Thread.pass if rand(2) == 1
end

def recurse(n = 0)
  relay_from_ruby_to_main
  yasume

  if n < 500
    print "#{Thread.current} recursing at #{n}...\n"
    yasume

    recurse(n + 1)
    yasume
  end
end

threads = 10.times.map do
  Thread.new { recurse }
end

Thread.pass while threads.any? {|t| t.alive? }

puts 'GOOD JOB! :-)'
