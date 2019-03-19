#include "klee/Internal/Analysis/Colours.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/DebugInfo.h>

using namespace llvm;
using namespace klee;


void ColourCollector::visitStore(PointerAnalysis *pta,
                                 Function *f,
                                 StoreInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo pts;
  for (NodeID n : pta->getPts(id)) {
    pts.set(stripUniqueAS(pta, n));
  }
  ptsSets.push_back(pts);
}

bool ColourCollector::intersects(PointerAnalysis* pta,
                                 PointsTo &pts1,
                                 PointsTo &pts2) {
  for (NodeID n1 : pts1) {
    for (NodeID n2 : pts2) {
      PAGNode *pagNode;
      pagNode = pta->getPAG()->getPAGNode(n1);
      ObjPN *obj1 = dyn_cast<ObjPN>(pagNode);
      pagNode = pta->getPAG()->getPAGNode(n2);
      ObjPN *obj2 = dyn_cast<ObjPN>(pagNode);
      bool fiCheck = isa<FIObjPN>(obj1) || isa<FIObjPN>(obj2);
      if (fiCheck) {
        if (obj1->getMemObj()->getSymId() == obj2->getMemObj()->getSymId()) {
          return true;
        }
      } else {
        if (n1 == n2) {
          return true;
        }
      }
    }
  }

  return false;
}

void ColourCollector::computeColours(PointerAnalysis* pta) {
  int changes = 0;

  do {
    changes = 0;
    for (auto i = ptsSets.begin(); i != ptsSets.end(); i++) {
      for (auto j = i + 1; j != ptsSets.end(); ) {
        PointsTo &pts1 = *i;
        PointsTo &pts2 = *j;
        if (intersects(pta, pts1, pts2)) {
          changes++;
          pts1 |= pts2;
          j = ptsSets.erase(j);
        } else {
          j++;
        }
      }
    }
    errs() << "Completed loop with " << changes << " changes\n";
  } while(changes > 0);

  errs() << "There are " << ptsSets.size() << " colours\n";
}
