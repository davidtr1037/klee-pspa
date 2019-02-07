#include "klee/Internal/Analysis/ModularAnalysis.h"
#include "klee/Internal/Analysis/PTAUtils.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>

using namespace llvm;
using namespace klee;
using namespace std;


void ModularPTA::update(Function *f,
                        unsigned int line,
                        EntryState &entryState,
                        StateProjection &projection) {
  auto k = std::make_pair(f, line);
  AnalysisResults &results = cache[k];
  AnalysisResult result(entryState, projection);
  results.push_back(result);
}

bool ModularPTA::computeModSet(Function *f,
                               unsigned int line,
                               EntryState &entryState,
                               StateProjection &projection) {
  auto k = std::make_pair(f, line);
  AnalysisResults &results = cache[k];
  for (AnalysisResult &result : results) {
    SubstitutionInfo info;
    if (checkIsomorphism(result.entryState, entryState, info)) {
      substitute(result.entryState,
                 entryState,
                 info,
                 result.projection,
                 projection);
      return true;
    }
  }

  return false;
}

bool ModularPTA::checkIsomorphism(EntryState &es1,
                                  EntryState &es2,
                                  SubstitutionInfo &info) {
  if (es1.parameters.size() != es2.parameters.size()) {
    return false;
  }

  for (unsigned int i = 0; i < es1.parameters.size(); i++) {
    Parameter p1 = es1.parameters[i];
    Parameter p2 = es2.parameters[i];
    if (p1.index != p2.index) {
      return false;
    }

    if (!checkIsomorphism(es1, p1.nodeId, es2, p2.nodeId, info)) {
      return false;
    }
  }

  /* es1 is the cached one... */
  for (NodeID n : es1.usedGlobals) {
    if (!checkIsomorphism(es1, n, es2, n, info)) {
      return false;
    }
  }

  return true;
}

bool ModularPTA::checkIsomorphism(EntryState &es1,
                                  NodeID n1,
                                  EntryState &es2,
                                  NodeID n2,
                                  SubstitutionInfo &info) {
  list<pair<NodeID, NodeID>> worklist;
  set<pair<NodeID, NodeID>> checked;

  /* initialize list */
  worklist.push_back(make_pair(n1, n2));

  while (!worklist.empty()) {
    auto p = worklist.front();
    worklist.pop_front();

    if (checked.find(p) != checked.end()) {
      continue;
    }

    NodeID n1 = p.first;
    NodeID n2 = p.second;

    if (!es1.pta->getPAG()->findPAGNode(n1)) {
      if (es2.pta->getPAG()->findPAGNode(n2)) {
        return false;
      }
      continue;
    }

    if (!es2.pta->getPAG()->findPAGNode(n2)) {
      if (es1.pta->getPAG()->findPAGNode(n1)) {
        return false;
      }
      continue;
    }

    PAGNode *pagNode;
    pagNode = es1.pta->getPAG()->getPAGNode(n1);
    ObjPN *obj1 = dyn_cast<ObjPN>(pagNode);
    pagNode = es2.pta->getPAG()->getPAGNode(n2);
    ObjPN *obj2 = dyn_cast<ObjPN>(pagNode);

    if (isa<FIObjPN>(obj1)) {
      /* in this case, we restrict both of the objects to be field-insensitive */
      if (!isa<FIObjPN>(obj2)) {
        return false;
      }
    }

    if (isa<GepObjPN>(obj1)) {
      /* in this case, we restrict both of the objects to be field-sensitive */
      GepObjPN *gep1 = dyn_cast<GepObjPN>(obj1);
      GepObjPN *gep2 = dyn_cast<GepObjPN>(obj2);
      if (!gep2) {
        return false;
      }

      /* currently we require that the offsets must be the same, but actually
         the objects can be isomorphic event if they don't have the same offsets */
      if (gep1->getLocationSet().getOffset() != gep2->getLocationSet().getOffset()) {
        return false;
      }
    }

    const NodeBS &subNodes1 = es1.pta->getAllFieldsObjNode(n1);
    const NodeBS &subNodes2 = es2.pta->getAllFieldsObjNode(n2);

    vector<NodeID> ordered1, ordered2;
    if (!getMatchedNodes(es1, subNodes1, es2, subNodes2, ordered1, ordered2)) {
      return false;
    }

    if (ordered1.size() != ordered2.size()) {
      assert(false);
    }

    for (unsigned i = 0; i < ordered1.size(); i++) {
      NodeID s1 = ordered1[i];
      NodeID s2 = ordered2[i];

      PointsTo &pts1 = es1.pta->getPts(s1);
      PointsTo &pts2 = es2.pta->getPts(s2);

      if (pts1.count() != pts2.count()) {
        return false;
      }

      std::vector<NodeID> ptsList1;
      std::vector<NodeID> ptsList2;
      for (NodeID x : pts1) {
        ptsList1.push_back(x);
      }
      for (NodeID x : pts2) {
        ptsList2.push_back(x);
      }
      for (unsigned int i = 0; i < ptsList1.size(); i++) {
        NodeID p1 = ptsList1[i];
        NodeID p2 = ptsList2[i];
        worklist.push_back(make_pair(p1, p2));
      }
    }

    /* update the substitution mapping */
    NodeID base1 = es1.pta->getBaseObjNode(n1);
    NodeID base2 = es2.pta->getBaseObjNode(n2);
    if (info.mapping.find(base1) != info.mapping.end()) {
      NodeID prev = info.mapping[base1];
      if (prev != base2) {
        /* a base node can be translated only to one value */
        return false;
      }
    } else {
      info.mapping[base1] = base2;
    }

    /* TODO: is it the right place? */
    /* update checked pairs */
    checked.insert(p);
  }

  return true;
}

