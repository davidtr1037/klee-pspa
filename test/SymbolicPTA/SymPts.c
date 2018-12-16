// RUN: %llvmgcc -I../../../include -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf  %t.klee-out %t.klee-outSanity
// RUN: %klee -sym-pta -collect-pta-results -collect-modref  -pta-target=useStruct --output-dir=%t.klee-out %t.bc &> %t.log 
// RUN: grep '<badref> = call' %t.log | wc -l | grep 5
// RUN: grep '<badref> = call' %t.log | sed -n 's/.*i64 \([0-9]\+\).*/\1/p' | tr '\n' '-' | grep 7-10-11-12-13
// RUN: %klee -sym-pta-sanity -collect-pta-results -collect-modref  -pta-target=useStruct --output-dir=%t.klee-outSanity %t.bc 

#include <stdio.h>

void* malloc(size_t, char*);

struct mp {
    char *str;
    short y;
    int *values;
};

void useStruct(struct mp* obj) {
    obj->values[0] = 2;
    obj->str[1] = 'a';
}



int main() {
  struct mp* mainObj = malloc(sizeof(struct mp), "mainObj");
  mainObj->str = malloc(6, "str - 1");
  mainObj->str = malloc(7, "str - 2");


  int **ptrs[4];
  ptrs[0] = malloc(19, "ptr0");
  ptrs[0] = malloc(10, "ptr0.1");
  ptrs[1] = malloc(11, "ptr1");
  ptrs[2] = malloc(18, "ptr2");
  ptrs[2] = malloc(12, "ptr2.1");
  ptrs[3] = malloc(13, "ptr3");

  unsigned i;
  klee_make_symbolic(&i, sizeof(i), "i");
  klee_assume(i < 4);

  mainObj->values = malloc(2, "concrete");
  mainObj->values = ptrs[i];

  useStruct(mainObj);
  return 0;
}
