#!/bin/bash

function compile {
  echo "$1" | ./8cc > tmp.s
  if [ $? -ne 0 ]; then
    echo "Failed to compile $1"
    exit
  fi
  gcc -o tmp.out driver.c tmp.s
  if [ $? -ne 0 ]; then
    echo "GCC failed: $1"
    exit
  fi
}

function assertequal {
  if [ "$1" != "$2" ]; then
    echo "Test failed: $2 expected but got $1"
    exit
  fi
}

function testastf {
  result="$(echo "$2" | ./8cc -a)"
  if [ $? -ne 0 ]; then
    echo "Failed to compile $1 because gotten $result"
    exit
  fi
  assertequal "$result" "$1"
}

function testast {
  testastf "$1" "int f(){$2}"
}

function testf {
  compile "$2"
  assertequal "$(./tmp.out)" "$1"
}

function test {
  testf "$1" "int f(){$2}"
}

function testfail {
  expr="int f(){$1}"
  echo "$expr" | ./8cc > /dev/null 2>&1
  if [ $? -eq 0 ]; then
    echo "Should fail to compile, but succeded: $expr"
    exit
  fi
}

function emit {
  if [ $c -eq 0 ]; then 
    rm -rf "./assblr"
    mkdir "./assblr"
  fi
  if [ $c -lt 10 ]; then
    fname="./assblr/""0""$c.s"
  else
    fname="./assblr/$c.txt"
  fi
  echo "int f(){$1}" > "$fname"
  echo "int f(){$1}" | ./8cc >> "$fname"
  c=`expr $c + 1`
}

make -s 8cc

# Emit Assblr
c=0
emit '1+2*3+4;'
emit  '1;'
emit 'int a=3;'
emit 'int a=3;int *b=&a;*b;'
emit 'int a[3]={20,30,40};int *b=a+1;*b;'
emit 'int a=3;int *b=&a;*b+5;'
emit 'if(1){printf("x");}else{printf("y");}1;'
emit 'if(0){printf("x");}else{printf("y");}1;'
emit 'for(int a=1;2;7){5;}'

# Parser
testast '(int)f(){1;}' '1;'
testast '(int)f(){(+ (- (+ 1 2) 3) 4);}' '1+2-3+4;'
testast '(int)f(){(+ (+ 1 (* 2 3)) 4);}' '1+2*3+4;'
testast '(int)f(){(+ (* 1 2) (* 3 4));}' '1*2+3*4;'
testast '(int)f(){(+ (/ 4 2) (/ 6 3));}' '4/2+6/3;'
testast '(int)f(){(/ (/ 24 2) 4);}' '24/2/4;'
testast '(int)f(){(decl int a 3);}' 'int a=3;'
testast "(int)f(){(decl char c 'a');}" "char c='a';"
testast '(int)f(){(decl char* s "abcd");}' 'char *s="abcd";'
testast '(int)f(){(decl char[5] s "asdf");}' 'char s[5]="asdf";'
testast '(int)f(){(decl char[5] s "asdf");}' 'char s[]="asdf";'
testast '(int)f(){(decl int[3] a {1,2,3});}' 'int a[3]={1,2,3};'
testast '(int)f(){(decl int[3] a {1,2,3});}' 'int a[]={1,2,3};'
testast '(int)f(){(decl int a 1);(decl int b 2);(= a (= b 3));}' 'int a=1;int b=2;a=b=3;'
testast '(int)f(){(decl int a 3);(& a);}' 'int a=3;&a;'
testast '(int)f(){(decl int a 3);(* (& a));}' 'int a=3;*&a;'
testast '(int)f(){(decl int a 3);(decl int* b (& a));(* b);}' 'int a=3;int *b=&a;*b;'
testast '(int)f(){(if 1 {2;});}' 'if(1){2;}'
testast '(int)f(){(if 1 {2;} {3;});}' 'if(1){2;}else{3;}'
testast '(int)f(){(for (decl int a 1) 3 7 {5;});}' 'for(int a=1;3;7){5;}'
testast '(int)f(){"abcd";}' '"abcd";'
testast "(int)f(){'c';}" "'c';"
testast '(int)f(){(int)a();}' 'a();'
testast '(int)f(){(int)a(1,2,3,4,5,6);}' 'a(1,2,3,4,5,6);'
testast '(int)f(){(return 1);}' 'return 1;'

testastf '(int)f(int c){c;}' 'int f(int c){c;}'
testastf '(int)f(int c){c;}(int)g(int d){d;}' 'int f(int c){c;} int g(int d){d;}'

# Basic arithmetic
test 0 '0;'
test 3 '1+2;'
test 3 '1 + 2;'
test 10 '1+2+3+4;'
test 11 '1+2*3+4;'
test 14 '1*2+3*4;'
test 4 '4/2+6/3;'
test 4 '24/2/3;'
test 98 "'a'+1;"
test 2 '1;2;'

# Comparison
test 1 '1<2;'
test 0 '2<1;'

# Declaration
test 3 'int a=1;a+2;'
test 102 'int a=1;int b=48+2;int c=a+b;c*2;'
test 55 'int a[]={55};int *b=a;*b;'
test 67 'int a[]={55,67};int *b=a+1;*b;'
test 30 'int a[]={20,30,40};int *b=a+1;*b;'
test 20 'int a[]={20,30,40};*a;'

# Function call
test a3 'printf("a");3;'
#test xy5 'printf("%s", "xy");5;' # Test failed 
#test b1 "printf(\"%c\", 'a'+1);1;" # Test failed

# Pointer
test 61 'int a=61;int *b=&a;*b;'
test 97 'char *c="ab";*c;'
test 98 'char *c="ab"+1;*c;'
test 122 'char s[]="xyz";char *c=s+2;*c;'
test 65 'char s[]="xyz";*s=65;*s;'
emit 'char s[]="xyz";*s=65;*s;'

# If statement
test 'a1' 'if(1){printf("a");}1;'
test '1' 'if(0){printf("a");}1;'
test 'x1' 'if(1){printf("x");}else{printf("y");}1;'
test 'y1' 'if(0){printf("x");}else{printf("y");}1;'

# For statement
test 012340 'for(int i=0; i<5; i=i+1){printf("%d",i);}0;'

# Return statement
test 33 'return 33; return 10;'

# Function parameter
testf '102' 'int f(int n){n;}'
testf 77 'int g(){77;} int f(){g();}'
testf 79 'int g(int a){a;} int f(){g(79);}'
testf 21 'int g(int a,int b,int c,int d,int e,int f){a+b+c+d+e+f;} int f(){g(1,2,3,4,5,6);}'
testf 79 'int g(int a){a;} int f(){g(79);}'
testf 98 'int g(int *p){*p;} int f(){int a[]={98};g(a);}'
testf '99 98 97 1' 'int g(int *p){printf("%d ",*p);p=p+1;printf("%d ",*p);p=p+1;printf("%d ",*p);1;} int f(){int a[]={1,2,3};int *p=a;*p=99;p=p+1;*p=98;p=p+1;*p=97;g(a);}'

testfail '0abc;'
testfail '1+;'
testfail '1=2;'

# & is only applicable to an lvalue
testfail '&"a";'
testfail '&1;'
testfail '&a();'

echo "All tests passed"