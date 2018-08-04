#ifndef KLEE_PTAUTILS_H
#define KLEE_PTAUTILS_H

#include <klee/Internal/Analysis/PTAStats.h>

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

namespace klee {

void dumpNodeInfo(PointerAnalysis *pta,
                  NodeID nodeId);

class InstructionVisitor {

public:

  InstructionVisitor() {

  }

  virtual void visitFunction(PointerAnalysis *pta, llvm::Function *f);

  virtual void visitReachable(PointerAnalysis *pta, llvm::Function *entry);

  virtual void visitAll(PointerAnalysis *pta);

  virtual void visitStore(PointerAnalysis *pta,
                          llvm::Function *f,
                          llvm::StoreInst *inst) {

  }

  virtual void visitLoad(PointerAnalysis *pta,
                         llvm::Function *f,
                         llvm::LoadInst *inst) {

  }

};


class StatsCollector : public InstructionVisitor {

public:

  StatsCollector(bool dump) :
    dump(dump) {

  }

  virtual void visitStore(PointerAnalysis *pta,
                          llvm::Function *f,
                          llvm::StoreInst *inst);

  virtual void visitLoad(PointerAnalysis *pta,
                         llvm::Function *f,
                         llvm::LoadInst *inst);

  PTAStats &getStats() {
    return stats;
  }

private:

  void updateStats(NodeID nodeId, unsigned size);

  bool dump;
  PTAStats stats;
  std::set<NodeID> visited;
};

class ResultsCollector : public InstructionVisitor {

public:

  ResultsCollector(llvm::raw_ostream &log) :
    log(log) {

  }

  virtual void visitReachable(PointerAnalysis *pta,
                              llvm::Function *entry);

  virtual void visitStore(PointerAnalysis *pta,
                          llvm::Function *f,
                          llvm::StoreInst *inst);

  virtual void visitLoad(PointerAnalysis *pta,
                         llvm::Function *f,
                         llvm::LoadInst *inst);

private:

  llvm::raw_ostream &log;
};

}

#endif
