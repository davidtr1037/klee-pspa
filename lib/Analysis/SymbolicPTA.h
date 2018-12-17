#ifndef SYMBOLIC_PTA_H
#define SYMBOLIC_PTA_H

#include "../Core/Memory.h"
#include "../Core/TimingSolver.h"
#include "../Core/Context.h"

#include "klee/Expr.h"
#include "klee/ExecutionState.h"

#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DataLayout.h"

#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>


namespace klee {

class SymbolicPTA;
class Executor;

class Pointer {
  // No public members should go through SymbolicPTA
  friend SymbolicPTA;
  friend Executor;

public:

  bool isWeak() {
    return weakUpdate;
  }
 
  bool isFunctionPtr() {
    return pointerContainer == nullptr;
  }
 
  std::string print() {
    if (isFunctionPtr()) {
      return "function:" + f->getName().str();
    } else {
      return "mo: " + pointerContainer->name + ":" + std::to_string(offset);
    }
  }

private:

  Pointer(const MemoryObject *mo, ref<ConstantExpr> o) :
    pointerContainer(mo),
    offset(o->getZExtValue()),
    f(nullptr) {

  }

  Pointer(const llvm::Function *f) :
    pointerContainer(nullptr),
    offset(0),
    f(f) {

  }

  // chunck of memory where pointer can be read from
  const MemoryObject *pointerContainer;

  // offset into the container where pointer is located
  uint64_t offset;

  /* TODO: add docs */
  bool multiplePointers = false;

  /* TODO: add docs */
  bool weakUpdate = false;

  /* TODO: add docs */
  const llvm::Function *f;
};

/* TODO: we can use here getStructInfo, getFieldLayoutVec, ... from SVF */
template <class T>
class TypeVisitor {

  int visitCount = 0;

  std::unordered_map<llvm::Type*, T> cache;

public:

  T visit(llvm::Type *t);

  virtual void reset() = 0;

protected:

  virtual void visitStruct(llvm::StructType *st) = 0;

  virtual void visitArray(llvm::ArrayType *at) = 0;

  virtual void visitPointer(llvm::PointerType *st) = 0;

  virtual void visitInteger(llvm::IntegerType *st) = 0;

  T results;
};

struct FieldDetails {

    FieldDetails(unsigned int offset, bool isWeak) :
      offset(offset),
      isWeak(isWeak) {

    }

    /* the offset of the field (in bytes) */
    unsigned int offset;
    /* weather the field must be weakly updated */
    bool isWeak;
};

class OffsetFinder : public TypeVisitor<std::vector<FieldDetails>> {

  void visitStruct(llvm::StructType *st);

  void visitArray(llvm::ArrayType *at);

  void visitPointer(llvm::PointerType *st);

  void visitInteger(llvm::IntegerType *st);

  int globalOffset = 0;

  bool weakUpdate = false;

  llvm::DataLayout &layout;

public:

  OffsetFinder(llvm::DataLayout &l) :
    TypeVisitor<std::vector<FieldDetails>>(),
    layout(l) {

  }

  virtual void reset();
};

typedef std::pair<Pointer *, Pointer *> PointsToPair;

class SymbolicPTA {

  class TransitiveTraverser {

    /* TODO: add docs */
    SymbolicPTA &s;
    /* TODO: add docs */
    Pointer *p;

  public:

    class iterator {
      friend SymbolicPTA;

      /* TODO: add docs */
      std::deque<PointsToPair> ptrsToReturn;
      /* TODO: add docs */
      std::unordered_set<Pointer *> seenPointers;
      /* TODO: add docs */
      SymbolicPTA& symPTA;

      void processNext(Pointer *p);

      iterator(SymbolicPTA &s, Pointer *p); 

      // Has empty ptrsToReturn;
      iterator(SymbolicPTA &s);

    public:

      bool operator!=(const iterator& other);

      bool operator==(const iterator& other);

      void operator++();

      PointsToPair& operator*();

      PointsToPair* operator->();
    };

    TransitiveTraverser(SymbolicPTA &s, Pointer *p) :
      s(s), 
      p(p) {

    }

    iterator begin() const {
      return iterator(s, p);
    }

    iterator end() const {
      return iterator(s); 
    }

  };

public:

  SymbolicPTA(TimingSolver &solver,
              ExecutionState &state,
              std::set<uint64_t> &lf,
              llvm::DataLayout &l) :
    solver(solver),
    state(state),
    legalFunctions(lf),
    layout(l),
    of(l) {

  }

  ~SymbolicPTA();

  // Gets the pointer representation of the location
  Pointer* getPointer(const MemoryObject *mo,
                      ref<Expr> offset);

  Pointer* getFunctionPointer(const llvm::Function *f);

  std::vector<Pointer *> getPointerTarget(Pointer &p);

  std::vector<Pointer *> getColocatedPointers(Pointer &p);

  void setMemoryObjectType(const MemoryObject *mo,
                           llvm::Type *type);

  llvm::Type* getMemoryObjectType(const MemoryObject *mo);

  TransitiveTraverser traverse(Pointer *p) {
    return TransitiveTraverser(*this, p);
  }

private:

  bool mustBeTrue(ref<Expr> e) { return mustBeTrue(e, state); }
  bool mustBeTrue(ref<Expr> e, ExecutionState &s);

  bool mayBeTrue(ref<Expr> e) { return mayBeTrue(e, state); }
  bool mayBeTrue(ref<Expr> e, ExecutionState &s);

  std::vector<Pointer *> handleFunctionPtr(ref<Expr> fp);

  bool isPointerOffset(Pointer &p);

  /* TODO: add docs */
  TimingSolver &solver;
  /* TODO: add docs */
  ExecutionState &state;
  /* TODO: add docs */
  std::unordered_map<const MemoryObject *, std::vector<Pointer *>> allPointers;
  /* TODO: add docs */
  std::unordered_map<const MemoryObject *, llvm::Type *> moTypes;
  /* TODO: add docs */
  std::set<uint64_t> &legalFunctions;
  /* TODO: add docs */
  llvm::DataLayout &layout;
  /* TODO: add docs */
  OffsetFinder of;
};

}
#endif
