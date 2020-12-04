/************************************************************************
 *
 * @file Dataflow.h
 *
 * General dataflow framework
 *
 ***********************************************************************/

#ifndef _DATAFLOW_H_
#define _DATAFLOW_H_

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

#include <map>

using namespace llvm;

///
/// Dummy class to provide a typedef for the detailed result set
/// For each basicblock, we compute its input dataflow val and its output
/// dataflow val
///
template <class T>
struct DataflowResult {
    typedef typename std::map<BasicBlock *, std::pair<T, T> > Type;
};

template <class T>
struct DataflowInsResult {
    typedef typename std::map<Instruction *, std::pair<T, T> > Type;
};

/// Base dataflow visitor class, defines the dataflow function

template <class T>
class DataflowVisitor {
   public:
    virtual ~DataflowVisitor() {}

    /// Dataflow Function invoked for each basic block
    ///
    /// @block the Basic Block
    /// @dfval the input dataflow value
    /// @isforward true to compute dfval forward, otherwise backward
    virtual void compDFVal(BasicBlock *block, T *dfval, bool isforward) {
        if (isforward == true) {
            for (BasicBlock::iterator ii = block->begin(), ie = block->end();
                 ii != ie; ++ii) {
                Instruction *inst = &*ii;
                compDFVal(inst, dfval);
            }
        } else {
            for (BasicBlock::reverse_iterator ii = block->rbegin(),
                                              ie = block->rend();
                 ii != ie; ++ii) {
                Instruction *inst = &*ii;
                compDFVal(inst, dfval);
            }
        }
    }
    
    virtual void compDFVal(BasicBlock *block, typename DataflowInsResult<T>::Type *dfval, bool isforward) {
        if (isforward == true) {
            for (BasicBlock::iterator ii = block->begin(), ie = block->end();
                 ii != ie; ++ii) {
                Instruction *inst = &*ii;
                compDFVal(inst, dfval);
            }
        } else {
            for (BasicBlock::reverse_iterator ii = block->rbegin(),
                                              ie = block->rend();
                 ii != ie; ++ii) {
                Instruction *inst = &*ii;
                compDFVal(inst, dfval);
            }
        }
    }

    virtual void mergeInputDF(Function* func,BasicBlock* block,T* bbinval){};
    ///
    /// Dataflow Function invoked for each instruction
    ///
    /// @inst the Instruction
    /// @dfval the input dataflow value
    /// @return true if dfval changed
    virtual void compDFVal(Instruction *inst, T *dfval) {};
    virtual void compDFVal(Instruction *inst, typename DataflowInsResult<T>::Type *dfval) {};

    ///
    /// Merge of two dfvals, dest will be ther merged result
    /// @return true if dest changed
    ///
    virtual void merge(T *dest, const T &src) = 0;
};



Instruction* getFisrtIns(BasicBlock* block){
    Instruction* ins = &*(block->begin());
    return ins;
}

Instruction* getLastIns(BasicBlock* block){
    Instruction* ins = &*(--(block->end()));
    return ins;
}

