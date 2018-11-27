// RUN: %llvmgcc -I../../../include -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf %t.klee-outViaStruct %t.klee-outuseM
// RUN: %klee -collect-pta-results -collect-modref  -pta-target=useMatrixViaStruct --output-dir=%t.klee-outViaStruct %t.bc &> %tViaStruct.log 
// RUN: grep 'AS: ' %tViaStruct.log | wc -l | grep 2
// RUN: grep '\[6 x i8\] c"haha' %tViaStruct.log 
// RUN: grep 'malloc(i64 56, i8*' %tViaStruct.log 

#include <stdio.h>

void* malloc(size_t, char*);

struct mp {
    char *str;
    short y;
    int *values;
    int values_size;
    short* matrix[3];

};

void useMatrixViaStruct(struct mp* p) {
    p->values_size = 1;
    p->str[3] = 'g';
}

char str0[6] = {'h','a','h','a','\0'};

int main() {
  struct mp* mainObj = malloc(sizeof(struct mp), "mainObj");
  mainObj->str = "HAHHAHA";
  mainObj->str = str0;
  mainObj->values = malloc(10, "mainObj.values");
  mainObj->values_size = 10;

  mainObj->matrix[0] = malloc(6, "mainObj.matrix[0]");
  mainObj->matrix[1] = malloc(7, "mainObj.matrix[1]");
  mainObj->matrix[2] = malloc(8, "mainObj.matrix[2]");
  mainObj->matrix[2] = malloc(9, "mainObj.matrix[2.1]");

  useMatrixViaStruct(mainObj);

  return 0;
}
