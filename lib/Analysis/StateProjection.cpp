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

size_t klee::getFlatModSize(StateProjection &projection) {
  std::set<NodeID> mod;

  for (auto i : projection.pointsToMap) {
    NodeID nodeId = stripUniqueAS(i.first);
    mod.insert(nodeId);
  }

  return mod.size();
}
