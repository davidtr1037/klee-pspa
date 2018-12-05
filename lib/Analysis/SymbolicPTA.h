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
//No public members should go through SymbolicPTA
  friend SymbolicPTA;
	friend Executor;
public:
 bool isWeak() {return weakUpdate;}
 bool isFunctionPtr() { return pointerContainer == nullptr; }
 std::string print() { 
     return isFunctionPtr() ? 
              ("fun:" + fun->getName().str()) : 
              ("mo: " + pointerContainer->name + ":" + std::to_string(offset->getZExtValue())) ; 
 }
private:
 Pointer (const MemoryObject* mo, ref<ConstantExpr> o): pointerContainer(mo), offset(o), fun(nullptr) {}
 Pointer (const llvm::Function *f): pointerContainer(nullptr), offset(nullptr), fun(f) {}
 const MemoryObject* pointerContainer; //chuck of memory where pointer can be read from
 ref<ConstantExpr> offset; //offset into the container where pointer is located
 bool multiplePointers = false;
 bool weakUpdate = false;
 const llvm::Function* fun;

};

class SymbolicPTA {

  class TransitiveTraverser {
      SymbolicPTA &s;
      Pointer *p;
  public:
    class iterator {
      friend SymbolicPTA;
      std::deque<std::pair<Pointer*, Pointer*>> ptrsToReturn;
      std::unordered_set<Pointer*> seenPointers;
      SymbolicPTA& symPTA;

      void processNext(Pointer* p);
      iterator(SymbolicPTA &s, Pointer *p); 
      //Has empty ptrsToReturn;
      iterator(SymbolicPTA &s);
    public:
      bool operator!=(const iterator& other);
      bool operator==(const iterator& other);
      void operator++();
      std::pair<Pointer*, Pointer*>& operator*();
      std::pair<Pointer*, Pointer*>* operator->();
    };
    TransitiveTraverser(SymbolicPTA &_s, Pointer *_p): s(_s), p(_p) {}
    iterator begin() const { return iterator(s,p); }
    iterator end() const { return iterator(s); }

  };

public:
  SymbolicPTA(TimingSolver &solver, 
              ExecutionState &state, 
              std::set<uint64_t> &lf,
              llvm::DataLayout l): solver(solver), state(state), legalFunctions(lf), layout(l) {}
  ~SymbolicPTA();

  //Gets the pointer representation of the location
  Pointer* getPointer(const MemoryObject* mo, ref<Expr> offset);
  Pointer* getFunctionPointer(const llvm::Function* f);
  std::vector<Pointer*> getPointerTarget(Pointer &p);
  std::vector<Pointer*> getColocatedPointers(Pointer &p);
  void giveMemoryObjectType(const MemoryObject* mo, llvm::Type*);
  llvm::Type* getMemoryObjectType(const MemoryObject* mo);
  TransitiveTraverser traverse(Pointer *p) { return TransitiveTraverser(*this,p); }
  //TODO: dtor

private:
  TimingSolver &solver;
  ExecutionState &state;
  std::unordered_map<const MemoryObject*, std::vector<Pointer*>> allPointers;
  std::unordered_map<const MemoryObject*, llvm::Type*> moTypes;
  bool mustBeTrue(ref<Expr> e);
  bool mayBeTrue(ref<Expr> e);
  std::vector<Pointer*> handleFunctionPtr(ref<Expr> fp);

  bool isPointerOffset(Pointer& p);
  std::set<uint64_t>& legalFunctions;
  llvm::DataLayout &layout;
};

template <class T>
class TypeVisitor {
  int visitCount = 0;
public:
  T visit(llvm::Type* t);
  virtual void reset() = 0;

protected:
  T results;
  virtual void visitStruct(llvm::StructType* st) = 0;
  virtual void visitArray(llvm::ArrayType* at) = 0;
  virtual void visitPointer(llvm::PointerType* st) = 0;
  virtual void visitInteger(llvm::IntegerType* st) = 0;

};

class OffsetFinder : public TypeVisitor<std::vector<std::pair<unsigned, bool>>> {
  void visitStruct(llvm::StructType* st);
  void visitArray(llvm::ArrayType* at);
  void visitPointer(llvm::PointerType* st);
  void visitInteger(llvm::IntegerType* st);

  int globalOffset = 0;
  bool weakUpdate = false;
  llvm::DataLayout &layout;
public:
  OffsetFinder(llvm::DataLayout &l): TypeVisitor<std::vector<std::pair<unsigned, bool>>>(), layout(l) {}
  virtual void reset();
};


}
#endif
