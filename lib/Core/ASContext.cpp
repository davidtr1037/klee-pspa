#include "klee/ASContext.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Internal/Support/Debug.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include <vector>

using namespace llvm;
using namespace klee;

ASContext::ASContext(std::vector<Instruction *> &callTrace,
                     Instruction *allocInst) {
  refCount = 0;

  for (Instruction *inst : callTrace) {
    trace.push_back(inst);
  }

  trace.push_back(allocInst);
}

ASContext::ASContext(ASContext &other) :
  refCount(0),
  trace(other.trace)    
{

}

void ASContext::dump() {
  DEBUG_WITH_TYPE(DEBUG_BASIC, klee_message("allocation site context:"));
  for (Instruction *inst : trace) {
    Function *f = inst->getParent()->getParent();
    DEBUG_WITH_TYPE(DEBUG_BASIC, errs() << "  -- " << f->getName() << ":");
    DEBUG_WITH_TYPE(DEBUG_BASIC, inst->dump());
  }
}

bool ASContext::operator==(ASContext &other) {
  return trace == other.trace;
}

bool ASContext::operator!=(ASContext &other) {
  return !(*this == other);
}
