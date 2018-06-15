#include "klee/Internal/Analysis/Reachability.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>

#include <stack>

using namespace llvm;
using namespace klee;
using namespace std;


static Function *extractFunction(ConstantExpr *ce) {
  if (!ce->isCast()) {
    return NULL;
  }

  Value *value = ce->getOperand(0);
  if (isa<Function>(value)) {
    return dyn_cast<Function>(value);
  }

  if (isa<GlobalAlias>(value)) {
    Constant *aliasee = dyn_cast<GlobalAlias>(value)->getAliasee();
    if (isa<Function>(aliasee)) {
      return dyn_cast<Function>(aliasee);
    }
    if (isa<ConstantExpr>(aliasee)) {
      return extractFunction(dyn_cast<ConstantExpr>(aliasee));
    }
  }

  return NULL;
}

static void getCallTargets(CallInst *callInst,
                           PointerAnalysis *pta,
                           FunctionSet &targets) {
  Function *calledFunction = callInst->getCalledFunction();
  Value *calledValue = callInst->getCalledValue();

  if (!calledFunction) {
    /* the called value should be one of these: function pointer, cast, alias */
    if (isa<ConstantExpr>(calledValue)) {
      Function *extracted = extractFunction(dyn_cast<ConstantExpr>(calledValue));
      if (!extracted) {
        /* TODO: unexpected value... */
        assert(false);
      }
      calledFunction = extracted;
    }
  }

  if (calledFunction == NULL) {
    if (!pta) {
      /* function pointers will not be handled... */
      return;
    }

    /* the called value should be a function pointer */
    CallSite cs(callInst);
    PointerAnalysis::CallEdgeMap::iterator i = pta->getIndCallMap().find(cs);
    if (i == pta->getIndCallMap().end()) {
      return;
    }

    for (const Function *f : i->second) {
      targets.insert((Function *)(f));
    }

  } else {
    targets.insert(calledFunction);
  }
}

void klee::computeReachableFunctions(Function *entry,
                                     PointerAnalysis *pta,
                                     FunctionSet &results) {
  stack<Function *> worklist;
  FunctionSet pushed;

  worklist.push(entry);
  pushed.insert(entry);
  results.insert(entry);

  while (!worklist.empty()) {
    Function *f = worklist.top();
    worklist.pop();

    for (inst_iterator iter = inst_begin(f); iter != inst_end(f); iter++) {
      Instruction *inst = &*iter;
      if (inst->getOpcode() != Instruction::Call) {
        continue;
      }

      FunctionSet targets;
      CallInst *callInst = dyn_cast<CallInst>(inst);
      getCallTargets(callInst, pta, targets);

      for (Function *target : targets) {
        results.insert(target);
        if (target->isDeclaration()) {
          continue;
        }

        if (pushed.find(target) == pushed.end()) {
          worklist.push(target);
          pushed.insert(target);
        }
      }
    }
  }
}
