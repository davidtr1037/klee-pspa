#ifndef KLEE_PTAGRAPH_H
#define KLEE_PTAGRAPH_H

#include <WPA/AndersenDynamic.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

namespace klee {

class BasePTAGraphDumper {

public:

  BasePTAGraphDumper() {

  }

  virtual void dump(AndersenDynamic *pta);

  virtual void handle(AndersenDynamic *pta,
                      NodeID src,
                      PointsTo &pts);

};


class PTAGraphDumper : public BasePTAGraphDumper {

public:

  PTAGraphDumper(llvm::raw_ostream &log) :
    log(log) {

  }

  void dump(AndersenDynamic *pta);

  void handle(AndersenDynamic *pta,
              NodeID src,
              PointsTo &pts);

  void dumpDebugInfo(AndersenDynamic *pta,
                     NodeID nodeId);

  void dumpObjDebugInfo(AndersenDynamic *pta,
                        ObjPN *obj);

  void dumpValDebugInfo(AndersenDynamic *pta,
                        ValPN *val);

private:

  llvm::raw_ostream &log;

};

}

#endif