bool ModularPTA::getMatchedNodes(EntryState &es1,
                                 const NodeBS &subNodes1,
                                 EntryState &es2,
                                 const NodeBS &subNodes2,
                                 std::vector<NodeID> &ordered1,
                                 std::vector<NodeID> &ordered2) {
  PAGNode *pagNode;
  NodeBS filtered1;
  NodeBS filtered2;

  filterNodes(es1, subNodes1, filtered1);
  filterNodes(es2, subNodes2, filtered2);

  if (filtered1.count() != filtered2.count()) {
    /* it is restrictive, but is it correct? */
    return false;
  }

  for (NodeID s1 : filtered1) {
    pagNode = es1.pta->getPAG()->getPAGNode(s1);
    ObjPN *subObj1 = dyn_cast<ObjPN>(pagNode);
    if (isa<FIObjPN>(subObj1)) {
      bool found = false;
      FIObjPN *fiObj1 = dyn_cast<FIObjPN>(subObj1);

      for (NodeID s2 : filtered2) {
        pagNode = es2.pta->getPAG()->getPAGNode(s2);
        FIObjPN *fiObj2 = dyn_cast<FIObjPN>(pagNode);
        if (fiObj2) {
          ordered1.push_back(fiObj1->getMemObj()->getSymId());
          ordered2.push_back(fiObj2->getMemObj()->getSymId());
          found = true;
          break;
        }
      }

      if (!found) {
        return false;
      }
    }

    if (isa<GepObjPN>(subObj1)) {
      bool found = false;
      GepObjPN *subGep1 = dyn_cast<GepObjPN>(subObj1);
      size_t offset1 = subGep1->getLocationSet().getOffset();

      for (NodeID s2 : subNodes2) {
        pagNode = es2.pta->getPAG()->getPAGNode(s2);
        GepObjPN *subGep2 = dyn_cast<GepObjPN>(pagNode);
        if (!subGep2) {
          continue;
        }
        size_t offset2 = subGep2->getLocationSet().getOffset();
        if (offset1 == offset2) {
          ordered1.push_back(s1);
          ordered2.push_back(s2);
          found = true;
          break;
        }
      }

      if (!found) {
        return false;
      }
    }
  }

  return true;
}

