#ifndef KLEE_AIPHASE_H
#define KLEE_AIPHASE_H

#include <llvm/IR/Function.h>

#include <MemoryModel/PointerAnalysis.h>

#include "klee/ExecutionState.h"

#include <stdint.h>
#include <vector>

namespace klee {

class AIPhase {

public:

  struct Stats {
    uint64_t exploredPaths;
    uint64_t forks;
    uint64_t discarded;
    std::set<llvm::Function *> reachable;

    Stats() :
      exploredPaths(0),
      forks(0),
      discarded(0) {

    }

    void reset() {
      exploredPaths = 0;
      forks = 0;
      discarded = 0;
      reachable.clear();
    }
  };

public:

  typedef std::map<NodeID, PointsTo> PointsToMap;

  AIPhase() :
    initialState(nullptr) {

  }

  ExecutionState *getInitialState();

  void setInitialState(ExecutionState *es);

  void updateMod(NodeID src, PointsTo dst, bool isStrong);

  const PointsToMap &getPointsToMap() {
    return pointsToMap;
  };

  void clearAll();

  bool shouldDiscardState(ExecutionState &state,
                          ref<Expr> condition);

  bool canIgnore(PAG *pag,
                 NodeID nodeId,
                 std::set<llvm::Function *> &called);

  void getStateProjection(PAG *pag,
                          std::set<llvm::Function *> &called,
                          StateProjection &projection);

  void reset();

  void dump();

  /* statistics */
  Stats stats;

private:

  /* the symbolic state from which the exploration starts */
  ExecutionState *initialState;
  /* TODO: add docs */
  PointsToMap pointsToMap;
};

}

#endif
