#include "klee/Internal/Analysis/PTAUtils.h"

#include <MemoryModel/PointerAnalysis.h>
#include <WPA/Andersen.h>
#include <WPA/DynamicAndersenBase.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/InstIterator.h>

using namespace llvm;
using namespace klee;


void klee::dumpNodeInfo(PointerAnalysis *pta, NodeID nodeId) {
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

static void visitStore(PointerAnalysis *pta, Function *f, StoreInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  errs() << f->getName() << ": "  << *inst << ":\n";
  for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
    NodeID nodeId = *i;
    dumpNodeInfo(pta, nodeId);
  }
}

static void visitLoad(PointerAnalysis *pta, Function *f, LoadInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  errs() << f->getName() << ": "  << *inst << ":\n";
  for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
    NodeID nodeId = *i;
    dumpNodeInfo(pta, nodeId);
  }
}

void klee::dumpPTAResults(PointerAnalysis *pta,
                          Function *f) {
  for (inst_iterator j = inst_begin(f); j != inst_end(f); j++) {
    Instruction *inst = &*j;
    if (inst->getOpcode() == Instruction::Store) {
      visitStore(pta, f, dyn_cast<StoreInst>(inst));
    }
    if (inst->getOpcode() == Instruction::Load) {
      visitLoad(pta, f, dyn_cast<LoadInst>(inst));
    }
  }
}

void klee::dumpPTAResults(PointerAnalysis *pta) {
  Module &module = *pta->getModule();
  for (Function &f : module) {
    dumpPTAResults(pta, &f);
  }
}
