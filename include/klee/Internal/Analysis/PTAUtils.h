#ifndef KLEE_PTAUTILS_H
#define KLEE_PTAUTILS_H

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

namespace klee {

void dumpNodeInfo(PointerAnalysis *pta, NodeID nodeId);

void dumpPTAResults(PointerAnalysis *pta);

void dumpPTAResults(PointerAnalysis *pta,
                    llvm::Function *f);

}

#endif
