#include "klee/Internal/Analysis/PTAGraph.h"
#include "klee/Internal/Analysis/PTAUtils.h"
#include "klee/Internal/Analysis/Reachability.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/InstIterator.h>
#include "llvm/IR/DebugInfo.h"

using namespace llvm;
using namespace klee;
using namespace std;

static void getInstructionDebugInfo(Instruction *inst,
                                    string &file,
                                    unsigned int &line) {
  errs() << "DBG: " << *inst << "\n";
  MDNode *n = inst->getMetadata("dbg");
  if (n) {
    DILocation *loc = cast<DILocation>(n);
    file = loc->getFilename();
    line = loc->getLine();
  }
}

void BasePTAGraphDumper::dump(AndersenDynamic *pta) {
  auto ptsMap = pta->getPtsMap();
  for (auto i : ptsMap) {
    NodeID nodeId = i.first;
    PointsTo &pts = pta->getPts(nodeId);
    handle(pta, nodeId, pts);
  }
}

void BasePTAGraphDumper::handle(AndersenDynamic *pta,
                                NodeID src,
                                PointsTo &pts) {
  // ...
}

void PTAGraphDumper::dump(AndersenDynamic *pta) {
  log << "----\n";
  BasePTAGraphDumper::dump(pta);
}

void PTAGraphDumper::handle(AndersenDynamic *pta,
                            NodeID src,
                            PointsTo &pts) {
  log << "From: " << src << "\n";
  dumpDebugInfo(pta, src);
  for (NodeID nodeId : pts) {
    log << "To: " << nodeId << "\n";
    dumpDebugInfo(pta, nodeId);
  }
}

void PTAGraphDumper::dumpDebugInfo(AndersenDynamic *pta,
                                   NodeID nodeId) {
  PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
  ObjPN *obj = dyn_cast<ObjPN>(pagNode);
  if (obj) {
    dumpObjDebugInfo(pta, obj);
    return;
  }

  ValPN *val = dyn_cast<ValPN>(pagNode);
  if (val) {
    dumpValDebugInfo(pta, val);
    return;
  }
}

void PTAGraphDumper::dumpObjDebugInfo(AndersenDynamic *pta,
                                      ObjPN *obj) {
  const Value *value = obj->getMemObj()->getRefVal();
  if (!value) {
    return;
  }

  if (isa<Function>(value)) {
    const Function *f = dyn_cast<Function>(value);
    log << "AS: " << f->getName() << " (function)\n";
    return;
  }

  log << "AS: " << value->getName() << "\n";
  GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
  if (gepObj) {
    log << "offset: " << gepObj->getLocationSet().getOffset() << "\n";
  } else {
    log << "offset: field-insensitive\n";
  }

  if (isa<Instruction>(value)) {
    Instruction *inst = (Instruction *)(value);
    string filename;
    unsigned int line = 0;
    getInstructionDebugInfo(inst, filename, line);

    if (inst->getParent()) {
      log << "function: " << inst->getParent()->getParent()->getName() << "\n";
    }
    if (filename != "") {
      log << "file: " << filename << "\n";
    }
    if (line != 0) {
      log << "line: " << line << "\n";
    }
  }
}

void PTAGraphDumper::dumpValDebugInfo(AndersenDynamic *pta,
                                      ValPN *val) {
  if (!val->hasValue()) {
    log << "Dummy:\n";
    return;
  }

  const Value *value = val->getValue();
  log << "Pointer: " << val->getValueName() << "\n";
  if (isa<Instruction>(value)) {
    Instruction *inst = (Instruction *)(value);
    string filename;
    unsigned int line = 0;
    getInstructionDebugInfo(inst, filename, line);

    log << "function: " << inst->getParent()->getParent()->getName() << "\n";
    if (filename != "") {
      log << "file: " << filename << "\n";
    }
    if (line != 0) {
      log << "line: " << line << "\n";
    }
  }
}
