#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include "IR/ExprTree.h"
#include "Solver/Graph.hpp"

namespace extract {

using Forest = vector<shared_ptr<ir::ExprTree>>;
using TreeNodeVec = vector<shared_ptr<ir::ExprTreeNode>>;

void extract(Forest expr_tree);

// TODO: Only work for ArrayRef. May need to support DeclRef and Number.
class DeltaNode {
  private:
    shared_ptr<ir::ExprTreeNode> arr_left;
    shared_ptr<ir::ExprTreeNode> arr_right;
    std::map<string, int> delta;

  public:
    /**
     * @brief Construct a new Delta Node object.
     * @param commutable true for commutable operator(like '+', '*'). 
     * If true, the constructor guarantees the left array will be "smaller" than the right array.
     * Else, the left and right will remain the same order as input.
     */
    DeltaNode(shared_ptr<ir::ExprTreeNode> left, shared_ptr<ir::ExprTreeNode> right, bool commutable = true);

    inline shared_ptr<ir::ExprTreeNode> getArrLeft() {
        return arr_left;
    }

    inline shared_ptr<ir::ExprTreeNode> getArrRight() {
        return arr_right;
    }

    bool operator==(const DeltaNode &other) const;

    string to_string() const;

    size_t getHash() const {
        return std::hash<string>()(this->to_string());
    }

};

} // namespace extract

#endif