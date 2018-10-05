#ifndef KLEE_MODULARANALYSIS_H
#define KLEE_MODULARANALYSIS_H

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

#include <vector>
#include <set>


namespace klee {

struct EntryState {
  llvm::Function *f;
  std::vector<NodeID> parameters;
};

void computeModSet(EntryState &entryState,
                   std::set<NodeID> &result);

}

#endif

