#include "klee/Internal/Analysis/StateProjection.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

using namespace llvm;
using namespace klee;


void SideEffectsCollector::visitStore(PointerAnalysis *pta,
                                      Function *f,
                                      StoreInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  for (NodeID nodeId : pts) {
    if (canEscape(pta->getPAG(), nodeId, called)) {
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

size_t klee::getFlatModSize(PointerAnalysis *pta,
                            StateProjection &projection) {
  std::set<NodeID> mod;

  for (auto i : projection.pointsToMap) {
    NodeID nodeId = i.first;
    PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
    ObjPN *obj = dyn_cast<ObjPN>(pagNode);
    if (!obj) {
      continue;
    }

    const MemObj *memObj = obj->getMemObj();
    const Value *origVal = memObj->getOrigRefVal();
    if (origVal) {
      NodeID base = pta->getPAG()->getObjectNode(origVal);
      if (isa<FIObjPN>(obj)) {
        mod.insert(pta->getFIObjNode(base));
      }
      if (isa<GepObjPN>(obj)) {
        GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
        mod.insert(pta->getGepObjNode(base, gepObj->getLocationSet()));
      }
    } else {
      mod.insert(nodeId);
    }
  }

  return mod.size();
}
