#!/bin/sh
# Usage: sh run.sh DIRECTORIES_WHERE_RUBIES_ARE_INSTALLED

# enable core dumping
ulimit -c unlimited

for prefix in "$@"; do
  echo
  echo "###############################################################"
  echo "# $( "$prefix"/bin/ruby -v )"
  echo "###############################################################"
  echo

  export PATH=$prefix/bin:$PATH

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
