#ifndef KLEE_ANDERSENSYMBOLIC_H
#define KLEE_ANDERSENSYMBOLIC_H

#include <WPA/Andersen.h>
#include <WPA/DynamicAndersenBase.h>

#include <llvm/IR/Module.h>

namespace klee {

class DynamicAndersen : public DynamicAndersenBase {
public:

    DynamicAndersen(llvm::Module &module) : module(module) {

    }

    void setup();

private:

    llvm::Module &module;
};

}

#endif
