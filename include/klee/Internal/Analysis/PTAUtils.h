#ifndef KLEE_PTAUTILS_H
#define KLEE_PTAUTILS_H

#include <klee/Internal/Analysis/PTAStats.h>

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

#include <set>
#include <vector>


namespace klee {

void dumpNodeInfo(PointerAnalysis *pta,
                  NodeID nodeId,
                  unsigned int level = 0);

bool canEscape(PAG *pag,
               NodeID nodeId,
               std::set<llvm::Function *> called);

NodeID stripUniqueAS(PointerAnalysis *pta,
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
  std::set<NodeID> mod;
  std::set<NodeID> ref;
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

class ModRefCollector : public InstructionVisitor {

public:

  ModRefCollector(std::set<llvm::Function *> called,
                  bool collectMod = true,
                  bool collectRef = true) :
    called(called),
    collectMod(collectMod),
    collectRef(collectRef) {

  }

  virtual void visitStore(PointerAnalysis *pta,
                          llvm::Function *f,
                          llvm::StoreInst *inst);

  virtual void visitLoad(PointerAnalysis *pta,
                         llvm::Function *f,
                         llvm::LoadInst *inst);

  void dumpModSet(PointerAnalysis *pta);

  void dumpRefSet(PointerAnalysis *pta);

private:

  std::set<llvm::Function *> called;
  bool collectMod;
  bool collectRef;
  std::set<NodeID> mod;
  std::set<NodeID> ref;
};

}

#endif
