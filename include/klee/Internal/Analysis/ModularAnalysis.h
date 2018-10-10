#ifndef KLEE_MODULARANALYSIS_H
#define KLEE_MODULARANALYSIS_H

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

#include <vector>
#include <set>


namespace klee {

struct EntryState {
  std::vector<NodeID> parameters;
};


class ModularPTA {

public:

  ModularPTA() {

  }

  ~ModularPTA() {

  }

  void computeModSet(llvm::Function *f,
                     EntryState &entryState,
                     std::set<NodeID> &result);

private:

};

}

#endif

