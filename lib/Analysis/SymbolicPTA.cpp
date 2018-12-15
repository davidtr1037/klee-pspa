#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

#include "SymbolicPTA.h"

using namespace llvm;
using namespace klee;


bool SymbolicPTA::isPointerOffset(Pointer &p) {
  Type *type = getMemoryObjectType(p.pointerContainer);
  PointerType *pty = dyn_cast<PointerType>(type);
  assert(pty && "assumes input is a pointer type");

  uint64_t typeSize = layout.getTypeStoreSize(pty->getElementType());
  uint64_t offset = p.offset;
  offset = offset % typeSize;

  for (const auto &offsetWeak : of.visit(pty->getElementType())) {
    if (offsetWeak.offset == offset) {
      return true;
    }
  }
  return false;
}

// Pointers can also be the pointed to objects which are not neccesarly pointers
// This currently ignores cases where offset can point to multiple pointers in a MemoryObject
Pointer* SymbolicPTA::getPointer(const MemoryObject *mo,
                                 ref<Expr> offset) {
  Pointer *retPtr = nullptr;
  std::vector<Pointer*> &ptrs = allPointers[mo];
  if (auto co = dyn_cast<ConstantExpr>(offset)) {
    uint64_t off = co->getZExtValue();
    for (Pointer *p : ptrs) {
      if (p->offset == off) {
        return p;
      }
    }
    retPtr = new Pointer(mo, co);
    ptrs.push_back(retPtr);
    return retPtr;
  } else {
    for (auto p : ptrs) {
      ref<Expr> e = EqExpr::create(ConstantExpr::create(p->offset, offset->getWidth()), offset);
      if (mayBeTrue(e)) {
        if (retPtr == nullptr) {
          retPtr = p;
        } else {
          // We resolved to a ptr already, so we flag it as mutliple resultions
          retPtr->multiplePointers = true;
        }
      }
    }
  }

  // On a path where offset is symbolic
  if (retPtr != nullptr) {
    return retPtr;
  }

  ref<ConstantExpr> constant_offset;
  solver.getValue(state, offset, constant_offset);
  retPtr = new Pointer(mo, constant_offset);
  if (mayBeTrue(NeExpr::create(offset, constant_offset))) {
    retPtr->multiplePointers = true;
  }

  ptrs.push_back(retPtr);
  return retPtr;
}

Pointer* SymbolicPTA::getFunctionPointer(const Function *f) {
  // MemoryObject and Function addresses can't overlap so this is fine but hacky
  std::vector<Pointer*> &ptrs = allPointers[(const MemoryObject*)(f)];
  if (ptrs.size() > 0) {
   return ptrs[0];
  }

  Pointer* retPtr = new Pointer(f);
  ptrs.push_back(retPtr);
  return retPtr;
}

std::vector<Pointer*> SymbolicPTA::handleFunctionPtr(ref<Expr> fp) {
  std::vector<Pointer*> ret;
  ConstantExpr *cfp = dyn_cast<ConstantExpr>(fp);
  if (cfp == nullptr) {
    return ret;
  }

  // TODO: symbolic function pointers
  uint64_t addr = cfp->getZExtValue();
  if (legalFunctions.count(addr)) {
    const Function *f = (const Function *)(addr);
    ret.push_back(getFunctionPointer(f));
  }
  return ret;
}

