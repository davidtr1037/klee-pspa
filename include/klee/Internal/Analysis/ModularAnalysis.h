#ifndef KLEE_MODULARANALYSIS_H
#define KLEE_MODULARANALYSIS_H

#include <klee/util/Ref.h>
#include "klee/Internal/Analysis/StateProjection.h"

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

  void dump() {
    llvm::errs() << "Substitution:\n";
    for (auto i : mapping) {
      llvm::errs() << i.first << " / " << i.second << "\n";
    }
  }
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
  /* globals */
  std::set<NodeID> usedGlobals;

};

struct AnalysisResult {

  AnalysisResult(EntryState &entryState,
                 StateProjection &projection) :
    entryState(entryState),
    projection(projection) {

  }

  EntryState entryState;
  StateProjection projection;
};

class ModularPTA {

public:

  ModularPTA() {

  }

  ~ModularPTA() {

  }

  void update(llvm::Function *f,
              unsigned int line,
              EntryState &entryState,
              StateProjection &projection);

  bool computeModSet(llvm::Function *f,
                     unsigned int line,
                     EntryState &entryState,
                     StateProjection &projection);

  bool checkIsomorphism(EntryState &es1,
                        EntryState &es2,
                        SubstitutionInfo &info);

  bool checkIsomorphism(EntryState &es1,
                        NodeID n1,
                        EntryState &es2,
                        NodeID n2,
                        SubstitutionInfo &info);

  bool getMatchedNodes(EntryState &es1,
                       const NodeBS &subNodes1,
                       EntryState &es2,
                       const NodeBS &subNodes2,
                       std::vector<NodeID> &ordered1,
                       std::vector<NodeID> &ordered2);

  void filterNodes(EntryState &es, const NodeBS &nodes, NodeBS &result);

  void substitute(EntryState &es1,
                  EntryState &es2,
                  SubstitutionInfo &info,
                  StateProjection &cached,
                  StateProjection &result);

  NodeID substituteNode(EntryState &es1,
                        EntryState &es2,
                        SubstitutionInfo &info,
                        NodeID nodeId);

  void dump(EntryState &es);

  void dump(EntryState &es, NodeID nodeId, unsigned level = 0);

  void dump(EntryState &es, NodeID nodeId, std::set<NodeID> &visited, unsigned level);

private:

  typedef std::pair<llvm::Function *, unsigned int> Context;
  typedef std::vector<AnalysisResult> AnalysisResults;
  typedef std::map<Context, AnalysisResults> Cache;

  Cache cache;
};

}

#endif
