#include "klee/Internal/Analysis/PTAUtils.h"
#include "klee/Internal/Analysis/Reachability.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/InstIterator.h>

using namespace llvm;
using namespace klee;


void klee::dumpNodeInfo(PointerAnalysis *pta,
                        NodeID nodeId) {
    errs() << "-- node: " << nodeId << "\n";
    PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
    ObjPN *obj = dyn_cast<ObjPN>(pagNode);
    if (obj) {
        const Value *value = obj->getMemObj()->getRefVal();
        if (!value) {
            return;
        }

        errs() << "-- AS: " << *value << "\n";
        errs() << "   -- kind: " << obj->getNodeKind() << "\n";
        GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
        if (gepObj) {
            errs() << "   -- ls: " << gepObj->getLocationSet().getOffset() << "\n";
        }
    }
}

static unsigned visitStore(PointerAnalysis *pta,
                           Function *f,
                           StoreInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  errs() << f->getName() << ": "  << *inst << ":\n";
  for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
    NodeID nodeId = *i;
    dumpNodeInfo(pta, nodeId);
  }

  return pts.count();
}

static unsigned visitLoad(PointerAnalysis *pta,
                          Function *f,
                          LoadInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  //errs() << f->getName() << ": "  << *inst << ":\n";
  for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
    NodeID nodeId = *i;
    //dumpNodeInfo(pta, nodeId);
  }

  return pts.count();
}

static void updatePTAStats(PTAStats &stats, unsigned size) {
  stats.queries += 1;
  stats.total += size;
  if (size > stats.max_size) {
    stats.max_size = size;
  }
}

void dumpPTA(PointerAnalysis *pta,
             Function *f,
             PTAStats &stats) {
  unsigned pts_size;

  for (inst_iterator j = inst_begin(f); j != inst_end(f); j++) {
    Instruction *inst = &*j;
    if (inst->getOpcode() == Instruction::Store) {
      pts_size = visitStore(pta, f, dyn_cast<StoreInst>(inst));
      updatePTAStats(stats, pts_size);
    }
    if (inst->getOpcode() == Instruction::Load) {
      pts_size = visitLoad(pta, f, dyn_cast<LoadInst>(inst));
      updatePTAStats(stats, pts_size);
    }
  }
}

void klee::dumpPTAResults(PointerAnalysis *pta,
                          Function *entry,
                          PTAStats &stats) {
  /* compute reachable functions (without resolving) */
  FunctionSet functions;
  computeReachableFunctions(entry, functions);

  for (Function *f : functions) {
    dumpPTA(pta, f, stats);
  }
}

void klee::dumpPTAResults(PointerAnalysis *pta, PTAStats &stats) {
  Module &module = *pta->getModule();
  for (Function &f : module) {
    dumpPTAResults(pta, &f, stats);
  }
}
