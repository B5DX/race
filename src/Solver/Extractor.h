#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include "IR/ExprTree.h"
#include "Solver/Graph.hpp"

namespace extract {

using Forest = vector<shared_ptr<ir::ExprTree>>;
using NodeVec = vector<shared_ptr<ir::ExprTreeNode>>;
class Node;

void extract(Forest expr_tree);

class ArrayRefDelta {
private:
    vector<shared_ptr<DeclRef>> index_vars;
    
};

class Node {
private:
    shared_ptr<ir::ExprTreeNode> arr_left;
    shared_ptr<ir::ExprTreeNode> arr_right;
};

}

#endif