std::vector<Pointer*> SymbolicPTA::getPointerTarget(Pointer &p) {
  std::vector<Pointer*> ret;
  if (p.isFunctionPtr()) {
    return ret;
  }

  const ObjectState* os = state.addressSpace.findObject(p.pointerContainer);
  uint64_t ptrWidth = Context::get().getPointerWidth();
  // holds pointers that need to be considered
  std::vector<Pointer *> pointers;
  if (p.multiplePointers) {
    pointers = getColocatedPointers(p);
  } else {
    pointers.push_back(&p);
  }

  uint64_t cnt = 0;
  for (Pointer *cp : pointers) {
    assert(isPointerOffset(*cp) && "Trying to resolve non pointer type field as pointer");
    ref<Expr> ptr = os->read(cp->offset, ptrWidth);
    ResolutionList rl;
    state.addressSpace.resolve(state, &solver, ptr, rl);
    for (auto &op : rl) {
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

std::vector<Pointer *> SymbolicPTA::getColocatedPointers(Pointer &p) {
  // If it's a pointer type it can be an array
  std::vector<Pointer *> ret;
  if (p.isFunctionPtr()) {
    return ret;
  }

  const MemoryObject *mo = p.pointerContainer;
  Type *ty = getMemoryObjectType(mo);
  PointerType *pty = dyn_cast<PointerType>(ty);
  assert(pty != nullptr && "Memory object must point to something");
  ty = pty->getElementType();
  // stride is in bytes
  uint64_t stride = layout.getTypeStoreSize(ty);
  std::vector<TypeInfo> offsets = of.visit(ty);

  for (auto o : offsets) {
    for (unsigned int off = o.offset; off < mo->size; off += stride) {
      Pointer *p = getPointer(mo, ConstantExpr::create(off, Expr::Int32));
      p->weakUpdate = o.isWeak || off != o.offset;
      ret.push_back(p);
    }
  }

  return ret;
}
 
Type* SymbolicPTA::getMemoryObjectType(const MemoryObject *mo) {
  Type *t = moTypes[mo];
  if (t != nullptr) {
    return t;
  }

  if (mo->allocSite == nullptr) {
    assert(0 && "Can't type memory object without allocSite, caller needs to giveMOType");
  }

  // special cases
  if (mo->name == "__args") { // for MO that holds arguments
    t = Type::getInt8Ty(mo->allocSite->getContext())->getPointerTo();
  } else if (mo->name == "argv") { // For argvMO
    t = Type::getInt8Ty(mo->allocSite->getContext())->getPointerTo()->getPointerTo();
    // End special cases
  } else if (auto GV = dyn_cast<GlobalVariable>(mo->allocSite)) {
    assert(GV->getType()->isPointerTy() && "GV has non pointer type");
    t = GV->getType();
  } else if (mo->allocSite->getNumUses() != 1) {
    // mo->allocSite->dump();
    // state.dumpStack(errs());
    // mo->allocSite->getType()->dump();
    // errs() << mo->name << " has multiple uses\n";
    // assert(0 && "Unhandled multiple uses");
    t = mo->allocSite->getType();
  } else if (const auto BI = dyn_cast<CastInst>(mo->allocSite->user_back())) {
      t = BI->getDestTy();
      assert(t->isPointerTy() && "BI has non pointer type");
  } else {
    // errs() << mo->name << " has non bitcast 1 use\n";
    // mo->allocSite->dump();
    // mo->allocSite->user_back()->dump();
    // assert(0 && "Unhandled BI type");
    t = mo->allocSite->getType();
  }

  setMemoryObjectType(mo, t);
  return t;
}

void SymbolicPTA::setMemoryObjectType(const MemoryObject *mo,
                                      Type *type) {
  moTypes[mo] = type;
}

bool SymbolicPTA::mustBeTrue(klee::ref<Expr> e) {
  bool res;
  if (solver.mustBeTrue(state, e, res)) {
    return res;
  } else {
    assert(0 && "Solver failure not handled in SymbolicPTA");
  }
}

bool SymbolicPTA::mayBeTrue(klee::ref<Expr> e) {
  bool res;
  if (solver.mayBeTrue(state, e, res)) {
    return res;
  } else {
    assert(0 && "Solver failure not handled in SymbolicPTA");
  }
}

SymbolicPTA::~SymbolicPTA() {
  for (auto &i : allPointers) {
    for (Pointer *ptr : i.second) {
      delete ptr;
    }
  }
}

/* Iterator */

SymbolicPTA::TransitiveTraverser::iterator::iterator(SymbolicPTA &s,
                                                     Pointer *p) :
  symPTA(s) {
  processNext(p);
}

void SymbolicPTA::TransitiveTraverser::iterator::processNext(Pointer *p) {
  // avoid loops
  if (seenPointers.count(p) != 0) {
    return;
  }
  seenPointers.insert(p); // If p is not a pointer
  for (Pointer *s: symPTA.getColocatedPointers(*p)) {
    seenPointers.insert(s);

    for (Pointer *t: symPTA.getPointerTarget(*s)) {
      ptrsToReturn.emplace_back(s, t);
    }
  }
}

SymbolicPTA::TransitiveTraverser::iterator::iterator(SymbolicPTA &s) :
  symPTA(s) {

}

bool SymbolicPTA::TransitiveTraverser::iterator::operator!=(const iterator &other) {
  // return (seenPointers != other.seenPointers) || (ptrsToReturn != other.ptrsToReturn);
  return (ptrsToReturn != other.ptrsToReturn);
}

bool SymbolicPTA::TransitiveTraverser::iterator::operator==(const iterator &other) {
  if (this == &other) {
    return true;
  }
  // return (seenPointers == other.seenPointers) && (ptrsToReturn == other.ptrsToReturn);
  return (ptrsToReturn == other.ptrsToReturn);
}

void SymbolicPTA::TransitiveTraverser::iterator::operator++() {
  processNext(ptrsToReturn[0].second);
  ptrsToReturn.pop_front();
}

SymbolicPTA::PointsToPair& SymbolicPTA::TransitiveTraverser::iterator::operator*() {
  return ptrsToReturn[0];
}

SymbolicPTA::PointsToPair* SymbolicPTA::TransitiveTraverser::iterator::operator->() {
  return &ptrsToReturn[0];
}

/* Type Visitor */

template <class T>
T TypeVisitor<T>::visit(Type *t) {
  if (visitCount == 0 && cache.count(t) == 1) {
    return cache.at(t);
  }
  visitCount++;
  if (t->isStructTy()) {
    visitStruct(dyn_cast<StructType>(t));
  } else if (t->isArrayTy()) {
    visitArray(dyn_cast<ArrayType>(t));
  } else if (t->isPointerTy()) {
    visitPointer(dyn_cast<PointerType>(t));
  } else if (t->isIntegerTy()) {
    visitInteger(dyn_cast<IntegerType>(t));
  } else {
    errs() << "Unhandled type\n";
    t->dump();
    assert(0 && "Unhandled type");
  }

  visitCount--;
  T r =  results;
  if (visitCount == 0) {
    cache[t] = r;
    reset();
  }
  return r;
}

void OffsetFinder::reset() {
  results.clear();
  globalOffset = 0;
}

void OffsetFinder::visitStruct(StructType *st) {
  const StructLayout *l = layout.getStructLayout(st);
  int index = 0;
  for (auto subType : st->elements()) {
    uint64_t offset = l->getElementOffset(index);
    globalOffset += offset;
    visit(subType);
    globalOffset -= offset;
    index++;
  }
}

void OffsetFinder::visitArray(ArrayType *at) {
  uint64_t numElements = at->getNumElements();
  uint64_t len = layout.getTypeStoreSize(at->getElementType());
  size_t numResults = results.size();
  for (uint64_t i = 0; i < numElements; i++) {
    if (!weakUpdate) {
      weakUpdate = i != 0;
    }
    globalOffset += i * len;
    visit(at->getElementType());
    globalOffset -= i * len;
    if (numResults == results.size()) {
      break; // All elements are of the same type so nothing will change
    }
  }
}

void OffsetFinder::visitPointer(PointerType *pt) {
  results.emplace_back(globalOffset, weakUpdate);
  // if (st->getElementType()->isFunctionTy()) {
  //   errs() << "FP at offetset: " << globalOffset << " !!!\n";
  // }
}

void OffsetFinder::visitInteger(IntegerType *it) {
  // Do nothing
}

template class TypeVisitor<std::vector<TypeInfo>>;
