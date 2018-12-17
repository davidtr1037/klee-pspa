// RUN: %llvmgcc -I../../../include -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf %t.klee-outViaStruct
// RUN: %klee -collect-pta-results -collect-pta-stats -collect-modref  -pta-target=useMatrix --output-dir=%t.klee-outViaStruct -use-pta-mode=symbolic %t.bc &> %tViaStruct.log 
// RUN: grep '<badref> = call' %tViaStruct.log | wc -l | grep 3
// RUN: grep '<badref> = call' %tViaStruct.log | sed -n 's/.*i64 \([0-9]\).*/\1/p' | tr '\n' '-' | grep 6-7-9
#include <stdio.h>

void* malloc(size_t, char*);

struct mp {
    char *str;
    short y;
    int *values;
    int values_size;
    short* matrix[3];

};


void (*modFunc_ptr)(struct mp*);

void useMatrix(struct mp* p) {
    (*modFunc_ptr)(p);

}

void useMatrixViaStruct(struct mp* p) {
    p->matrix[0][1] = 3;
}
void useMatrixViaStruct1(struct mp* p) {
}
void useMatrixViaStruct2(struct mp* p)  {
}


int main() {
  struct mp* mainObj = malloc(sizeof(struct mp), "mainObj");
  mainObj->str = "HAHHAHAHA";
  mainObj->values = malloc(10, "mainObj.values");
  mainObj->values_size = 10;

  mainObj->matrix[0] = malloc(6, "mainObj.matrix[0]");
  mainObj->matrix[1] = malloc(7, "mainObj.matrix[1]");
  mainObj->matrix[2] = malloc(8, "mainObj.matrix[2]");
  mainObj->matrix[2] = malloc(9, "mainObj.matrix[2.1]");
  
  modFunc_ptr = &useMatrixViaStruct1;
  modFunc_ptr = &useMatrixViaStruct2;
  modFunc_ptr = &useMatrixViaStruct;

  useMatrix(mainObj);

  return 0;
}
