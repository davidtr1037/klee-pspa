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
  SubstitutionInfo info;
  FunctionCache &fcache = cache[f];
  for (ModResult &modResult : fcache) {
    if (checkIsomorphism(entryState, modResult.entryState, info)) {
      result = modResult.mod;
      return true;
    }
  }

  return false;
}

bool ModularPTA::checkIsomorphism(EntryState &es1,
                                  EntryState &es2,
                                  SubstitutionInfo &info) {
  if (es1.parameters.size() != es2.parameters.size()) {
    return false;
  }

  for (unsigned int i = 0; i < es1.parameters.size(); i++) {
    Parameter p1 = es1.parameters[i];
    Parameter p2 = es2.parameters[i];
    if (p1.index != p2.index) {
      return false;
    }

    if (!checkIsomorphism(es1, p1.nodeId, es2, p2.nodeId, info)) {
      return false;
    }
  }

  return true;
}

bool ModularPTA::checkIsomorphism(EntryState &es1,
                                  NodeID n1,
                                  EntryState &es2,
                                  NodeID n2,
                                  SubstitutionInfo &info) {
  PointsTo &pts1 = es1.pta->getPts(n1);
  PointsTo &pts2 = es1.pta->getPts(n2);

  if (pts1.count() != pts2.count()) {
    return false;
  }

  if (pts1.count() > 1) {
    assert(false);
  }

  bool equal = false;
  if (pts1.count() == 0) {
      equal = true;
  } else {
      NodeID p1 = *pts1.begin();
      NodeID p2 = *pts1.begin();
      equal = checkIsomorphism(es1, p1, es2, p2, info);
  }

  if (equal) {
    NodeID base1 = es1.pta->getBaseObjNode(n1);
    NodeID base2 = es2.pta->getBaseObjNode(n2);
    info.mapping[base1] = base2;
  }

  return equal;
}
