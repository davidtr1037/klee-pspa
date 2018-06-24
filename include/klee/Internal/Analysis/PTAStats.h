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

  PTAStats() : queries(0), total(0), max_size(0) {

  }
};

class PTAStatsLogger {

public:

  virtual ~PTAStatsLogger() {

  }

  virtual void dump(llvm::Function *f, PTAStats &stats) = 0;

};

class PTAStatsPrintLogger : public PTAStatsLogger {

public:

  PTAStatsPrintLogger();

  virtual void dump(llvm::Function *f, PTAStats &stats);

};

class PTAStatsCSVLogger : public PTAStatsLogger {

public:

  PTAStatsCSVLogger(std::string path);

  virtual ~PTAStatsCSVLogger();

  virtual void dump(llvm::Function *f, PTAStats &stats);

private:
  llvm::raw_fd_ostream *file;

};

}

#endif
