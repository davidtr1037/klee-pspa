#ifndef KLEE_EXTENDED_SEARCHER_H
#define KLEE_EXTENDED_SEARCHER_H

#include "Searcher.h"
#include "Executor.h"
#include <klee/ExecutionState.h>

#include <llvm/Support/raw_ostream.h>

#include <vector>

namespace klee {

  class ExtendedSearcher : public Searcher {

  public:

    ExtendedSearcher(Searcher *baseSearcher,
                     Searcher *saSearcher,
                     Executor &executor);

    ~ExtendedSearcher();

    ExecutionState &selectState();

    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates);

    bool empty();

    void printName(llvm::raw_ostream &os) {
      os << "ExtendedSearcher\n";
      os << "- base searcher: "; baseSearcher->printName(os);
      os << "- SA searcher: "; saSearcher->printName(os);
      os << "\n";
    }

  private:

    Searcher *baseSearcher;
    Searcher *saSearcher;
    Executor &executor;
  };

}

#endif
