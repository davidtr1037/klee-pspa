// RUN: %llvmgcc -I../../../include -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee -collect-pta-results -collect-modref  -pta-target=useStruct --output-dir=%t.klee-out --write-kqueries %t.bc > %t.log ; false

#include <stdio.h>

void* malloc(size_t, char*);

struct mp {
    char *str;
    short y;
    int *values;
};

void useStruct(struct mp* obj) {
    obj->values[0] = 2;
//    obj->str[1] = 'a';
}



int main() {
  struct mp* mainObj = malloc(sizeof(struct mp), "mainObj");
  mainObj->str = malloc(6, "str - 1");
  mainObj->str = malloc(7, "str - 2");


  int **ptrs[4];
  ptrs[0] = malloc(10, "ptr0");
  ptrs[1] = malloc(11, "ptr1");
  ptrs[2] = malloc(12, "ptr2");
  ptrs[3] = malloc(13, "ptr3");

  unsigned i;
  klee_make_symbolic(&i, sizeof(i), "i");
  klee_assume(i < 4);

  mainObj->values = malloc(2, "concrete");
  mainObj->values = ptrs[i];

  useStruct(mainObj);
  return 0;
}
