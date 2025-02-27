#include "IR/ExprTree.h"
#include "llvm/Support/raw_ostream.h"
#include <queue>
using std::deque;

using namespace ir;

void ExprTreeNode::hoist() {
    if (!isOperatorNode())
        return;

    int children_len = children.size();
    vector<int> lifted_children;
    for (int i = 0; i < children_len; i++) {
        children[i]->hoist();
    }
    // "-", "/" and "**" shouldn't be hoisted directly.
    if (this->to_string() != "-" && this->to_string() != "/" 
        && this->to_string() != "call" && this->to_string() != "**") {
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
            exit(0);
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
        // ! Note that src here may have empty member lhs or rhs.
        // if (auto add = llvm_pointer_cast<AddOp>(src)) {
        //     biop = make_shared<AddOp>();
        // } else if (auto sub = llvm_pointer_cast<SubOp>(src)) {
        //     biop = make_shared<SubOp>();
        // } else if (auto mul = llvm_pointer_cast<MulOp>(src)) {
        //     biop = make_shared<MulOp>();
        // } else if (auto div = llvm_pointer_cast<DivOp>(src)) {
        //     biop = make_shared<DivOp>();
        // } else if (auto as = llvm_pointer_cast<AssignOp>(src)) {
        //     biop = make_shared<AssignOp>();
        // } else if (auto call = llvm_pointer_cast<CallOp>(src)) {
        //     biop = make_shared<CallOp>();
        // } else if (auto power = llvm_pointer_cast<PowerOp>(src)) {
        //     biop = make_shared<PowerOp>();
        // } else if (auto mod = llvm_pointer_cast<ModOp>(src)) {
        //     biop = make_shared<ModOp>();
        // } else {
        //     llvm::outs() << "Deposite unsupported operator: " << src->to_string() << "\n";
        //     exit(0);
        // }
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
        // more detailed information
        // std::cout << "Error: parent tostring. " << this->to_string() << "\n";
        // std::cout << "Error: parent expression. " << this->getExpression()->to_string() << "\n";
        // for (auto child : children) {
        //     std::cout << "Error: children: " << child->to_string() << "\n";
        // }
        exit(0);
    }
    auto res = std::find(children.begin(), children.end(), child);
    if (res == children.end()) {
        // std::cout << "Error: Try to erase a non-exist child.\n";
        // std::cout << "Error: parent tostring. " << this->to_string() << "\n";
        // std::cout << "Error: parent expression. " << this->getExpression()->to_string() << "\n";
        // for (auto child : children) {
        //     std::cout << "Error: children: " << child->to_string() << "\n";
        // }
        exit(0);
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