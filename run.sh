#!/bin/sh
#
# Usage: sh run.sh <coroutine> <allocation> <ruby_install_prefix>...
#
# Where: <coroutine> is "pcl", "pthread", or "ucontext" library
# Where: <allocation> is "static" or "dynamic" stack allocation
# Where: <ruby_install_prefix> is a Ruby installation's $PREFIX
#

# check command-line usage
if test "$#" -lt 3; then
  sed -n '2,/^$/p' "$0" # show this file's comment header
  exit 1
fi

# enable core dumping
ulimit -c unlimited

coroutine=$(echo "$1" | tr 'a-z' 'A-Z'); shift
allocation=$(echo "$1" | tr 'a-z' 'A-Z'); shift

for prefix in "$@"; do
  # use the Ruby installed at $prefix
  old_path=$PATH
  export PATH=$prefix:$prefix/bin:$PATH

  echo
  echo "###############################################################"
  echo "# $( ruby -v )"
  echo "# coroutine library: $coroutine"
  echo "# coroutine stack allocation: $allocation"
  echo "###############################################################"
  echo

  # compile and run test case
  ruby extconf.rb &&
  sed -i 's/-shared//g' Makefile &&
  sed -i "s/^CPPFLAGS =/& -DDEMONSTRATE_$coroutine -DDEMONSTRATE_$allocation/" Makefile &&
  make && time ./main.so ||

  # inspect the core dump, if any
  test -f core &&
  echo "bt full" > $$ &&
  gdb -batch -x $$ ./main.so core < /dev/null

  # clean up
  PATH=$old_path
  rm -f $$
  make distclean
done
