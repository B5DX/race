#include "Solver/Extractor.h"
#include "llvm/Support/raw_ostream.h"

using namespace ir;

namespace extract {

void extract(Forest expr_tree) {
    vector<string> op_list = {"+", "-", "*", "/", "sin", "cos"};
    std::map<string, NodeVec> candidates;
    for (auto op : op_list) {
        candidates[op] = NodeVec();
    }

    for (auto tree : expr_tree) {
        tree->hoist();
        for (auto node : *tree) {
            llvm::outs() << node->isArrayRef() << "\n";
        } 
        tree->printTree();
        for (auto node : *tree) {
            if (auto op = llvm_pointer_cast<Operator>(node->getExpression())) {
                candidates[op->getOpStr()].push_back(node);
            }
        }
    }

    

    // print candidates
    for (auto &pair : candidates) {
        llvm::outs() << pair.first << ": ";
        for (auto node : pair.second) {
            llvm::outs() << node->getExpression()->to_string() << ", ";
        }
        llvm::outs() << "\n";
    }
}

}