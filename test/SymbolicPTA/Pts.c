// RUN: %llvmgcc -I../../../include -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf %t.klee-outViaStructSanity %t.klee-outViaStruct %t.klee-outuseM %t.klee-outuseMSanity
// RUN: %klee -collect-pta-results -collect-modref  -pta-target=useMatrixViaStruct --output-dir=%t.klee-outViaStruct -sym-pta %t.bc &> %tViaStruct.log 
// RUN: grep '<badref> = call' %tViaStruct.log | wc -l | grep 3
// RUN: grep '<badref> = call' %tViaStruct.log | sed -n 's/.*i64 \([0-9]\).*/\1/p' | tr '\n' '-' | grep 6-7-9

// RUN: %klee -collect-pta-results -collect-modref  -pta-target=useM --output-dir=%t.klee-outuseM -sym-pta %t.bc &> %tuseM.log 
// RUN: grep '<badref> = call' %tuseM.log | wc -l | grep 5
// RUN: grep '<badref> = call' %tuseM.log | sed -n 's/.*i64 \([0-9][0-9]\).*/\1/p' | tr '\n' '-' | grep 12-16-20-32-28

// RUN: %klee -collect-pta-results -collect-modref  -pta-target=useM --output-dir=%t.klee-outuseMSanity -sym-pta-sanity %t.bc 
// RUN: %klee -collect-pta-results -collect-modref  -pta-target=useMatrixViaStruct --output-dir=%t.klee-outViaStructSanity -sym-pta-sanity %t.bc
#include <stdio.h>

void* malloc(size_t, char*);

struct mp {
    char *str;
    short y;
    int *values;
    int values_size;
    short* matrix[3];

};


void useMatrix(short **matrix) {
    matrix[0][2] = 2;
}

void useMatrixViaStruct(struct mp* p) {
    p->matrix[0][1] = 3;
}

void useM(int **m) {
    m[3][3] = 3;
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

   int **matrix = malloc(sizeof(int*)*5, "matrix");
  matrix[0] = malloc(sizeof(int)*3, "matrix[0]");
  matrix[1] = malloc(sizeof(int)*4, "matrix[1]");
  matrix[2] = malloc(sizeof(int)*5, "matrix[2]");
  matrix[3] = malloc(sizeof(int)*6, "matrix[3]");
  matrix[3] = malloc(sizeof(int)*8, "matrix[3.1]");
  matrix[4] = malloc(sizeof(int)*7, "matrix[4]");
  useMatrixViaStruct(mainObj);
  useM(matrix);

  return 0;
}
