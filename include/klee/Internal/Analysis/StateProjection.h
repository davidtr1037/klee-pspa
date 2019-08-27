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

  void dump() const;
};

class StateProjectionCollector : public klee::InstructionVisitor {

public:

  StateProjectionCollector(std::set<llvm::Function *> &called,
                           StateProjection &projection,
                           bool collectMod) :
    called(called),
    projection(projection),
    collectMod(collectMod) {

  }

  virtual void visitStore(PointerAnalysis *pta,
                          llvm::Function *f,
                          llvm::StoreInst *inst);

  virtual void visitLoad(PointerAnalysis *pta,
                         llvm::Function *f,
                         llvm::LoadInst *inst);

private:

  /* TODO: add docs */
  std::set<llvm::Function *> called;
  /* TODO: add docs */
  StateProjection &projection;
  /* TODO: add docs */
  bool collectMod;
};

size_t getFlatModSize(StateProjection &projection);

}

#endif
