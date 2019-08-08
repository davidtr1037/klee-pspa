#ifndef PTAINFO_H
#define PTAINFO_H

class PTAInfo : public AttachedInfo {

public:

    PTAInfo(const llvm::Value *allocSite);

    ~PTAInfo();

    const llvm::Value *getAllocSite();

private:

    const llvm::Value *allocSite;
};

#endif
