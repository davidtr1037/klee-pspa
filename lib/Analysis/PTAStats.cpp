#include <klee/Internal/Analysis/PTAStats.h>

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

using namespace klee;
using namespace llvm;
using namespace std;


void PTAStatsLogger::dump(Function *f, PTAStats &stats) {
  PTAStatsSummary summary;

  summary.f = f;
  summary.queries = stats.queries;
  summary.average_size = stats.queries > 0 ? double(stats.total) / double(stats.queries) : 0;
  summary.max_size = stats.max_size;
  dump(summary);
}

PTAStatsPrintLogger::PTAStatsPrintLogger() {

}

void PTAStatsPrintLogger::dump(PTAStatsSummary &summary) {
  errs() << "PTA for: " << summary.f->getName() << "\n";
  errs() << "  -- queries: " << summary.queries << "\n";
  errs() << "  -- average size: " << summary.average_size << "\n";
  errs() << "  -- max size: " << summary.max_size << "\n";
}

PTAStatsCSVLogger::PTAStatsCSVLogger(string path) {
  error_code ec;
  sys::fs::file_status result;

  /* check file status before opening the file... */
  sys::fs::status(path, result);

  file = new raw_fd_ostream(path.c_str(), ec, sys::fs::F_Append);
  if (ec) {
    assert(false);
  }

  if (!sys::fs::exists(result)) {
    *file << "Function,Queries,Average,Max\n";
  }
}

PTAStatsCSVLogger::~PTAStatsCSVLogger() {
  if (file) {
    delete file;
  }
}

void PTAStatsCSVLogger::dump(PTAStatsSummary &summary) {
  *file << summary.f->getName() << ",";
  *file << summary.queries << ",";
  *file << summary.average_size << ",";
  *file << summary.max_size << "\n";
}
