#!/bin/sh

for dir in ~/.multiruby/install/*
do
  ver=$(basename "$dir")
  echo $ver

  (
    export PATH=$dir/bin:$PATH

    # enable core dumping
    ulimit -c unlimited

    # compile and run test case
    ruby -v extconf.rb && sed -i 's,-shared,,g' Makefile && make && ./main.so ||

    # inspect the core dump, if any
    test -f core && echo "bt full" > $$ && gdb -batch -x $$ ./main.so core

    # clean up
    rm -f $$
    make distclean

  ) >& "$ver.log"
done

