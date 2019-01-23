#include "AIPhase.h"

#include <MemoryModel/PointerAnalysis.h>

#include <klee/ExecutionState.h>
#include <klee/Internal/Module/KInstruction.h>

using namespace klee;
using namespace llvm;

cl::opt<bool>
UseKUnrolling("use-k-unrolling", cl::init(false), cl::desc(""));

cl::opt<unsigned>
UnrollingLimit("unrolling-limit", cl::init(0), cl::desc(""));

cl::opt<unsigned>
ForksLimit("forks-limit", cl::init(0), cl::desc(""));

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

bool AIPhase::shouldDiscardState(ExecutionState &state,
                                 ref<Expr> condition) {
  if (ForksLimit && stats.forks > ForksLimit) {
    return true;
  }

  ref<Expr> simplified = state.constraints.simplifyExpr(condition);
  if (isa<ConstantExpr>(simplified)) {
    /* if the condition is not symbolic,
       then we don't have any restrictions in this case */
    return false;
  }

  /* the condition may be symbolic */
  StackFrame &sf = state.stack.back();
  Instruction *inst = state.prevPC->inst;
  if (sf.loopTrackingInfo.find(inst) != sf.loopTrackingInfo.end()) {
    /* this branch is the terminator of a loop header block */
    uint64_t count = sf.loopTrackingInfo[inst];
    if (UseKUnrolling && count > UnrollingLimit) {
      return true;
    }
  }
  return false;
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
