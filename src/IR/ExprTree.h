#ifndef EXPR_TREE_H
#define EXPR_TREE_H

#include "IR/IR.h"
#include <functional>
#include <queue>
#include <set>
#include <stack>

namespace ir {

using std::make_shared;
using std::pair;
using std::queue;
using std::set;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;

/**
 * @brief A m-ary tree node class for intermediate variable extraction.
 * @details
 * {shared_ptr<ir::Expression> src} is a pointer to the PAST Expression it represents.
 * This class is to transform the original PAST Expression(which is a binary tree) to a m-ary tree.
 */
class ExprTreeNode : public std::enable_shared_from_this<ExprTreeNode> {
  protected:
    vector<shared_ptr<ExprTreeNode>> children;
    // parent use weak_ptr to avoid Circular Reference.
    // Using shared_ptr hear will cause memory leak. For example, parent A has children B.
    // If A is nolonger used, it will not be deleted since there is a reference from B to A.
    std::weak_ptr<ExprTreeNode> parent;
    shared_ptr<Expression> src;
    bool isOperator;

  public:
    ExprTreeNode() : src(nullptr), parent(std::weak_ptr<ExprTreeNode>()), isOperator(false) {}

    /**
     * @brief Construct a new Expr Tree Node object which points to Expression e.
     * @note
     * * Its parent is set to p, but p's children is not changed. So you should add this node to p's children manually under normal conditions.
     */
    ExprTreeNode(shared_ptr<ir::Expression> e, shared_ptr<ExprTreeNode> p) {
        src = e;
        parent = p;
        if (llvm_isa<ir::Operator>(e)) {
            isOperator = true;
        } else {
            isOperator = false;
        }
    }

    void addChild(shared_ptr<ExprTreeNode> child) {
        if (!isOperator) {
            throw std::runtime_error("AddOp child to a none operator node.");
        }
        children.push_back(child);
    }

    void addChild(shared_ptr<ir::Expression> e) {
        if (!isOperator) {
            throw std::runtime_error("AddOp child to a none operator node.");
        }
        children.push_back(make_shared<ExprTreeNode>(e, shared_from_this()));
    }

    void setParent(weak_ptr<ExprTreeNode> p) { parent = p; }

    void setExpression(shared_ptr<ir::Expression> e) { this->src = e; }

    /**
     * @brief Hoist this operator node's children whose operator equals itself.
     * @details
     * This method transforms the tree from the original binary tree to a m-ary tree.
     * TODO: MinusOp and DivOp May be wrong.
     */
    void hoist();

    /**
     * @brief Deposit the tree to a binary form using post traverse.
     * @details
     * This method transforms the tree to a binary tree with relatively low depth.
     * TODO: MinusOp and DivOp May be wrong.
     */
    virtual void deposit();

    const vector<shared_ptr<ExprTreeNode>> &getChildren() { return children; }

    shared_ptr<ExprTreeNode> getParent() {
        if (parent.expired()) {
            throw std::runtime_error("getParent while parent is deleted.");
        } else {
            return parent.lock();
        }
    }

    shared_ptr<ir::Expression> getExpression() { return src; }

    /**
     * @brief Get the Displacement of this ArrayRef node.
     *
     * @return vector<pair<int, int>>: A vector of pairs of (dim, displacement). eg. [(2, 0), (1, 1), (0, 2)] means the ArrayRef is A[0][1][2].
     */
    vector<pair<int, int>> getDisplacement();

    shared_ptr<DeclRef> getArrayName() {
        if (auto arf = llvm_pointer_cast<ArrayRef>(src)) {
            return arf->getArrayName();
        } else {
            return nullptr;
        }
    }

    void eraseChild(shared_ptr<ExprTreeNode> child);

    inline bool isLeafNode() { return children.empty(); }

    inline bool isOperatorNode() { return isOperator; }

    inline bool isArrayRef() { return llvm_isa<ir::ArrayRef>(src); }

    inline bool isLiteral() { return llvm_isa<Number>(src); }

    inline bool isDeclRef() { return llvm_isa<ir::DeclRef>(src); }

