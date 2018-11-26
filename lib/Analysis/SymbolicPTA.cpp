#include "SymbolicPTA.h"
using namespace klee;

//This currently ignores cases where offset can point to multiple pointers in a MemoryObject
Pointer* SymbolicPTA::getPointer(const MemoryObject* mo, ref<Expr> offset) {
    Pointer *retPtr = nullptr;
    std::vector<Pointer*> &ptrs = allPointers[mo];
    for(auto p : ptrs) {
        if(mayBeTrue(EqExpr::create(p->offset->ZExt(offset->getWidth()), offset))) {
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

    if(auto co = dyn_cast<ConstantExpr>(offset))
      retPtr = new Pointer(mo, co);
    else {
      ref<ConstantExpr> constant_offset;
      solver.getValue(state, offset, constant_offset);
      retPtr = new Pointer(mo, constant_offset);
      if(mayBeTrue(NeExpr::create(offset, constant_offset)))
        retPtr->multiplePointers = true;
    }
    ptrs.push_back(retPtr);
    return retPtr;
}

std::vector<Pointer*> SymbolicPTA::getPointerTarget(Pointer &p) {
    assert(p.multiplePointers == false && "handling not implemented yet");
    const ObjectState* os = state.addressSpace.findObject(p.pointerContainer);
    std::vector<Pointer*> ret;
    auto ptrWidth = Context::get().getPointerWidth();
    ref<Expr> ptr = os->read(p.offset, ptrWidth);
      ResolutionList rl;
      state.addressSpace.resolve(state, &solver, ptr, rl);
      for(auto &op : rl) {
          const MemoryObject* mo = op.first;
          Pointer *p = getPointer(mo, mo->getOffsetExpr(ptr));
          ret.push_back(p);
      }
    return ret;
}

std::vector<Pointer*> SymbolicPTA::getColocatedPointers(Pointer &p) {
  const MemoryObject* mo = p.pointerContainer;
  llvm::Type* ty = getMemoryObjectType(mo);
  llvm::PointerType *pty = dyn_cast<llvm::PointerType>(ty); 
  assert(pty != nullptr && "Memory object must point to something");
  ty = pty->getElementType();
  auto stride = layout.getTypeAllocSize(ty);

  //If it's a pointer type it can be an array
  std::vector<Pointer*> ret; 

  OffsetFinder of(layout);
  std::vector<std::pair<unsigned, bool>> offsets = of.visit(ty);
  
  for(auto o : offsets) {
      for(auto off = o.first; off < mo->size; off += stride) {
          Pointer *p = getPointer(mo, ConstantExpr::create(off, Expr::Int32));
          p->weakUpdate = o.second || off != o.first;
          ret.push_back(p);
      }
  }

  
  return ret;
}
 
llvm::Type* SymbolicPTA::getMemoryObjectType(const MemoryObject* mo) {
    llvm::Type* t = moTypes[mo];
    if(t != nullptr) 
        return t;
    if(mo->allocSite == nullptr) 
       assert(0 && "Can't type memory object without allocSite, caller needs to giveMOType");
    
    if(auto GV = dyn_cast<llvm::GlobalVariable>(mo->allocSite)) {
        assert(GV->getType()->isPointerTy() && "GV has non pointer type");
        t = GV->getType();
    } else if(mo->allocSite->getNumUses() != 1) {
        llvm::errs() << mo->name << " has multiple uses\n";
        assert(0 && "Unhandled multiple uses");
    } else if(const auto BI = dyn_cast<llvm::CastInst>(mo->allocSite->user_back())) {
        t = BI->getDestTy();
        assert(t->isPointerTy() && "BI has non pointer type");
    } else {
        llvm::errs() << mo->name << " has non bitcast 1 use\n";
        mo->allocSite->dump();
        assert(0 && "Unhandled BI type");
    }

    giveMemoryObjectType(mo, t);
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
/* =====================================Iterator ================================= 
   =================================================================================== */
SymbolicPTA::TransitiveTraverser::iterator::iterator(SymbolicPTA &s, Pointer *p): symPTA(s) {
    processNext(p);
}

void SymbolicPTA::TransitiveTraverser::iterator::processNext(Pointer *p) {
    //avoid loops
    if(seenPointers.count(p) != 0) 
        return;

    for(Pointer* s: symPTA.getColocatedPointers(*p)) {
        seenPointers.insert(s);
        for(Pointer* t: symPTA.getPointerTarget(*s)) {
            ptrsToReturn.emplace_back(s, t);
        }
    }
}

SymbolicPTA::TransitiveTraverser::iterator::iterator(SymbolicPTA &s):symPTA(s){}

bool SymbolicPTA::TransitiveTraverser::iterator::operator!=(const iterator& other) {
//    return (seenPointers != other.seenPointers) || (ptrsToReturn != other.ptrsToReturn);
    return (ptrsToReturn != other.ptrsToReturn);
}
bool SymbolicPTA::TransitiveTraverser::iterator::operator==(const iterator& other) {
    if(this == &other) return true;
//    return (seenPointers == other.seenPointers) && (ptrsToReturn == other.ptrsToReturn);
    return (ptrsToReturn == other.ptrsToReturn);
}
void SymbolicPTA::TransitiveTraverser::iterator::operator++() {
    processNext(ptrsToReturn[0].second);
    ptrsToReturn.pop_front();
}

std::pair<Pointer*, Pointer*>& SymbolicPTA::TransitiveTraverser::iterator::operator*() {
    return ptrsToReturn[0];
}
std::pair<Pointer*, Pointer*>* SymbolicPTA::TransitiveTraverser::iterator::operator->() {
    return &ptrsToReturn[0];
}





/* =====================================Type Visitor ================================= 
   =================================================================================== */
template <class T>
T TypeVisitor<T>::visit(llvm::Type* t) {
    visitCount++;
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

    visitCount--;
    T r =  results;
    if(visitCount == 0)
        reset();
    return r;
}

void OffsetFinder::reset() {
    results.clear();
    globalOffset = 0;
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

void OffsetFinder::visitArray(llvm::ArrayType* AT) {
    auto numElements = AT->getNumElements();
    auto len = layout.getTypeAllocSize(AT->getElementType()); 
    for(int i = 0; i < numElements; i++) {
      if(!weakUpdate)
        weakUpdate = i != 0;
      globalOffset += i*len; 
      visit(AT->getElementType());
      globalOffset -= i*len;
    }

}
void OffsetFinder::visitPointer(llvm::PointerType* st) {
    results.emplace_back(globalOffset, weakUpdate);
}
void OffsetFinder::visitInteger(llvm::IntegerType* st) {
    //Do nothing
}

template class TypeVisitor<std::vector<std::pair<unsigned, bool>>>;
