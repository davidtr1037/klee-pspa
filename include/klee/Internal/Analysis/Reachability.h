#ifndef KLEE_REACHABILITY_H
#define KLEE_REACHABILITY_H

#include <llvm/IR/Function.h>

#include <set>

namespace klee {

typedef std::set<const llvm::Function *> FunctionSet;

void computeReachableFunctions(const llvm::Function *entry,
                               FunctionSet &results);

};

#endif
