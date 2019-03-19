#ifndef KLEE_COLOURS_H
#define KLEE_COLOURS_H

#include <klee/Internal/Analysis/PTAUtils.h>

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

namespace klee {

class ColourCollector : public InstructionVisitor {
  
public:
  

  virtual void visitStore(PointerAnalysis *pta,
                          llvm::Function *f,
                          llvm::StoreInst *inst);

  ColourCollector(llvm::raw_ostream &outputFile): outputFile(outputFile) {}

  bool intersects(PointerAnalysis* pta,
                  PointsTo &pts1,
                  PointsTo &pts2);

  void computeColours(PointerAnalysis* pta);

private:
  std::vector<PointsTo> ptsSets;
  llvm::raw_ostream &outputFile;
};

}

#endif
