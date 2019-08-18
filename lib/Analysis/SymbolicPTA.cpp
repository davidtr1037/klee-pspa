#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

#include "SymbolicPTA.h"
#include "klee/Internal/Support/ErrorHandling.h"

using namespace llvm;
using namespace klee;


Pointer *SymbolicPTA::nullPointer = nullptr;

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
      if (p->offset == off && !p->multiplePointers) {
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
  //Get a new state to add temporary constraints to
  ExecutionState es(state); 
  es.addConstraint(UltExpr::create(offset, mo->getSizeExpr()));
  solver.getValue(es, offset, constant_offset);
  retPtr = new Pointer(mo, constant_offset);
  if (mayBeTrue(NeExpr::create(offset, constant_offset), es)) {
    retPtr->multiplePointers = true;
  }

  ptrs.push_back(retPtr);
  return retPtr;
}

Pointer* SymbolicPTA::getNullPointer() {
  if (!nullPointer) {
    nullPointer = new Pointer();
  }
  return nullPointer;
}

Pointer* SymbolicPTA::getFunctionPointer(const Function *f) {
  // MemoryObject and Function addresses can't overlap so this is fine but hacky
  std::vector<Pointer *> &ptrs = allPointers[(const MemoryObject *)(f)];
  if (ptrs.size() > 0) {
    return ptrs[0];
  }

  Pointer *retPtr = new Pointer(f);
  ptrs.push_back(retPtr);
  return retPtr;
}

Pointer *SymbolicPTA::handleUnresolved(ref<Expr> e) {
  ConstantExpr *ce = dyn_cast<ConstantExpr>(e);
  if (ce == nullptr) {
    return nullptr;
  }

  // TODO: symbolic function pointers
  uint64_t addr = ce->getZExtValue();
  if (addr == 0) {
    return getNullPointer();
  }

  if (legalFunctions.count(addr)) {
    const Function *f = (const Function *)(addr);
    return getFunctionPointer(f);
  }

  return nullptr;
}

std::vector<Pointer *> SymbolicPTA::getPointerTarget(Pointer &p) {
  std::vector<Pointer *> ret;
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
    if (cp->offset + (ptrWidth / 8) > p.pointerContainer->size) {
      /* TODO: add warning? */
      continue;
    }
    ref<Expr> ptr = os->read(cp->offset, ptrWidth);
    ResolutionList rl;
    bool timedout = state.addressSpace.resolve(state, &solver, ptr, rl, 0, 10);
    if(timedout) {
        klee_warning("Pointers %s resolution timed out in 10 second", cp->print().c_str());
    }

    for (auto &op : rl) {
      const MemoryObject *mo = op.first;
      Pointer *p = getPointer(mo, mo->getOffsetExpr(ptr));
      p->weakUpdate = cnt != 0;
      ret.push_back(p);
      cnt++;
    }
    Pointer *resolved = handleUnresolved(ptr);
    if (resolved) {
      ret.push_back(resolved);
    }
  }
  return ret;
}

std::vector<Pointer *> SymbolicPTA::getColocatedPointers(Pointer &p) {
  // If it's a pointer type it can be an array
  std::vector<Pointer *> ret;
  if (p.isNullPtr() || p.isFunctionPtr()) {
    return ret;
  }

  const MemoryObject *mo = p.pointerContainer;
  Type *ty = getMemoryObjectType(mo);
  PointerType *pty = dyn_cast<PointerType>(ty);
  assert(pty != nullptr && "Memory object must point to something");
  ty = pty->getElementType();
  // stride is in bytes
  uint64_t stride = layout.getTypeStoreSize(ty);
  std::vector<FieldDetails> offsets = of.visit(ty);

  for (auto o : offsets) {
    for (unsigned int off = o.offset; off < mo->size; off += stride) {
      Pointer *p = getPointer(mo, ConstantExpr::create(off, Expr::Int32));
      p->weakUpdate = o.isWeak || off != o.offset;
      ret.push_back(p);
    }
  }

  return ret;
}
 
/* TODO: we can use the type info collected by Executor::handleBitCast */
Type* SymbolicPTA::getMemoryObjectType(const MemoryObject *mo) {
  /* TODO: probably should use only this API... */
  PointerType *hint = mo->getTypeHint();
  if (hint) {
    return hint;
  }

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
  } else if (mo->name == "varrr args") { //for varargs
    const CallInst *ci = dyn_cast<CallInst>(mo->allocSite);
    assert(ci && "var args MO wasn't allocated at a call site");

    FunctionType *ft = ci->getFunctionType();
    assert(ft->isVarArg() && "vararg allocated at non vararg function");

    unsigned nonVarArgParamNumber = ft->getNumParams();
    std::vector<Type*> varArgTypes;

    for (const Use& u : ci->arg_operands()) {
        if (nonVarArgParamNumber > 0) {
          nonVarArgParamNumber--;
        } else {
          varArgTypes.push_back(u.get()->getType());
        } 
    }
    if (mo->size == 0) //means that there were no varargs, so do whatever
      t = Type::getInt8Ty(mo->allocSite->getContext())->getPointerTo();
    else
      //Might not handle allignment correctly
      t = StructType::create(varArgTypes, "", true)->getPointerTo();
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

bool SymbolicPTA::mayBeTrue(klee::ref<Expr> e, ExecutionState &s) {
  bool res;
  if (solver.mayBeTrue(s, e, res)) {
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

PointsToPair& SymbolicPTA::TransitiveTraverser::iterator::operator*() {
  return ptrsToReturn[0];
}

PointsToPair* SymbolicPTA::TransitiveTraverser::iterator::operator->() {
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
  } else if (t->isFloatingPointTy()) {
    //Can't cast here as there are 6 float types
    visitFloat(t);
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

void OffsetFinder::visitFloat(Type *ft) {
  // Do nothing
}
template class TypeVisitor<std::vector<FieldDetails>>;
