#include "Passes.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include <set>

using namespace llvm;
using namespace std;

namespace klee {

char UnusedValuesRemovalPass::ID;

bool UnusedValuesRemovalPass::runOnModule(Module &module) {
  /* entry function must not be removed */
  set<string> keep;
  keep.insert(entry);

  std::set<Function *> functions;
  std::set<GlobalVariable *> globals;
  std::set<GlobalAlias *> aliases;

  for (Function &f : module) {
    if (keep.find(f.getName().str()) != keep.end()) {
      continue;
    }

    if (f.hasNUses(0)) {
      functions.insert(&f);
    }
  }

  for (GlobalVariable &g : module.globals()) {
    if (g.hasNUses(0)) {
      globals.insert(&g);
    }
  }

  for (GlobalAlias &ga : module.aliases()) {
    if (ga.hasNUses(0)) {
      aliases.insert(&ga);
    }
  }

  for (Function *f : functions) {
    f->eraseFromParent();
  }

  for (GlobalVariable *g : globals) {
    g->eraseFromParent();
  }

  for (GlobalAlias *ga : aliases) {
    ga->eraseFromParent();
  }

  return !functions.empty() || !globals.empty() || !aliases.empty();
}

}
