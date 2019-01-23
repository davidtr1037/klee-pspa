#ifndef KLEE_AIPHASE_H
#define KLEE_AIPHASE_H

#include <MemoryModel/PointerAnalysis.h>

#include "klee/ExecutionState.h"

#include <stdint.h>
#include <vector>

namespace klee {

class AIPhase {

  struct Stats {
    uint64_t exploredPaths;
    uint64_t forks;
    uint64_t discarded;

    Stats() :
      exploredPaths(0),
      forks(0),
      discarded(0) {

    }

    void reset() {
      exploredPaths = 0;
      forks = 0;
      discarded = 0;
    }
  };

public:

    AIPhase() :
      initialState(nullptr) {

    }

    ExecutionState *getInitialState();

    void setInitialState(ExecutionState *es);

    void updateMod(NodeID src, PointsTo dst, bool isStrong);

    void clearAll();

    bool shouldDiscardState(ExecutionState &state,
                            ref<Expr> condition);

    void reset();

    void dump();

    /* statistics */
    Stats stats;

private:

    /* the symbolic state from which the exploration starts */
    ExecutionState *initialState;
    /* TODO: add docs */
    std::map<NodeID, PointsTo> pointsToMap;
};

}

#endif
