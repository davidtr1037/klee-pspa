#include <klee/Internal/Analysis/PTAStats.h>

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

using namespace klee;
using namespace llvm;
using namespace std;


PTAStatsLogger() {

}

void PTAStatsLogger::dump(Function *f, PTAStats &stats) {
  errs() << "PTA for: " << f->getName() << "\n";
  errs() << "  -- queries: " << stats.queries << "\n";
  errs() << "  -- average size: " << float(stats.total) / float(stats.queries) << "\n";
  errs() << "  -- max size: " << stats.max_size << "\n";
}

PTAStatsCSVLogger(string path) :
  path(path) {
  error_code ec;
  file = new raw_fd_ostream(path.c_str(), ec, sys::fs::F_Append);
}

~PTAStatsCSVLogger() {
  delete file;
}

void PTAStatsCSVLogger::dump(Function *f, PTAStats &stats) {

}
