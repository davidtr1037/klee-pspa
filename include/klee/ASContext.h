#ifndef KLEE_ASCONTEXT_H
#define KLEE_ASCONTEXT_H

#include "llvm/IR/Instruction.h"

#include <vector>

namespace klee {

class ExecutionState;

class ASContext {

public:

  ASContext() : refCount(0) {

  }

  ASContext(std::vector<llvm::Instruction *> &callTrace,
            llvm::Instruction *inst);

  ASContext(ASContext &other);

  bool operator==(ASContext &other);

  bool operator!=(ASContext &other);

  void dump();

  unsigned int refCount;

private:

  std::vector<llvm::Instruction *> trace;

};

}

#endif
