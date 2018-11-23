#include "../Core/Memory.h"
#include "../Core/TimingSolver.h"
#include "../Core/Context.h"

#include "klee/Expr.h"
#include "klee/ExecutionState.h"

#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DataLayout.h"

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
 bool multiplePointers = false;

};

class SymbolicPTA {
public:
  SymbolicPTA(TimingSolver &solver, 
              ExecutionState &state, 
              llvm::DataLayout l): solver(solver), state(state), layout(l) {}

  //Gets the pointer representation of the location
  Pointer* getPointer(const MemoryObject* mo, ref<Expr> offset);
  std::vector<Pointer*> getPointerTarget(Pointer &p);
  std::vector<Pointer*> getColocatedPointers(Pointer &p);
  void giveMemoryObjectType(const MemoryObject* mo, llvm::Type*);

private:
  TimingSolver &solver;
  ExecutionState &state;
  std::unordered_map<const MemoryObject*, std::vector<Pointer*>> allPointers;
  std::unordered_map<const MemoryObject*, llvm::Type*> moTypes;
  bool mustBeTrue(ref<Expr> e);
  bool mayBeTrue(ref<Expr> e);
  llvm::Type* getMemoryObjectType(const MemoryObject* mo);

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

class OffsetFinder : public TypeVisitor<std::vector<unsigned>> {
  void visitStruct(llvm::StructType* st);
  void visitArray(llvm::ArrayType* at);
  void visitPointer(llvm::PointerType* st);
  void visitInteger(llvm::IntegerType* st);

  int globalOffset = 0;
  llvm::DataLayout &layout;
public:
  OffsetFinder(llvm::DataLayout &l): TypeVisitor<std::vector<unsigned>>(), layout(l) {}
  virtual void reset();
};


}
