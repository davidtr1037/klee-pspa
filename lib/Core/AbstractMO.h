#ifndef KLEE_ABSTRACTMO_H
#define KLEE_ABSTRACTMO_H

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include <MemoryModel/PointerAnalysis.h>

#include <stdint.h>

namespace klee {

struct DynamicMemoryLocation {
  const llvm::Value *value;
  uint64_t offset;
};

NodeID computeAbstractMO(PointerAnalysis *pta,
                         DynamicMemoryLocation &location);

uint32_t computeAbstractFieldOffset(uint32_t offset,
                                    const llvm::Type *moType);

}

#endif
