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
                        NodeID nodeId,
                        unsigned int level) {
  std::string prefix = std::string(level, ' ');

  errs() << prefix << "-- node: " << nodeId << "\n";
  if (!pta->getPAG()->findPAGNode(nodeId)) {
    /* probably was deallocated (unique AS) */
    errs() << prefix << "   -- not found\n";
    return;
  }

  PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
  ObjPN *obj = dyn_cast<ObjPN>(pagNode);
  if (obj) {
    const Value *value = obj->getMemObj()->getRefVal();
    if (!value) {
      return;
    }

    errs() << prefix << "-- value: " << value << "\n";
    if (isa<Function>(value)) {
      const Function *f = dyn_cast<Function>(value);
      errs() << prefix << "-- AS: " << f->getName() << "\n";
    } else {
      errs() << prefix << "-- AS: " << *value << "\n";
    }
    errs() << prefix << "   -- base: " << pta->getBaseObjNode(obj->getId()) << "\n";
    errs() << prefix << "   -- kind: " << obj->getNodeKind() << "\n";
    GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
    if (gepObj) {
       errs() << prefix << "   -- ls: " << gepObj->getLocationSet().getOffset() << "\n";
    }
  }
}

bool klee::canEscape(PAG *pag,
                     NodeID nodeId,
                     std::set<Function *> called) {
  if (!pag->findPAGNode(nodeId)) {
    /* probably was deallocated (unique AS) */
    return false;
  }

  PAGNode *pagNode = pag->getPAGNode(nodeId);
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

NodeID klee::stripUniqueAS(NodeID nodeId) {
  PAGNode *pagNode = PAG::getPAG()->getPAGNode(nodeId);
  ObjPN *obj = dyn_cast<ObjPN>(pagNode);
  if (!obj) {
    return nodeId;
  }

  const MemObj *memObj = obj->getMemObj();
  const Value *origVal = memObj->getOrigRefVal();
  if (origVal) {
    NodeID base = PAG::getPAG()->getObjectNode(origVal);
    if (isa<FIObjPN>(obj)) {
      return PAG::getPAG()->getFIObjNode(base);
    }

    if (isa<GepObjPN>(obj)) {
      GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
      return PAG::getPAG()->getGepObjNode(base, gepObj->getLocationSet());
    }

    /* TODO: should not happen... */
    assert(false);

  } else {
    return nodeId;
  }
}

void InstructionVisitor::visitFunction(PointerAnalysis *pta, Function *f) {
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

void InstructionVisitor::visitReachable(PointerAnalysis *pta,
                                        Function *entry) {
  /* compute reachable functions (without resolving) */
  FunctionSet functions;
  computeReachableFunctions(entry, pta, functions);

  for (Function *f : functions) {
    visitFunction(pta, f);
  }
}

void InstructionVisitor::visitAll(PointerAnalysis *pta) {
  Module &module = *pta->getModule();
  for (Function &f : module) {
    visitFunction(pta, &f);
  }
}

void StatsCollector::visitStore(PointerAnalysis *pta,
                                Function *f,
                                StoreInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  if (dump) {
    errs() << f->getName() << ": "  << *inst << ":\n";
  }
  for (NodeID nodeId : pts) {
    mod.insert(nodeId);
    if (dump) {
      dumpNodeInfo(pta, nodeId);
    }
  }

  updateStats(id, pts.count());
}

void StatsCollector::visitLoad(PointerAnalysis *pta,
                               Function *f,
                               LoadInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  if (dump) {
    errs() << f->getName() << ": "  << *inst << ":\n";
  }
  for (NodeID nodeId : pts) {
    ref.insert(nodeId);
    if (dump) {
      dumpNodeInfo(pta, nodeId);
    }
  }

  updateStats(id, pts.count());
}

void StatsCollector::updateStats(NodeID nodeId, unsigned size) {
  if (visited.find(nodeId) != visited.end()) {
    return;
  }

  stats.queries += 1;
  stats.total += size;
  if (size > stats.max_size) {
    stats.max_size = size;
  }

  stats.mod_size = mod.size();
  stats.ref_size = ref.size();

  visited.insert(nodeId);
}

void ResultsCollector::visitReachable(PointerAnalysis *pta,
                                      llvm::Function *entry) {
  log << "Entry: " << entry->getName() << "\n";
  InstructionVisitor::visitReachable(pta, entry);
}

void ResultsCollector::visitStore(PointerAnalysis *pta,
                                  Function *f,
                                  StoreInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  log << "[S] " << f->getName() << " " << id << ": { ";
  for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
    NodeID nodeId = *i;
    log << nodeId << " ";
  }
  log << "}\n";
}

void ResultsCollector::visitLoad(PointerAnalysis *pta,
                                 Function *f,
                                 LoadInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  log << "[L] " << f->getName() << " " << id << ": { ";
  for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
    NodeID nodeId = *i;
    log << nodeId << " ";
  }
  log << "}\n";
}

void ModRefCollector::visitStore(PointerAnalysis *pta,
                                 Function *f,
                                 StoreInst *inst) {
  if (!collectMod) {
    return;
  }

  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  for (NodeID nodeId : pts) {
    if (canEscape(pta->getPAG(), nodeId, called)) {
      continue;
    }
    if (nodeId == pta->getPAG()->getConstantNode()) {
      continue;
    }
    mod.insert(nodeId);
  }
}

void ModRefCollector::visitLoad(PointerAnalysis *pta,
                                Function *f,
                                LoadInst *inst) {
  if (!collectRef) {
    return;
  }

  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo &pts = pta->getPts(id);

  for (NodeID nodeId : pts) {
    if (canEscape(pta->getPAG(), nodeId, called)) {
      continue;
    }
    ref.insert(nodeId);
  }
}

void ModRefCollector::dumpModSet(PointerAnalysis *pta) {
  errs() << "--- Mod Set ---\n";
  for (NodeID nodeId : mod) {
    dumpNodeInfo(pta, nodeId);
  }
}

void ModRefCollector::dumpRefSet(PointerAnalysis *pta) {
  errs() << "--- Ref Set ---\n";
  for (NodeID nodeId : ref) {
    dumpNodeInfo(pta, nodeId);
  }
}
