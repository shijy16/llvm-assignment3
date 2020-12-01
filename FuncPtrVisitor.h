#include <llvm/IR/Function.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include "Dataflow.h"
using namespace llvm;


typedef std::map<Value*,std::set<Value*>> Pointer2Set;

struct PointerInfo {
    Pointer2Set p2set;
    PointerInfo() : p2set() {}
    PointerInfo(const PointerInfo &info) : p2set(info.p2set) {}

    bool operator==(const PointerInfo &info) const {
        return p2set == info.p2set;
    }
};
/*
inline raw_ostream &operator<<(raw_ostream &out, const PointerInfo &info) {
    for (std::pair<Value *,std::set<Value*>>::iterator ii = info.p2set.begin(),
                                           ie = info.p2set.end();
         ii != ie; ++ii) {

    }
    return out;
}
*/


class FuncPtrVisitor : public DataflowVisitor<struct PointerInfo> {
   public:
    FuncPtrVisitor() {}
    void merge(PointerInfo *dest, const PointerInfo &src) override {
        for (Pointer2Set::const_iterator ii = src.p2set.begin(),
                ie = src.p2set.end();
                ii != ie; ++ii) {
            std::set<Value*> value_set = ii->second;
            for(std::set<Value*>::iterator vi = value_set.begin(),
                    ve = value_set.end();
                    vi != ve;++vi){
                dest->p2set[ii->first].insert(*vi);
            }
        }
    }

    void compDFVal(Instruction *inst, PointerInfo *dfval) override {
        /*
        if (isa<DbgInfoIntrinsic>(inst)) return;
        dfval->LiveVars.erase(inst);
        for (User::op_iterator oi = inst->op_begin(), oe = inst->op_end();
             oi != oe; ++oi) {
            Value *val = *oi;
            if (isa<Instruction>(val))
                dfval->LiveVars.insert(cast<Instruction>(val));
        }
        */
    }
};

