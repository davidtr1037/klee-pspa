#include "klee/Internal/Analysis/ModularAnalysis.h"
#include "klee/Internal/Analysis/PTAUtils.h"

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
    if (checkIsomorphism(modResult.entryState, entryState, info)) {
      substitute(modResult.entryState, entryState, modResult.mod, info, result);
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
  /* TODO: check all sub-nodes */

  PAGNode *pagNode;
  pagNode = es1.pta->getPAG()->getPAGNode(n1);
  ObjPN *obj1 = dyn_cast<ObjPN>(pagNode);
  pagNode = es2.pta->getPAG()->getPAGNode(n2);
  ObjPN *obj2 = dyn_cast<ObjPN>(pagNode);

  if (isa<FIObjPN>(obj1)) {
    if (!isa<FIObjPN>(obj2)) {
      return false;
    }
  }

  if (isa<GepObjPN>(obj1)) {
    GepObjPN *gep1 = dyn_cast<GepObjPN>(obj1);
    GepObjPN *gep2 = dyn_cast<GepObjPN>(obj2);
    if (!gep2) {
      return false;
    }

    if (gep1->getLocationSet().getOffset() != gep2->getLocationSet().getOffset()) {
      return false;
    }
  }

  PointsTo &pts1 = es1.pta->getPts(n1);
  PointsTo &pts2 = es2.pta->getPts(n2);

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
      NodeID p2 = *pts2.begin();
      equal = checkIsomorphism(es1, p1, es2, p2, info);
  }

  if (equal) {
    NodeID base1 = es1.pta->getBaseObjNode(n1);
    NodeID base2 = es2.pta->getBaseObjNode(n2);
    info.mapping[base1] = base2;
  }

  return equal;
}

void ModularPTA::substitute(EntryState &es1,
                            EntryState &es2,
                            std::set<NodeID> &cachedMod,
                            SubstitutionInfo &info,
                            std::set<NodeID> &result) {
  for (NodeID nodeId : cachedMod) {
    NodeID base1 = es1.pta->getBaseObjNode(nodeId);
    if (info.mapping.find(base1) == info.mapping.end()) {
      /* TODO: is it always correct? */
      result.insert(nodeId);
      continue;
    }

    /* get corresponding node */
    NodeID base2 = info.mapping[base1];

    PAGNode *pagNode = es1.pta->getPAG()->getPAGNode(nodeId);
    ObjPN *obj = dyn_cast<ObjPN>(pagNode);
    if (!obj) {
      assert(false);
    }

    if (isa<FIObjPN>(obj)) {
      result.insert(es2.pta->getFIObjNode(base2));
    }

    if (isa<GepObjPN>(obj)) {
      GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
      result.insert(es2.pta->getGepObjNode(base2, gepObj->getLocationSet()));
    }
  }
}
