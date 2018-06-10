#ifndef KLEE_PTAUTILS_H
#define KLEE_PTAUTILS_H

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

namespace klee {

struct PTAStats {
    uint32_t queries;
    uint32_t total;
    uint32_t max_size;

    PTAStats() : queries(0), total(0), max_size(0) {

    }
};

void dumpNodeInfo(PointerAnalysis *pta,
                  NodeID nodeId);

void dumpPTAResults(PointerAnalysis *pta,
                    llvm::Function *entry,
                    PTAStats &state);

void dumpPTAResults(PointerAnalysis *pta,
                    PTAStats &stats);

}

#endif
