#!/bin/bash

function compile {
  echo "$2" | ./8cc > tmp.s
  if [ $? -ne 0 ]; then
    echo "Failed to compile $1"
    exit
  fi
  
  if [[ $1 == "-S" ]] ; then
    gcc -o tmp.out tmp.s driver.c
  elif [[ $1 == "-I" ]] ; then
    gcc -o tmp.out tmp.s -D INTFN driver.c
  fi

  if [ $? -ne 0 ]; then
    echo "GCC failed"
    exit
  fi
}

function assertequal {
  if [ "$1" != "$2" ]; then
    echo "Test failed: $2 expected but got $1"
    exit
  fi
}

function testast {
  result="$(echo "$2" | ./8cc -a)"
  if [ $? -ne 0 ]; then
    echo "Failed to compile $1"
    exit
  fi
  assertequal "$result" "$1"
}

function test {
  if [ $# -ne 3 ]; then
  echo "Should need 3 args, but typed $# args."
  exit 1
  fi
  compile "$1" "$3"
  assertequal "$(./tmp.out)" "$2"
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

testast '1' '1'
testast '(+ (- (+ 1 2) 3) 4)' '1+2-3+4'
testast '(+ (+ 1 (* 2 3)) 4)' '1+2*3+4'
testast '(+ (* 1 2) (* 3 4))' '1*2+3*4'
testast '(+ (/ 4 2) (/ 6 3))' '4/2+6/3'
testast '(/ (/ 24 2) 4)' '24/2/4'

test -I 0 0
test -S abc '"abc"'

test -I 3 '1+2'
test -I 3 '1 + 2'
test -I 10 '1+2+3+4'
test -I 4 '1+2-3+4'
test -I 11 '1+2*3+4'
test -I 14 '1*2+3*4'
test -I 4 '4/2+6/3'
test -I 3 '24/2/4'

testfail '"abc'
testfail '0abc'
testfail '1+'
testfail '1+"abc"'

echo "All tests passed"