void ModularPTA::filterNodes(EntryState &es,
                             const NodeBS &nodes,
                             NodeBS &result) {
  for (NodeID n : nodes) {
    PointsTo &pts = es.pta->getPts(n);
    if (pts.count() == 0) {
       continue;
    }
    if (pts.count() == 1) {
        NodeID x = *pts.begin();
        if (x == es.pta->getPAG()->getNullPtr()) {
            continue;
        }
    }
    result.set(n);
  }
}

void ModularPTA::substitute(EntryState &es1,
                            EntryState &es2,
                            SubstitutionInfo &info,
                            StateProjection &cached,
                            StateProjection &result) {
  for (auto i : cached.pointsToMap) {
    NodeID n1 = i.first;
    PointsTo &pts1 = i.second;

    if (!es1.pta->getPAG()->findPAGNode(n1)) {
      /* TODO: add docs */
      continue;
    }

    NodeID n2 = substituteNode(es1, es2, info, n1);
    PointsTo &pts2 = result.pointsToMap[n2];
    for (NodeID v1 : pts1) {
      if (!es1.pta->getPAG()->findPAGNode(v1)) {
        /* TODO: add docs */
        continue;
      }
      NodeID v2 = substituteNode(es1, es2, info, v1);
      pts2.set(v2);
    }
  }
}

NodeID ModularPTA::substituteNode(EntryState &es1,
                                  EntryState &es2,
                                  SubstitutionInfo &info,
                                  NodeID nodeId) {
  NodeID base1 = es1.pta->getBaseObjNode(nodeId);
  if (info.mapping.find(base1) == info.mapping.end()) {
    /* TODO: is it always correct? */
    return nodeId;
  }

  /* get corresponding node */
  NodeID base2 = info.mapping[base1];

  PAGNode *pagNode = es1.pta->getPAG()->getPAGNode(nodeId);
  ObjPN *obj = dyn_cast<ObjPN>(pagNode);
  if (!obj) {
    assert(false);
  }

  if (isa<FIObjPN>(obj)) {
    return es2.pta->getFIObjNode(base2);
  }

  if (isa<GepObjPN>(obj)) {
    GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
    return es2.pta->getGepObjNode(base2, gepObj->getLocationSet());
  }

  if (obj->getMemObj()->isConstantObj()) {
    return nodeId;
  }

  if (obj->getMemObj()->isBlackHoleObj()) {
    return nodeId;
  }

  /* should be unreachable... */
  assert(false);
}

void ModularPTA::dump(EntryState &es) {
  errs() << "### Entry state ###\n";

  for (NodeID n : es.usedGlobals) {
    errs() << "// global\n";
    dump(es, n);
  }

  for (unsigned int i = 0; i < es.parameters.size(); i++) {
    Parameter p = es.parameters[i];
    errs() << "// parameter " << p.index << ":\n";
    dump(es, p.nodeId);
  }
}

void ModularPTA::dump(EntryState &es, NodeID nodeId, unsigned level) {
  std::set<NodeID> visited;
  dump(es, nodeId, visited, level);
}

void ModularPTA::dump(EntryState &es, NodeID nodeId, std::set<NodeID> &visited, unsigned level) {
  std::string indent = std::string(level * 4, ' ');
  if (visited.find(nodeId) != visited.end()) {
    errs() << indent << "// loop " << nodeId << "\n";
    return;
  }

  visited.insert(nodeId);
  errs() << indent << "// node:\n";
  dumpNodeInfo(es.pta.get(), nodeId, level * 4);

  const NodeBS &subNodes = es.pta->getAllFieldsObjNode(nodeId);
  for (NodeID subNode : subNodes) {
    visited.insert(subNode);
    errs() << indent << "// sub node:\n";
    dumpNodeInfo(es.pta.get(), subNode, level * 4);

    PointsTo &pts = es.pta->getPts(subNode);
    for (NodeID p : pts) {
      dump(es, p, visited, level + 1);
    }
  }
}
