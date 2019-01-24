#include "ExtendedSearcher.h"
#include "Executor.h"
#include <klee/ExecutionState.h>

#include <llvm/Support/raw_ostream.h>

using namespace klee;
using namespace llvm;

ExtendedSearcher::ExtendedSearcher(Searcher *baseSearcher,
                                   Searcher *saSearcher,
                                   Executor &executor) :
  baseSearcher(baseSearcher),
  saSearcher(saSearcher),
  executor(executor) {

}

ExtendedSearcher::~ExtendedSearcher() {
  delete baseSearcher;
  delete saSearcher;
}

ExecutionState &ExtendedSearcher::selectState() {
  if (!saSearcher->empty()) {
    return saSearcher->selectState();
  } else {
    if (executor.getExecutionMode() == Executor::ExecutionModeAI) {
      executor.setExecutionMode(Executor::ExecutionModeSymbolic);
      return *executor.getPendingState();
    }
  }

  return baseSearcher->selectState();
}

void ExtendedSearcher::update(
  ExecutionState *current,
  const std::vector<ExecutionState *> &addedStates,
  const std::vector<ExecutionState *> &removedStates
) {
  std::vector<ExecutionState *> addedBaseStates;
  std::vector<ExecutionState *> addedDummyStates;
  std::vector<ExecutionState *> removedBaseStates;
  std::vector<ExecutionState *> removedDummyStates;

  for (ExecutionState *es : addedStates) {
    if (es->isDummy) {
      addedDummyStates.push_back(es);
    } else {
      addedBaseStates.push_back(es);
    }
  }

  for (ExecutionState *es : removedStates) {
    if (es->isDummy) {
      removedDummyStates.push_back(es);
    } else {
      removedBaseStates.push_back(es);
    }
  }

  baseSearcher->update(current && !current->isDummy ? current : nullptr,
                       addedBaseStates,
                       removedBaseStates);
  saSearcher->update(current && current->isDummy ? current : nullptr,
                     addedDummyStates,
                     removedDummyStates);
}

bool ExtendedSearcher::empty() {
  return baseSearcher->empty() && saSearcher->empty();
}
