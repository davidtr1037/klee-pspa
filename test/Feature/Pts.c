// RUN: %llvmgcc -I../../../include -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee  -collect-modref  -pta-target=useMatrix --output-dir=%t.klee-out --write-kqueries %t.bc > %t.log ; false

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
    matrix[1][2] = 2;
}

int main() {
  struct mp* mainObj = malloc(sizeof(struct mp), "mainObj");
  mainObj->str = "HAHHAHAHA";
  mainObj->values = malloc(10, "mainObj.values");
  mainObj->values_size = 10;

  mainObj->matrix[0] = malloc(5, "mainObj.matrix[0]");
  mainObj->matrix[1] = malloc(5, "mainObj.matrix[1]");
  mainObj->matrix[2] = malloc(5, "mainObj.matrix[2]");

  
  int **matrix = malloc(sizeof(int*)*5, "matrix");
  useMatrix(mainObj->matrix);

//  klee_print_pts(mainObj);
//  klee_print_pts(matrix);


  return 0;
}
