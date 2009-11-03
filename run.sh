#!/bin/sh

# enable core dumping
ulimit -c unlimited

for dir in ~/.multiruby/install/*
do
  ver=$(basename "$dir")

  echo
  echo "###############################################################"
  echo "# $( "$dir"/bin/ruby -v )"
  echo "###############################################################"
  echo

  export PATH=$dir/bin:$PATH

  # compile and run test case
  ruby -v extconf.rb &&
  sed -i 's,-shared,,g' Makefile &&
  make && time ./main.so ||

  # inspect the core dump, if any
  test -f core &&
  echo "bt full" > $$ &&
  gdb -batch -x $$ ./main.so core < /dev/null

  # clean up
  rm -f $$
  make distclean
done