///
/// Compute a forward iterated fixedpoint dataflow function, using a
/// user-supplied visitor function. Note that the caller must ensure that the
/// function is in fact a monotone function, as otherwise the fixedpoint may not
/// terminate.
///
/// @param fn The function
/// @param visitor A function to compute dataflow vals
/// @param result The results of the dataflow
/// @initval the Initial dataflow value
/*
template <class T>
void compForwardDataflow(Function *fn, DataflowVisitor<T> *visitor,
                         typename DataflowInsResult<T>::Type *result,
                         const T &initval) {
    std::set<BasicBlock *> worklist;
    for (Function::iterator bi = fn->begin(); bi != fn->end(); ++bi) {
        BasicBlock *bb = &*bi;
        worklist.insert(bb);
        for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii != ie; ++ii){
            Instruction *ins = &*ii;
            result->insert(std::make_pair(ins, std::make_pair(initval, initval)));
        }
    }
    // Iteratively compute the dataflow result
    while (!worklist.empty()) {
        BasicBlock *bb = *worklist.begin();
        worklist.erase(worklist.begin());

        // Merge all incoming value
        T bbinval = (*result)[getFisrtIns(bb)].first;
        //合并所有前继基本块的out
        for (auto pi = pred_begin(bb), pe = pred_end(bb); pi != pe; pi++) {
            visitor->merge(&bbinval, (*result)[getLastIns(*pi)].second);
        }

        T bbexitval = (*result)[getLastIns(bb)].second;
        (*result)[getFisrtIns(bb)].first = bbinval;
        
        //计算一遍基本块内控制流
        visitor->compDFVal(bb, result, false);

        //算出来最后一个指令的out pointer2set变了的话，所有后继都要重算
        if (bbexitval == (*result)[getLastIns(bb)].second) continue;

        for (succ_iterator si = succ_begin(bb), se = succ_end(bb); si != se;
             si++) {
            worklist.insert(*si);
        }
        
    }
    return;
}
*/
template <class T>
void compForwardDataflow(Function *fn, DataflowVisitor<T> *visitor,
                         typename DataflowResult<T>::Type *result,
                         const T &initval) {
    std::set<BasicBlock *> worklist;
    for (Function::iterator bi = fn->begin(); bi != fn->end(); ++bi) {
        BasicBlock *bb = &*bi;
        worklist.insert(bb);
        result->insert(std::make_pair(bb, std::make_pair(initval, initval)));
    }
    // Iteratively compute the dataflow result
    while (!worklist.empty()) {
        BasicBlock *bb = *worklist.begin();
        worklist.erase(worklist.begin());
        // Merge all incoming value
        T bbinval = (*result)[bb].first;
        //如果是函数第一个块，去拿它所有参数的pts然后合并
        if(bb == &fn->getEntryBlock()){
            errs()<< "---------" <<fn->getName() << "-----\n";
            visitor->mergeInputDF(fn,bb,&bbinval);
        } else {
        //否则合并所有前继块
            for (auto pi = pred_begin(bb), pe = pred_end(bb); pi != pe; pi++) {
                visitor->merge(&bbinval, (*result)[*pi].second);
            }
        }
        T old_bbexitval = (*result)[bb].second;
        (*result)[bb].first = bbinval;
        //计算一遍基本块内控制流
        visitor->compDFVal(bb,&bbinval, true);

        //算出来最后一个out pointer2set变了的话，所有后继都要重算
        if (bbinval == (*result)[bb].second) continue;
        (*result)[bb].second = bbinval;

        for (succ_iterator si = succ_begin(bb), se = succ_end(bb); si != se;
             si++) {
            worklist.insert(*si);
        }
    }
    return;
}
///
/// Compute a backward iterated fixedpoint dataflow function, using a
/// user-supplied visitor function. Note that the caller must ensure that the
/// function is in fact a monotone function, as otherwise the fixedpoint may not
/// terminate.
///
/// @param fn The function
/// @param visitor A function to compute dataflow vals
/// @param result The results of the dataflow
/// @initval The initial dataflow value
template <class T>
void compBackwardDataflow(Function *fn, DataflowVisitor<T> *visitor,
                          typename DataflowResult<T>::Type *result,
                          const T &initval) {
    std::set<BasicBlock *> worklist;

    // Initialize the worklist with all exit blocks
    for (Function::iterator bi = fn->begin(); bi != fn->end(); ++bi) {
        BasicBlock *bb = &*bi;
        result->insert(std::make_pair(bb, std::make_pair(initval, initval)));
        worklist.insert(bb);
    }

    // Iteratively compute the dataflow result
    while (!worklist.empty()) {
        BasicBlock *bb = *worklist.begin();
        worklist.erase(worklist.begin());

        // Merge all incoming value
        T bbexitval = (*result)[bb].second;
        for (auto si = succ_begin(bb), se = succ_end(bb); si != se; si++) {
            BasicBlock *succ = *si;
            visitor->merge(&bbexitval, (*result)[succ].first);
        }

        (*result)[bb].second = bbexitval;
        visitor->compDFVal(bb, &bbexitval, false);

        // If outgoing value changed, propagate it along the CFG
        if (bbexitval == (*result)[bb].first) continue;
        (*result)[bb].first = bbexitval;

        for (pred_iterator pi = pred_begin(bb), pe = pred_end(bb); pi != pe;
             pi++) {
            worklist.insert(*pi);
        }
    }
}

template <class T>
void printDataflowResult(raw_ostream &out,
                         const typename DataflowResult<T>::Type &dfresult) {
    for (typename DataflowResult<T>::Type::const_iterator it = dfresult.begin();
         it != dfresult.end(); ++it) {
        if (it->first == NULL)
            out << "*";
        else
            it->first->dump();
        out << "\n\tin : " << it->second.first
            << "\n\tout :  " << it->second.second << "\n";
    }
}

#endif /* !_DATAFLOW_H_ */
