#include "../Core/Memory.h"
#include "../Core/TimingSolver.h"
#include "../Core/Context.h"

#include "klee/Expr.h"
#include "klee/ExecutionState.h"

#include <vector>
#include <unordered_map>


namespace klee {

class SymbolicPTA;

class Pointer {
//No public members should go through SymbolicPTA
  friend SymbolicPTA;
public:
private:
 Pointer (const MemoryObject* mo, ref<Expr> o): pointerContainer(mo), offset(o) {}
 const MemoryObject* pointerContainer; //chuck of memory where pointer can be read from
 ref<Expr> offset; //offset into the container where pointer is located

};

class SymbolicPTA {
public:
  //Gets the pointer representation of the location
  Pointer* getPointer(const MemoryObject* mo, ref<Expr> offset);
  std::vector<Pointer*> getPointerTarget(Pointer &p);
  std::vector<Pointer*> getColocatedPointers(Pointer &p);
  SymbolicPTA(TimingSolver &solver, ExecutionState &state): solver(solver), state(state) {}

private:
  TimingSolver &solver;
  ExecutionState &state;
  std::unordered_map<const MemoryObject*, std::vector<Pointer*>> allPointers;
  bool mustBeTrue(ref<Expr> e);
  bool mayBeTrue(ref<Expr> e);
};

}
