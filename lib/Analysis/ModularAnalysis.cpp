#include "klee/Internal/Analysis/ModularAnalysis.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

using namespace llvm;
using namespace klee;
using namespace std;


void ModularPTA::update(Function *f,
                        EntryState &entryState,
                        set<NodeID> &mod) {
  FunctionCache &fcache = cache[f];
  ModResult modResult;
  modResult.entryState = entryState;
  modResult.mod = mod;

  fcache.push_back(modResult);
}

bool ModularPTA::computeModSet(Function *f,
                               EntryState &entryState,
                               set<NodeID> &result) {
  FunctionCache &fcache = cache[f];
  for (ModResult &modResult : fcache) {
    if (checkIsomorphism(entryState, modResult.entryState)) {
      result = modResult.mod;
      return true;
    }
  }

  return false;
}

bool ModularPTA::checkIsomorphism(EntryState &es1,
                                  EntryState &es2) {
  return true;
}
