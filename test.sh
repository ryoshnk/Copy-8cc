#!/bin/bash

function compile {
  echo "$1" | ./8cc > tmp.s
  if [ $? -ne 0 ]; then
    echo "Failed to compile $1"
    exit
  fi
  
  n='^[0-9]'
  if ! [[ $1 =~ $n ]] ; then
    gcc -o tmp.out tmp.s driver.c
  else 
    gcc -o tmp.out tmp.s -D INTFN driver.c
  fi

  if [ $? -ne 0 ]; then
    echo "GCC failed"
    exit
  fi
}

function test {
  expected="$1"
  expr="$2"
  compile "$expr"
  result="`./tmp.out`"
  if [ "$result" != "$expected" ]; then
    echo "Test failed: $expected expected but got $result"
    exit
  fi
}

function testfail {
  expr="$1"
  echo "$expr" | ./8cc > /dev/null 2>&1
  if [ $? -eq 0 ]; then
    echo "Should fail to compile, but succeded: $expr"
    exit
  fi
}

make -s 8cc

test 0 0
test abc '"abc"'

test 3 '2+1'
#test 3 '1 + 2'
#test 10 '1+2+3+4'

testfail '"abc'
testfail '0abc'
testfail '2+'

echo "All tests passed"