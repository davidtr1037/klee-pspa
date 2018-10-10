#ifndef KLEE_MODULARANALYSIS_H
#define KLEE_MODULARANALYSIS_H

#include <klee/util/Ref.h>

#include <MemoryModel/PointerAnalysis.h>
#include <WPA/AndersenDynamic.h>

#include <llvm/IR/Function.h>

#include <vector>
#include <set>


namespace klee {

struct Parameter {
  NodeID nodeId;
  unsigned int index;

  Parameter(NodeID nodeId, unsigned int index) :
    nodeId(nodeId), index(index) {

  }
};

struct SubstitutionInfo {
  std::map<NodeID, NodeID> mapping;
};

class EntryState {

public:

  EntryState() : pta(0) {

  }

  void setPTA(ref<AndersenDynamic> pta) {
    this->pta = pta;
  }

  void addParameter(NodeID nodeId, unsigned int index) {
    parameters.push_back(Parameter(nodeId, index));
  }

  ref<AndersenDynamic> pta;
  /* assumed to be orderd by the index */
  std::vector<Parameter> parameters;

};

struct ModResult {
  EntryState entryState;
  std::set<NodeID> mod;
};

class ModularPTA {

public:

  ModularPTA() {

  }

  ~ModularPTA() {

  }

  void update(llvm::Function *f,
              EntryState &entryState,
              std::set<NodeID> &mod);

  bool computeModSet(llvm::Function *f,
                     EntryState &entryState,
                     std::set<NodeID> &result);

  bool checkIsomorphism(EntryState &es1,
                        EntryState &es2,
                        SubstitutionInfo &info);

  bool checkIsomorphism(EntryState &es1,
                        NodeID n1,
                        EntryState &es2,
                        NodeID n2,
                        SubstitutionInfo &info);

private:

  typedef std::vector<ModResult> FunctionCache;
  typedef std::map<llvm::Function *, FunctionCache> Cache;

  Cache cache;
};

}

#endif
