#include "Passes.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>

using namespace llvm;
using namespace klee;

char LoopInfoCollector::ID;

bool LoopInfoCollector::runOnFunction(llvm::Function &f) {
  DominatorTree dtree(f);
  LoopInfo loopInfo(dtree);

  auto &headers = loopHeaders[&f];
  for (Loop *loop : loopInfo) {
    BasicBlock *header = loop->getHeader();
    headers.insert(&*header->begin());
    for (Loop *subLoop : loop->getSubLoops()) {
      BasicBlock *subHeader = subLoop->getHeader();
      headers.insert(&*subHeader->begin());
    }
  }
  return true;
}

//bool LoopInfoCollector::runOnLoop(llvm::Loop *loop,
//                                  llvm::LPPassManager &lpm) {
//  loop->dump();
//  return true;
//}
