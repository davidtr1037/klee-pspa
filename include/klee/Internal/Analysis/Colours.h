#ifndef KLEE_COLOURS_H
#define KLEE_COLOURS_H

#include <klee/Internal/Analysis/PTAUtils.h>

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include "llvm/ADT/BitVector.h"

namespace klee {

class ColourCollector : public InstructionVisitor {
  
public:
  

  virtual void visitStore(PointerAnalysis *pta,
                          llvm::Function *f,
                          llvm::StoreInst *inst);

  std::vector<int> getColour(llvm::Instruction *inst);

  ColourCollector() {}

  bool intersects(PointerAnalysis* pta,
                  PointsTo &pts1,
                  PointsTo &pts2);

  void computeColours(PointerAnalysis* pta, llvm::raw_ostream &outputFile);
  //True when colours can are computed
  bool witMode = false;

private:
  std::vector<PointsTo> ptsSets;
  PointerAnalysis* pta;
};

}

#endif
