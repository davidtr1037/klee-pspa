#include "AIPhase.h"

#include <MemoryModel/PointerAnalysis.h>

using namespace klee;
using namespace llvm;

ExecutionState *AIPhase::getInitialState() {
  return initialState;
}

void AIPhase::setInitialState(ExecutionState *es) {
  initialState = es;
}

void AIPhase::updateMod(NodeID src, PointsTo dst, bool isStrong) {
  PointsTo &pts = pointsToMap[src];
  if (isStrong) {
    pts.clear();
  }
  for (NodeID n : dst) {
    pts.set(n);
  }
}

void AIPhase::clearAll() {
  pointsToMap.clear();
}

void AIPhase::reset() {
  setInitialState(nullptr);
  clearAll();
  stats.reset();
}

void AIPhase::dump() {
  errs() << "### AI Phase Summary ###\n";
  errs() << "explored path: " << stats.exploredPaths << "\n";
  errs() << "forks: " << stats.forks << "\n";
  errs() << "discarded states: " << stats.discarded << "\n";
  for (auto i : pointsToMap) {
    NodeID src = i.first;
    PointsTo &pts = i.second;
    errs() << src << " { ";
    for (NodeID x : pts) {
      errs() << x << " ";
    }
    errs() << "}\n";
  }
}
