#!/bin/bash

cat <<'EOS' | xargs --no-run-if-empty -n2 -P0 ./assert.sh
0 'int main() { return 0; }'
42 'int main() { return 42; }'
21 'int main() { return 5+20-4; }'
41 'int main() { return  12 + 34 - 5 ; }'
47 'int main() { return 5+6*7; }'
15 'int main() { return 5*(9-6); }'
4 'int main() { return (3+5)/2; }'
10 'int main() { return -10+20; }'
10 'int main() { return - -10; }'
10 'int main() { return - - +10; }'

0 'int main() { return 0==1; }'
1 'int main() { return 42==42; }'
1 'int main() { return 0!=1; }'
0 'int main() { return 42!=42; }'

1 'int main() { return 0<1; }'
0 'int main() { return 1<1; }'
0 'int main() { return 2<1; }'
1 'int main() { return 0<=1; }'
1 'int main() { return 1<=1; }'
0 'int main() { return 2<=1; }'

1 'int main() { return 1>0; }'
0 'int main() { return 1>1; }'
0 'int main() { return 1>2; }'
1 'int main() { return 1>=0; }'
1 'int main() { return 1>=1; }'
0 'int main() { return 1>=2; }'

3 'int main() { int a; a=3; return a; }'
8 'int main() { int a; int z; a=3; z=5; return a+z; }'
3 'int main() { int a=3; return a; }'
8 'int main() { int a=3; int z=5; return a+z; }'

1 'int main() { return 1; 2; 3; }'
2 'int main() { 1; return 2; 3; }'
3 'int main() { 1; 2; return 3; }'

3 'int main() { int foo=3; return foo; }'
8 'int main() { int foo123=3; int bar=5; return foo123+bar; }'

3 'int main() { if (0) return 2; return 3; }'
3 'int main() { if (1-1) return 2; return 3; }'
2 'int main() { if (1) return 2; return 3; }'
2 'int main() { if (2-1) return 2; return 3; }'

3 'int main() { {1; {2;} return 3;} }'

10 'int main() { int i=0; i=0; while(i<10) i=i+1; return i; }'
55 'int main() { int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} return j; }'

55 'int main() { int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
3 'int main() { for (;;) return 3; return 5; }'

3 'int main() { return ret3(); }'
5 'int main() { return ret5(); }'
8 'int main() { return add(3, 5); }'
2 'int main() { return sub(5, 3); }'
21 'int main() { return add6(1,2,3,4,5,6); }'

32 'int main() { return ret32(); } int ret32() { return 32; }'
7 'int main() { return add2(3,4); } int add2(int x, int y) { return x+y; }'
1 'int main() { return sub2(4,3); } int sub2(int x, int y) { return x-y; }'
55 'int main() { return fib(9); } int fib(int x) { if (x<=1) return 1; return fib(x-1) + fib(x-2); }'

3 'int main() { int x=3; return *&x; }'
3 'int main() { int x=3; int *y=&x; int **z=&y; return **z; }'
5 'int main() { int x=3; int y=5; return *(&x+1); }'
5 'int main() { int x=3; int y=5; return *(1+&x); }'
3 'int main() { int x=3; int y=5; return *(&y-1); }'
2 'int main() { int x=3; return (&x+2)-&x; }'
5 'int main() { int x=3; int y=5; int *z=&x; return *(z+1); }'
3 'int main() { int x=3; int y=5; int *z=&y; return *(z-1); }'
5 'int main() { int x=3; int *y=&x; *y=5; return x; }'
7 'int main() { int x=3; int y=5; *(&x+1)=7; return y; }'
7 'int main() { int x=3; int y=5; *(&y-1)=7; return x; }'
8 'int main() { int x=3; int y=5; return foo(&x, y); } int foo(int *x, int y) { return *x + y; }'

3 'int main() { int x[2]; int *y=&x; *y=3; return *x; }'

3 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *x; }'
4 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+1); }'
5 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+2); }'

0 'int main() { int x[2][3]; int *y=x; *y=0; return **x; }'
1 'int main() { int x[2][3]; int *y=x; *(y+1)=1; return *(*x+1); }'
2 'int main() { int x[2][3]; int *y=x; *(y+2)=2; return *(*x+2); }'
3 'int main() { int x[2][3]; int *y=x; *(y+3)=3; return **(x+1); }'
4 'int main() { int x[2][3]; int *y=x; *(y+4)=4; return *(*(x+1)+1); }'
5 'int main() { int x[2][3]; int *y=x; *(y+5)=5; return *(*(x+1)+2); }'
6 'int main() { int x[2][3]; int *y=x; *(y+6)=6; return **(x+2); }'

3 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *x; }'
4 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+1); }'
5 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); }'
5 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); }'
5 'int main() { int x[3]; *x=3; x[1]=4; 2[x]=5; return *(x+2); }'

0 'int main() { int x[2][3]; int *y=x; y[0]=0; return x[0][0]; }'
1 'int main() { int x[2][3]; int *y=x; y[1]=1; return x[0][1]; }'
2 'int main() { int x[2][3]; int *y=x; y[2]=2; return x[0][2]; }'
3 'int main() { int x[2][3]; int *y=x; y[3]=3; return x[1][0]; }'
4 'int main() { int x[2][3]; int *y=x; y[4]=4; return x[1][1]; }'
5 'int main() { int x[2][3]; int *y=x; y[5]=5; return x[1][2]; }'
6 'int main() { int x[2][3]; int *y=x; y[6]=6; return x[2][0]; }'

8 'int main() { int x; return sizeof(x); }'
8 'int main() { int x; return sizeof x; }'
8 'int main() { int *x; return sizeof(x); }'
32 'int main() { int x[4]; return sizeof(x); }'
96 'int main() { int x[3][4]; return sizeof(x); }'
32 'int main() { int x[3][4]; return sizeof(*x); }'
8 'int main() { int x[3][4]; return sizeof(**x); }'
9 'int main() { int x[3][4]; return sizeof(**x) + 1; }'
9 'int main() { int x[3][4]; return sizeof **x + 1; }'
8 'int main() { int x[3][4]; return sizeof(**x + 1); }'

0 'int x; int main() { return x; }'
3 'int x; int main() { x=3; return x; }'
0 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[0]; }'
1 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[1]; }'
2 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[2]; }'
3 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[3]; }'

8 'int x; int main() { return sizeof(x); }'
32 'int x[4]; int main() { return sizeof(x); }'

1 'int main() { char x=1; return x; }'
1 'int main() { char x=1; char y=2; return x; }'
2 'int main() { char x=1; char y=2; return y; }'

1 'int main() { char x; return sizeof(x); }'
10 'int main() { char x[10]; return sizeof(x); }'
1 'int main() { return sub_char(7, 3, 3); } int sub_char(char a, char b, char c) { return a-b-c; }'

97 'int main() { return "abc"[0]; }'
98 'int main() { return "abc"[1]; }'
99 'int main() { return "abc"[2]; }'
0 'int main() { return "abc"[3]; }'
4 'int main() { return sizeof("abc"); }'
EOS

if [ $? -gt 0 ]; then
  echo 'not ok'
  exit 1
fi
echo OK
