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

  for (Module::iterator i = module.begin(); i != module.end(); i++) {
    Function *f = &*i;
    if (keep.find(f->getName().str()) != keep.end()) {
      continue;
    }

    if (f->hasNUses(0)) {
      functions.insert(f);
    }
  }

  for (Function *f : functions) {
    f->eraseFromParent();
  }

  return !functions.empty();
}

}
