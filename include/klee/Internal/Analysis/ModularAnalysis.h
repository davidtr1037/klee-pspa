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

struct ModResult {
  EntryState entryState;
  std::set<NodeID> mod;
};

class ModularPTA {

public:

  ModularPTA() {

  }

  ~ModularPTA() {

  }

  void update(llvm::Function *f,
              EntryState &entryState,
              std::set<NodeID> &mod);

  bool computeModSet(llvm::Function *f,
                     EntryState &entryState,
                     std::set<NodeID> &result);

  bool checkIsomorphism(EntryState &es1,
                        EntryState &es2);

private:

  typedef std::vector<ModResult> FunctionCache;
  typedef std::map<llvm::Function *, FunctionCache> Cache;

  Cache cache;
};

}

#endif

