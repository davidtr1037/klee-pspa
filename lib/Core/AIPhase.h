#ifndef KLEE_AIPHASE_H
#define KLEE_AIPHASE_H

#include <MemoryModel/PointerAnalysis.h>

#include "klee/ExecutionState.h"

#include <stdint.h>
#include <vector>

namespace klee {

class AIPhase {

public:

    AIPhase() :
      exploredPaths(0), forks(0), initialState(nullptr) {

    }

    ExecutionState *getInitialState();

    void setInitialState(ExecutionState *es);

    void updateMod(NodeID src, PointsTo dst, bool isStrong);

    void clearAll();

    void reset();

    void dump();

    /* statistics */
    uint64_t exploredPaths;
    uint64_t forks;

private:

    /* the symbolic state from which the exploration starts */
    ExecutionState *initialState;
    /* TODO: add docs */
    std::map<NodeID, PointsTo> pointsToMap;
};

}

#endif
