#include "SymbolicPTA.h"
using namespace klee;

//This currently ignores cases where offset can point to multiple pointers in a MemoryObject
Pointer* SymbolicPTA::getPointer(const MemoryObject* mo, ref<Expr> offset) {
    Pointer *retPtr = nullptr;
    std::vector<Pointer*> &ptrs = allPointers[mo];
    for(auto p : ptrs) {
        if(mayBeTrue(EqExpr::create(p->offset, offset))) {
            if(retPtr == nullptr) {
                retPtr = p;
            } else {
              //We resolved to a ptr already, so we flag it as mutliple resultions
                retPtr->multiplePointers = true;
            }
        }
    }
    if(retPtr != nullptr) 
       return retPtr;

    retPtr = new Pointer(mo, offset);
    ptrs.push_back(retPtr);
    return retPtr;
}

std::vector<Pointer*> SymbolicPTA::getPointerTarget(Pointer &p) {
    assert(p.multiplePointers == false && "handling not implemented yet");
    const ObjectState* os = state.addressSpace.findObject(p.pointerContainer);
    ref<Expr> ptr = os->read(p.offset, Context::get().getPointerWidth());

    std::vector<Pointer*> ret;

    ResolutionList rl;
    state.addressSpace.resolve(state, &solver, ptr, rl);
    for(auto &op : rl) {
        const MemoryObject* mo = op.first;
        ret.push_back(getPointer(mo, mo->getOffsetExpr(ptr)));
    }
    return ret;
}

std::vector<Pointer*> SymbolicPTA::getColocatedPointers(Pointer &p) {
  //TODO: properly do colocated pointers    
  std::vector<Pointer*> ret(1, &p); 
  return ret;
}
 
bool SymbolicPTA::mustBeTrue(klee::ref<Expr> e) {
    bool res;
    if(solver.mustBeTrue(state, e, res)) {
        return res;
    } else {
        assert(0 && "Solver failure not handled in SymbolicPTA");
    }
}

bool SymbolicPTA::mayBeTrue(klee::ref<Expr> e) {
    bool res;
    if(solver.mayBeTrue(state, e, res)) {
        return res;
    } else {
        assert(0 && "Solver failure not handled in SymbolicPTA");
    }
}
