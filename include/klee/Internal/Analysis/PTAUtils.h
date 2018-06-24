#ifndef KLEE_PTAUTILS_H
#define KLEE_PTAUTILS_H

#include <klee/Internal/Analysis/PTAStats.h>

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

namespace klee {

void dumpNodeInfo(PointerAnalysis *pta,
                  NodeID nodeId);

void evaluatePTAResults(PointerAnalysis *pta,
                        llvm::Function *entry,
                        PTAStats &state,
                        bool dump = true);

void evaluatePTAResults(PointerAnalysis *pta,
                        PTAStats &stats,
                        bool dump = true);

}

#endif