    virtual string to_string(bool onlyName = true) {
        if (isOperator) {
            return llvm_pointer_cast<Operator>(src)->getOpStr();
        } else if (auto arr = llvm_pointer_cast<ArrayRef>(src)) {
            if (onlyName) {
                return arr->getArrayName()->to_string();
            } else {
                return arr->to_string();
            }
        } else {
            return src->to_string();
        }
    }

    void printTree(int depth = 0);
};

/**
 * @brief A tree class which contains its root ExprTreeNode and its lhs Expression(DeclRef or ArrayRef).
 */
class ExprTree {
  protected:
    using TreeNodeVec = vector<shared_ptr<ExprTreeNode>>;
    shared_ptr<ExprTreeNode> root;     // The root of the AssignOp's rhs' expression tree.
    shared_ptr<Expression> assign_lhs; // The AssignOp's lhs.

    /**
     * @brief Build a tree from Expr e recursively.
     * @note The root of the tree is the operator node of e. root is already created, the function build its children.
     */
    void buildTree(shared_ptr<ExprTreeNode> root, shared_ptr<ir::Expression> e);

  public:
    struct Iterator {
        using difference_type = std::ptrdiff_t;

        Iterator(shared_ptr<ExprTreeNode> node) : node(node), children_index() {}

        shared_ptr<ExprTreeNode> operator*() const {
            return node;
        }

        shared_ptr<ExprTreeNode> operator->() const {
            return node;
        }

        // Pre-traverse
        Iterator &operator++() {
            if (node->isLeafNode()) {
                auto p = node->getParent();
                children_index.top()++;
                while (children_index.top() == p->getChildren().size()) {
                    children_index.pop();
                    if (children_index.empty())
                        break;
                    p = p->getParent();
                    children_index.top()++;
                }
                if (children_index.empty()) {
                    node = nullptr;
                } else {
                    node = p->getChildren()[children_index.top()];
                }
            } else {
                children_index.push(0);
                node = node->getChildren().front();
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++*this;
            return tmp;
        }

        friend bool operator==(const Iterator &a, const Iterator &b) { return a.node == b.node; }
        friend bool operator!=(const Iterator &a, const Iterator &b) { return a.node != b.node; }

      private:
        shared_ptr<ExprTreeNode> node;
        std::stack<int> children_index;
    };

    Iterator begin() { return Iterator(root); }

    Iterator end() { return Iterator(nullptr); }

    ExprTree(shared_ptr<ir::Assignment> assign) {
        assign_lhs = assign->getLHS();
        root = make_shared<ExprTreeNode>(assign->getRHS(), nullptr);
        buildTree(root, assign->getRHS());
    }

    void hoist() { root->hoist(); }

    virtual void deposit() { root->deposit(); }

    shared_ptr<ExprTreeNode> getRoot() { return root; }

    shared_ptr<ir::Expression> getExpression() { return root->getExpression(); }

    set<shared_ptr<ExprTreeNode>> getUsedArrayRefs();

    // void compressDim(shared_ptr<DeclRef> target_arr, int max_dim) {
    //     for (auto iter = this->begin(); iter != this->end(); ++iter) {
    //         auto node = *iter;
    //         if (auto arf = llvm_pointer_cast<ArrayRef>(node->getExpression())) {
    //             if (arf->getArrayName() == target_arr) {
    //                 auto spaceRefs = arf->getSpaceRefs();
    //                 spaceRefs.erase(spaceRefs.begin(), spaceRefs.end() - 1 - max_dim);
    //                 arf->setSpaceRefs(spaceRefs);
    //             }
    //         }
    //     }
    // }

    void replaceArf2Scalar(shared_ptr<ArrayRef> arf, shared_ptr<DeclRef> scalar);

    // void replaceLoopVar(int target_dim, string new_var);

    void printTree() { root->printTree(0); }

    vector<shared_ptr<ExprTreeNode>> getArrayRefs();

    vector<shared_ptr<ExprTreeNode>> collectNodes(std::function<bool(shared_ptr<ExprTreeNode>)> pred);
};

} // namespace ir

#endif