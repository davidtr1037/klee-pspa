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
  uint64_t offset;
  llvm::PointerType *hint;

  DynamicMemoryLocation() :
    value(NULL), offset(0), hint(0) {

  }

  DynamicMemoryLocation(const llvm::Value *value,
                        uint64_t offset,
                        llvm::PointerType *hint) :
    value(value), offset(offset), hint(hint) {

  }

};

NodeID computeAbstractMO(PointerAnalysis *pta,
                         DynamicMemoryLocation &location,
                         bool *canStronglyUpdate = NULL);

uint32_t computeAbstractFieldOffset(uint32_t offset,
                                    const llvm::Type *moType,
                                    bool &isArray);

}

#endif
