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
  const MemoryObject* mo = p.pointerContainer;
  llvm::Type* ty = getMemoryObjectType(mo);
  llvm::PointerType *pty = dyn_cast<llvm::PointerType>(ty); 
  assert(pty != nullptr && "Memory object must point to something");
  ty = pty->getElementType();
  //OffsetFinder of;
  //std::vector<unsigned> offsets = of.visit(ty);
 
  //TODO: properly do colocated pointers    
  std::vector<Pointer*> ret(1, &p); 
  return ret;
}
 
llvm::Type* SymbolicPTA::getMemoryObjectType(const MemoryObject* mo) {
    llvm::Type* t = moTypes[mo];
    if(t == nullptr) {
        assert(0 && "TODO: type memory objects (with alloc uses or something)");
        giveMemoryObjectType(mo, t);
    }
    return t;
}
void SymbolicPTA::giveMemoryObjectType(const MemoryObject* mo, llvm::Type* type) {
  moTypes[mo] = type;
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

template <class T>
T TypeVisitor<T>::visit(llvm::Type* t) {
    if(t->isStructTy()) 
        visitStruct(dyn_cast<llvm::StructType>(t));
    else if(t->isArrayTy())
        visitArray(dyn_cast<llvm::ArrayType>(t));
    else if(t->isPointerTy()) 
        visitPointer(dyn_cast<llvm::PointerType>(t));
    else if(t->isIntegerTy())
        visitInteger(dyn_cast<llvm::IntegerType>(t));
    else {
        llvm::errs() << "Unhandled type\n";
        t->dump();
        assert(0 && "Unhandled type");
    }

    return results;
}

void OffsetFinder::visitStruct(llvm::StructType* ST) {
  const llvm::StructLayout* l = layout.getStructLayout(ST);
  int index = 0;
  for(auto subType : ST->elements()) {
      auto offset = l->getElementOffset(index);
      globalOffset += offset;
      visit(subType);
      globalOffset -= offset;
      index++;
  }
}

void OffsetFinder::visitArray(llvm::ArrayType* at) {
    assert(0 && "TODO arrays not supported");
}
void OffsetFinder::visitPointer(llvm::PointerType* st) {
    results.emplace_back(globalOffset);
}
void OffsetFinder::visitInteger(llvm::IntegerType* st) {
    //Do nothing
}

template class TypeVisitor<std::vector<unsigned int>>;
