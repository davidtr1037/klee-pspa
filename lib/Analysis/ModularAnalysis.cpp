#include "klee/Internal/Analysis/ModularAnalysis.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

using namespace llvm;
using namespace klee;
using namespace std;


void klee::computeModSet(EntryState &entryState,
                         set<NodeID> &result) {
  assert(!entryState.parameters.empty());

  if (entryState.f->getName() == "osip_clrncpy") {
    /* modifies only the first parameter */
    result.insert(entryState.parameters[0]);
  }
  assert(false);
}
