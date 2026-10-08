#include "IR/ExprTree.h"
#include "llvm/Support/raw_ostream.h"
#include <queue>
using std::deque;

using namespace ir;

void ExprTreeNode::hoist() {
    if (!isOperatorNode()) return;
    const auto op = to_string();
    vector<shared_ptr<ExprTreeNode>> flattened;
    for (auto child : children) {
        child->hoist();
        if ((op == "+" || op == "*") && child->isOperatorNode() && child->to_string() == op) {
            for (auto grandchild : child->getChildren()) {
                grandchild->setParent(weak_from_this());
                flattened.push_back(grandchild);
            }
        } else flattened.push_back(child);
    }
    children = std::move(flattened);
}

void ExprTreeNode::deposit() {
    size_t children_len = children.size();
    if (children_len == 0)
        return;
    // Deposit children first
    for (size_t i = 0; i < children_len; i++) {
        children[i]->deposit();
    }
    // Copy children to queue
    deque<shared_ptr<ExprTreeNode>> node_queue;
    for (auto child : children) {
        node_queue.push_back(child);
    }

    if (node_queue.size() == 1) { // Only one child, this node should be a unary operator.
        auto child = node_queue.front();
        if (auto uop = llvm_pointer_cast<UnaryOp>(src)) {
            uop->setOperand(child->getExpression());
        } else {
            throw std::runtime_error("Invalid unary expression-tree node");
        }
        return;
    }

    // Only binary operator can reach here.
    // Build new binary tree. The last one node in the queue is the new root, which will replace this node.
    while (node_queue.size() > 1) {
        auto node1 = node_queue.front();
        node_queue.pop_front();
        auto node2 = node_queue.front();
        node_queue.pop_front();
        shared_ptr<BinaryOp> biop;
        biop = llvm_pointer_cast<BinaryOp>(src->clone());
        biop->setLHS(node1->getExpression());
        biop->setRHS(node2->getExpression());
        shared_ptr<ExprTreeNode> new_expr_node;
        new_expr_node = make_shared<ExprTreeNode>(biop, shared_from_this());
        node1->setParent(new_expr_node);
        node2->setParent(new_expr_node);
        new_expr_node->addChild(node1);
        new_expr_node->addChild(node2);
        node_queue.push_front(new_expr_node);
    }
    // Copy new root's info to replace this node.
    children.clear();
    auto new_root = node_queue.front();
    for (auto child : new_root->getChildren()) {
        children.push_back(child);
        child->setParent(weak_from_this());
    }
    src = new_root->getExpression();
}

size_t ExprTreeNode::eraseChild(shared_ptr<ExprTreeNode> child) {
    if (!isOperatorNode()) {
        throw std::runtime_error("Cannot remove a child from a leaf expression");
    }
    auto res = std::find(children.begin(), children.end(), child);
    if (res == children.end()) {
        throw std::runtime_error("Cannot remove an unknown expression-tree child");
    }
    children.erase(res);
    return res - children.begin();
}

void ExprTreeNode::printTree(int depth) {
    if (children.size() == 0)
        return;

    for (int i = 0; i < depth; i++)
        llvm::outs() << "-";

    llvm::outs() << this->to_string() << ": [";
    if (this->isOperatorNode()) {
        for (auto child : children) {
            llvm::outs() << child->to_string() << ",";
        }
        llvm::outs() << "]\n";
        for (auto child : children) {
            child->printTree(depth + 1);
        }
    }
}

void ExprTree::buildTree(shared_ptr<ExprTreeNode> root, shared_ptr<ir::Expression> e) {
    if (auto biop = llvm_pointer_cast<BinaryOp>(e)) {
        root->addChild(biop->getLHS());
        root->addChild(biop->getRHS());
        buildTree(root->getChildren().front(), biop->getLHS());
        buildTree(root->getChildren().back(), biop->getRHS());
    } else if (auto uop = llvm_pointer_cast<UnaryOp>(e)) {
        if (llvm_isa<ir::SinOp>(uop) || llvm_isa<ir::CosOp>(uop))
            return;
        root->addChild(uop->getOperand());
        buildTree(root->getChildren().front(), uop->getOperand());
    } else {
        return;
    }
}

void ExprTree::replaceArf2Scalar(shared_ptr<ArrayRef> arf, shared_ptr<DeclRef> scalar) {
    for (auto iter = this->begin(); iter != this->end(); ++iter) {
        auto node = *iter;
        if (auto cur_arf = llvm_pointer_cast<ArrayRef>(node->getExpression())) {
            if (cur_arf->to_string() == arf->to_string()) {
                node->setExpression(scalar);
            }
        }
    }
    this->deposit();
}

vector<shared_ptr<ExprTreeNode>> ExprTree::getArrayRefs() {
    vector<shared_ptr<ExprTreeNode>> res;
    for (auto iter = this->begin(); iter != this->end(); ++iter) {
        if (llvm_isa<ir::ArrayRef>(iter->getExpression())) {
            res.push_back(*iter);
        }
    }
    return res;
}

vector<shared_ptr<ExprTreeNode>> ExprTree::collectNodes(std::function<bool(shared_ptr<ExprTreeNode>)> pred) {
    vector<shared_ptr<ExprTreeNode>> res;
    for (auto iter = this->begin(); iter != this->end(); ++iter) {
        if (pred(*iter)) {
            res.push_back(*iter);
        }
    }
    return res;
}
