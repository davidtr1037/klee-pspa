// RUN: %llvmgcc -I../../../include -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf  %t.klee-out %t.klee-outSanity
// RUN: %klee -use-pta-mode=symbolic -collect-pta-results -collect-modref  -pta-target=useVal --output-dir=%t.klee-out %t.bc &> %t.log 
// RUN: grep '<badref> = call' %t.log | wc -l | grep 2
// RUN: grep '<badref> = call' %t.log | sed -n 's/.*i64 \([0-9]\+\).*/\1/p' | tr '\n' '-' | grep 2-7

#include <stdio.h>

void* mymalloc(size_t, char*);
#define malloc mymalloc

struct mp {
    char *str;
    short y;
    int *values;
};

void useVal(void** c) {
    **(char**)c = 5;
}



int main() {
  struct mp* mainObj = malloc(sizeof(struct mp), "mainObj");
  mainObj->str = malloc(6, "str - 1");
  mainObj->str = malloc(7, "str - 2");

  mainObj->values = malloc(2, "concrete");

  unsigned b;
  klee_make_symbolic(&b, sizeof(b), "b");
  klee_assume(b < 2);
  
  void **p = malloc(sizeof(char*) * 2, "p obj");
  p[0] = mainObj->values;
  p[1] = mainObj->str;


  useVal(&p[b]);
  return 0;
}
