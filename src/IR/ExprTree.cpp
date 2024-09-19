#include "IR/ExprTree.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>

using namespace ir;

void ExprTreeNode::hoist() {
    if (!isOperatorNode())
        return;

    int children_len = children.size();
    vector<int> lifted_children;
    for (int i = 0; i < children_len; i++) {
        children[i]->hoist();
    }
    // "-" and "/" shouldn't be hoisted directly.
    if (this->to_string() != "-" && this->to_string() != "/") {
        for (int i = 0; i < children_len; i++) {
            // This insert operation will maintain the original order of children. But takes more time.
            // (Maybe it can be considered to change children vector to list if the order matters?)
            if (children[i]->to_string() == this->to_string()) {
                lifted_children.push_back(i);
                auto cc = children[i]->getChildren();
                children.insert(children.begin() + i + 1, cc.begin(), cc.end());
                std::for_each(cc.begin(), cc.end(), [this](shared_ptr<ExprTreeNode> child) {
                    child->setParent(weak_from_this());
                });
                i += cc.size();
            }
        }
    }
    int idx_delta = 0;
    for (int lifted_child : lifted_children) {
        children.erase(children.begin() + lifted_child - idx_delta);
        idx_delta++;
    }
}

void ExprTreeNode::deposit() {
    size_t children_len = children.size();
    if (children_len == 0)
        return;

    // deposit children first
    for (size_t i = 0; i < children_len; i++) {
        children[i]->deposit();
    }
    // copy children to queue
    queue<shared_ptr<ExprTreeNode>> node_queue;
    for (auto child : children) {
        node_queue.push(child);
    }
    // Build new binary tree. The last one node in the queue is the new root, which will replace this node.
    while (node_queue.size() > 1) {
        auto node1 = node_queue.front();
        node_queue.pop();
        auto node2 = node_queue.front();
        node_queue.pop();
        shared_ptr<BinaryOp> biop;
        // ! Note that src here may have empty member lhs or rhs.
        if (auto add = llvm_pointer_cast<AddOp>(src)) {
            biop = make_shared<AddOp>();
        } else if (auto sub = llvm_pointer_cast<SubOp>(src)) {
            biop = make_shared<SubOp>();
        } else if (auto mul = llvm_pointer_cast<MulOp>(src)) {
            biop = make_shared<MulOp>();
        } else if (auto div = llvm_pointer_cast<DivOp>(src)) {
            biop = make_shared<DivOp>();
        } else if (auto as = llvm_pointer_cast<AssignOp>(src)) {
            biop = make_shared<AssignOp>();
        }
        biop->setLHS(node1->getExpression());
        biop->setRHS(node2->getExpression());
        shared_ptr<ExprTreeNode> new_expr_node;
        new_expr_node = make_shared<ExprTreeNode>(biop, shared_from_this());
        node1->setParent(new_expr_node);
        node2->setParent(new_expr_node);
        new_expr_node->addChild(node1);
        new_expr_node->addChild(node2);
        node_queue.push(new_expr_node);
    }
    // copy new root's info to replace this node.
    children.clear();
    auto new_root = node_queue.front();
    for (auto child : new_root->getChildren()) {
        children.push_back(child);
        child->setParent(weak_from_this());
    }
    src = new_root->getExpression();
}

void ExprTreeNode::eraseChild(shared_ptr<ExprTreeNode> child) {
    if (!isOperatorNode()) {
        throw std::runtime_error("erase child from a none operator node.");
    }
    auto res = std::find(children.begin(), children.end(), child);
    if (res == children.end()) {
        throw std::runtime_error("erase absent child.");
    }
    children.erase(res);
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
        root->addChild(uop->getOperand());
        buildTree(root->getChildren().front(), uop->getOperand());
    } else {
        return;
    }
}

set<shared_ptr<ExprTreeNode>> ExprTree::getUsedArrayRefs() {
    set<shared_ptr<ExprTreeNode>> res;
    queue<shared_ptr<ExprTreeNode>> node_queue;
    node_queue.push(root);
    while (!node_queue.empty()) {
        auto node = node_queue.front();
        node_queue.pop();
        if (auto arf = llvm_pointer_cast<ArrayRef>(node->getExpression())) {
            res.insert(node);
        }
        for (auto child : node->getChildren()) {
            node_queue.push(child);
        }
    }
    return res;
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

// void ExprTree::replaceLoopVar(int target_dim, string new_var) {
//     for (auto iter = this->begin(); iter != this->end(); ++iter) {
//         auto node = *iter;
//         if (auto arf = llvm_pointer_cast<ArrayRef>(node->getExpression())) {
//             auto spaceRefs = arf->getSpaceRefs();
//             for (auto &spaceRef : spaceRefs) {
//                 if (auto dim = llvm_pointer_cast<Dim>(spaceRef->getRef())) {
//                     if (dim->getValue() == target_dim) {
//                         spaceRef->setRef(make_shared<DeclRef>(new_var));
//                     }
//                 } else if (auto bop = llvm_pointer_cast<BinaryOp>(spaceRef->getRef())) {
//                     if (auto dim = llvm_pointer_cast<Dim>(bop->getLHS())) {
//                         if (dim->getValue() == target_dim) {
//                             bop->setLHS(make_shared<DeclRef>(new_var));
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }

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