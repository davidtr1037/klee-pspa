#ifndef KLEE_REACHABILITY_H
#define KLEE_REACHABILITY_H

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

#include <set>

namespace klee {

typedef std::set<llvm::Function *> FunctionSet;

void computeReachableFunctions(llvm::Function *entry,
                               PointerAnalysis *pta,
                               FunctionSet &results);

};

#endif
