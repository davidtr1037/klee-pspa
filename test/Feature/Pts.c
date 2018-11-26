// RUN: %llvmgcc -I../../../include -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee -collect-pta-results -collect-modref  -pta-target=useMatrixViaStruct,useM --output-dir=%t.klee-out --write-kqueries %t.bc > %t.log ; false

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
  klee_print_pts(mainObj);
 // klee_print_pts(mainObj->matrix[0]);
//  klee_print_pts(matrix);
 // useMatrix(mainObj->matrix);
//  useMatrixViaStruct(mainObj);
  //  klee_print_pts(mainObj);
//  klee_print_pts(matrix);



  return 0;
}
