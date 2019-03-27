#ifndef KLEE_STATEPROJECTION_H
#define KLEE_STATEPROJECTION_H

#include <klee/Internal/Analysis/PTAUtils.h>

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>


namespace klee {

struct StateProjection {
  /* TODO: add docs */
  std::map<NodeID, PointsTo> pointsToMap;
};

class SideEffectsCollector : public klee::InstructionVisitor {

public:

  SideEffectsCollector(std::set<llvm::Function *> &called,
                       StateProjection &projection) :
    called(called),
    projection(projection) {

  }

  virtual void visitStore(PointerAnalysis *pta,
                          llvm::Function *f,
                          llvm::StoreInst *inst);

  virtual void visitLoad(PointerAnalysis *pta,
                         llvm::Function *f,
                         llvm::LoadInst *inst) {

  }

  void dump(PointerAnalysis *pta);

private:

  /* TODO: add docs */
  std::set<llvm::Function *> called;
  /* TODO: add docs */
  StateProjection &projection;
};

size_t getFlatModSize(StateProjection &projection);

}

#endif
