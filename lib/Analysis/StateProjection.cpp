#include "klee/Internal/Analysis/StateProjection.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

using namespace llvm;
using namespace klee;


bool SideEffectsCollector::canEscape(PointerAnalysis *pta,
                                     NodeID nodeId) {
  if (!pta->getPAG()->findPAGNode(nodeId)) {
    /* probably was deallocated (unique AS) */
    return false;
  }

  PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
  ObjPN *obj = dyn_cast<ObjPN>(pagNode);
  if (obj) {
    const Value *value = obj->getMemObj()->getRefVal();
    if (value && isa<AllocaInst>(value)) {
      AllocaInst *alloca = (AllocaInst *)(dyn_cast<AllocaInst>(value));
      Function *src = alloca->getParent()->getParent();
      if (called.find(src) == called.end()) {
        return true;
      }
    }
  }

  return false;
}

void SideEffectsCollector::visitStore(PointerAnalysis *pta,
                                      Function *f,
                                      StoreInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  for (NodeID nodeId : pts) {
    if (canEscape(pta, nodeId)) {
      continue;
    }
    if (nodeId == pta->getPAG()->getConstantNode()) {
      continue;
    }

    NodeID value = pta->getPAG()->getValueNode(inst->getValueOperand());
    projection.pointsToMap[nodeId] = pta->getPts(value);
  }
}

void SideEffectsCollector::dump(PointerAnalysis *pta) {
  for (auto i : projection.pointsToMap) {
    NodeID src = i.first;
    errs() << src << ": { ";
    PointsTo &pts = i.second;
    for (NodeID n : pts) {
      errs() << n << " ";
    }
    errs() << "}\n";
  }
}
