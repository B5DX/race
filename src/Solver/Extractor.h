#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include "IR/ExprTree.h"
#include "Solver/Graph.hpp"

namespace extract {

using Forest = vector<shared_ptr<ir::ExprTree>>;
using TreeNodeVec = vector<shared_ptr<ir::ExprTreeNode>>;
class Node;

void extract(Forest expr_tree);

// TODO: Only work for ArrayRef. Need to support DeclRef and Number.
class Node {
  private:
    shared_ptr<ir::ExprTreeNode> arr_left;
    shared_ptr<ir::ExprTreeNode> arr_right;
    std::map<string, int> delta;

  public:
    Node(shared_ptr<ir::ExprTreeNode> left, shared_ptr<ir::ExprTreeNode> right);

    inline shared_ptr<ir::ExprTreeNode> getArrLeft() {
        return arr_left;
    }

    inline shared_ptr<ir::ExprTreeNode> getArrRight() {
        return arr_right;
    }

    bool operator==(const Node &other) const;

    string to_string() const;
};

} // namespace extract

#endif