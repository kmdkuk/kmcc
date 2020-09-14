#!/bin/bash

expected="$1"
input="$2"
filename="tmp$(uuidgen)"

./kmcc "$input" > ${filename}.s
cat <<EOF | gcc -xc -c -o ${filename}2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }
int add6(int a, int b, int c, int d, int e, int f) {
  return a+b+c+d+e+f;
}
EOF
gcc -static -o ${filename} ${filename}.s ${filename}2.o
./${filename}
actual="$?"

rm -f ${filename} ${filename}.s ${filename}2.o

if [ "$actual" = "$expected" ]; then
  echo "$input => $actual"
else
  echo "$input => $expected expected, but got $actual"
  exit 1
fi
