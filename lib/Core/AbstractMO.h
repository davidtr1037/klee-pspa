#ifndef KLEE_ABSTRACTMO_H
#define KLEE_ABSTRACTMO_H

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include <MemoryModel/PointerAnalysis.h>

#include <stdint.h>
#include <vector>

namespace klee {

struct DynamicMemoryLocation {
  const llvm::Value *value;
  size_t size;
  bool isSymbolicOffset;
  uint64_t offset;
  llvm::PointerType *hint;
  bool isVarArg;

  DynamicMemoryLocation() :
    value(NULL),
    size(0),
    isSymbolicOffset(false),
    offset(0),
    hint(0),
    isVarArg(false) {

  }

  DynamicMemoryLocation(const llvm::Value *value,
                        size_t size,
                        bool isSymbolicOffset,
                        uint64_t offset,
                        llvm::PointerType *hint,
                        bool isVarArg) :
    value(value),
    size(size),
    isSymbolicOffset(isSymbolicOffset),
    offset(offset),
    hint(hint),
    isVarArg(isVarArg) {

  }

};

NodeID computeAbstractMO(PointerAnalysis *pta,
                         DynamicMemoryLocation &location,
                         bool enforceFI,
                         bool *canStronglyUpdate = NULL);

uint32_t computeAbstractFieldOffset(uint32_t offset,
                                    const llvm::Type *moType,
                                    bool &isArrayElement);

void makeFieldInsensitive(MemObj *mem);

}

#endif
