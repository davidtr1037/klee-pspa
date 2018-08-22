#ifndef KLEE_PTASTATS_H
#define KLEE_PTASTATS_H

#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

#include <string>

namespace klee {

struct PTAStats {
  uint32_t queries;
  uint32_t total;
  uint32_t max_size;
  uint32_t mod_size;
  uint32_t ref_size;

  PTAStats() : queries(0), total(0), max_size(0), mod_size(0), ref_size(0) {

  }
};

struct CallingContext {
  llvm::Function *entry;
  uint32_t line;
  uint32_t call_depth;

  CallingContext() : entry(0), line(0), call_depth(0) {

  }
};

struct PTAStatsSummary {
  CallingContext context;
  uint32_t queries;
  double average_size;
  uint32_t max_size;
  uint32_t mod_size;
  uint32_t ref_size;

  PTAStatsSummary() : context(), queries(0), average_size(0), max_size(0), mod_size(0), ref_size(0) {

  }
};

class PTAStatsLogger {

public:

  virtual ~PTAStatsLogger() {

  }

  void dump(CallingContext &context, PTAStats &stats);

  virtual void dump(PTAStatsSummary &summary) = 0;

};

class PTAStatsPrintLogger : public PTAStatsLogger {

public:

  PTAStatsPrintLogger();

  virtual void dump(PTAStatsSummary &summary);

};

class PTAStatsCSVLogger : public PTAStatsLogger {

public:

  PTAStatsCSVLogger(std::string path);

  virtual ~PTAStatsCSVLogger();

  virtual void dump(PTAStatsSummary &summary);

private:
  llvm::raw_fd_ostream *file;

};

}

#endif
