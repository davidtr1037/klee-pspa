#include "SymbolicPTA.h"
using namespace klee;

bool SymbolicPTA::isPointerOffset(Pointer& p) {
    auto type = getMemoryObjectType(p.pointerContainer);
    auto pty = dyn_cast<llvm::PointerType>(type);
    assert(pty && "assumes input is a pointer type");

    auto typeSize = layout.getTypeStoreSize(pty->getElementType());
    auto offset = p.offset->getZExtValue();
    offset = offset % typeSize;

    OffsetFinder ofPacked(layout);
    for(const auto& offsetWeak : ofPacked.visit(pty->getElementType())) {
        if(offsetWeak.first == offset) 
            return true;
    }
    llvm::errs() << p.print();
    pty->dump();
    return false;
}

//Pointers can also be the pointed to objects which are not neccesarly pointers
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

Pointer* SymbolicPTA::getFunctionPointer(const llvm::Function* f) {
    //MemoryObject and llvm::Function addresses can't overlap so this is fine but hacky
    std::vector<Pointer*> &ptrs = allPointers[(const MemoryObject*)f];
    if(ptrs.size() > 0) 
       return ptrs[0];

    Pointer* retPtr = new Pointer(f);
    ptrs.push_back(retPtr);
    return retPtr;
}
std::vector<Pointer*> SymbolicPTA::handleFunctionPtr(ref<Expr> fp) {
    std::vector<Pointer*> ret;
    auto cfp = dyn_cast<ConstantExpr>(fp);
    if(cfp == nullptr) return ret;
    //TODO: symbolic function pointers
    uint64_t addr = cfp->getZExtValue();

    if (legalFunctions.count(addr)) {
      const llvm::Function* f = (const llvm::Function *)(addr);
      ret.push_back(getFunctionPointer(f));
    }
    return ret;
}
std::vector<Pointer*> SymbolicPTA::getPointerTarget(Pointer &p) {
    std::vector<Pointer*> ret;
    if(p.isFunctionPtr()) return ret;
    const ObjectState* os = state.addressSpace.findObject(p.pointerContainer);
    auto ptrWidth = Context::get().getPointerWidth();
    //holds pointers that need to be considered
    std::vector<Pointer*> ps;
    if(p.multiplePointers) {
        ps = getColocatedPointers(p);
    } else
      ps.push_back(&p);


    int cnt = 0;
    for(auto cp : ps) {
      assert(isPointerOffset(*cp) && "Trying to resolve non pointer type field as pointer");  
      ref<Expr> ptr = os->read(cp->offset, ptrWidth);
      ResolutionList rl;
      state.addressSpace.resolve(state, &solver, ptr, rl);
      for(auto &op : rl) {
          const MemoryObject* mo = op.first;
          Pointer *p = getPointer(mo, mo->getOffsetExpr(ptr));
          p->weakUpdate = cnt != 0;
          ret.push_back(p);
          cnt++;
      }
      auto fps = handleFunctionPtr(ptr);
      ret.insert(ret.end(), std::begin(fps), std::end(fps));

    }
    return ret;
}

std::vector<Pointer*> SymbolicPTA::getColocatedPointers(Pointer &p) {
  //If it's a pointer type it can be an array
  std::vector<Pointer*> ret; 
  if(p.isFunctionPtr()) return ret;

  const MemoryObject* mo = p.pointerContainer;
  llvm::Type* ty = getMemoryObjectType(mo);
  llvm::PointerType *pty = dyn_cast<llvm::PointerType>(ty); 
  assert(pty != nullptr && "Memory object must point to something");
  ty = pty->getElementType();
  //stride is in bytes
  auto stride = layout.getTypeStoreSize(ty);


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

    //special cases
    if(mo->name == "__args") { //for MO that holds arguments
        t = llvm::Type::getInt8Ty(mo->allocSite->getContext())->getPointerTo();
    } else if(mo->name == "argv") { //For argvMO
        t = llvm::Type::getInt8Ty(mo->allocSite->getContext())->getPointerTo()->getPointerTo();
   //End special cases 
    } else if(auto GV = dyn_cast<llvm::GlobalVariable>(mo->allocSite)) {
        assert(GV->getType()->isPointerTy() && "GV has non pointer type");
        t = GV->getType();
    } else if(mo->allocSite->getNumUses() != 1) {
        mo->allocSite->dump();
//        state.dumpStack(llvm::errs());
        mo->allocSite->getType()->dump();
        llvm::errs() << mo->name << " has multiple uses\n";
//        assert(0 && "Unhandled multiple uses");
        t = mo->allocSite->getType();
    } else if(const auto BI = dyn_cast<llvm::CastInst>(mo->allocSite->user_back())) {
        t = BI->getDestTy();
        assert(t->isPointerTy() && "BI has non pointer type");
    } else {
        llvm::errs() << mo->name << " has non bitcast 1 use\n";
        mo->allocSite->dump();
        mo->allocSite->user_back()->dump();
        //assert(0 && "Unhandled BI type");
        t = mo->allocSite->getType();
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

SymbolicPTA::~SymbolicPTA() {
   for(const auto& moPtrs : allPointers) {
       for(const auto& ptr : moPtrs.second) {
           delete ptr;
       }
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
    seenPointers.insert(p); //If p is not a pointer

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
    int numElements = AT->getNumElements();
    auto len = layout.getTypeStoreSize(AT->getElementType()); 
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
