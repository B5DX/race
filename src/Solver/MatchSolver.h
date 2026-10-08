#ifndef MATCH_SOLVER_H
#define MATCH_SOLVER_H

#include "IR/ExprTree.h"
#include "Solver/Graph.hpp"
#include "common.h"
#include <boost/rational.hpp>

/**
 * @brief A node in the delta-graph. It represents the difference between two ExprTreeNode(may be ArrayRef, DeclRef or Number).
 * @note DeclRef and Number is considered as a special case of Array(ArrayRef with no index).
 */
class DeltaNode {
private:
    shared_ptr<ir::ExprTreeNode> arr_left, arr_right;
    std::map<string, boost::rational<int>> delta; // key: intersection of left and right's dimension name, value: difference between left and right.
    size_t hash_number;

public:
    /**
     * @brief Construct a new Delta Node object.
     * @param commutable true for commutable operator(like '+', '*').
     * If true, the constructor guarantees the left array will be "smaller" than the right array.
     * Else, the left and right will remain the same order as input.
     */
    DeltaNode(shared_ptr<ir::ExprTreeNode> left, shared_ptr<ir::ExprTreeNode> right, bool commutable = true);

    inline shared_ptr<ir::ExprTreeNode> getLeft() {
        return arr_left;
    }

    inline shared_ptr<ir::ExprTreeNode> getRight() {
        return arr_right;
    }

    inline shared_ptr<const ir::ExprTreeNode> getLeft() const {
        return arr_left;
    }

    inline shared_ptr<const ir::ExprTreeNode> getRight() const {
        return arr_right;
    }

    const std::map<string, boost::rational<int>> &getDelta() const {
        return delta;
    }

    bool operator==(const DeltaNode &other) const;

    bool operator<(const DeltaNode &other) const;

    string to_string() const;

    size_t getHash() const {
        return hash_number;
    }
};

std::ostream &operator<<(std::ostream &os, const DeltaNode &node);

class Solver {
protected:
    // Temporary variables for the recursive function.
    vector<shared_ptr<DeltaNode>> temp_vec;
    std::map<size_t, int> temp_profit_map; // key: hash number, value: count of the node.
    int temp_profit;
    // Variables for the final result.
    vector<shared_ptr<DeltaNode>> result;
    int profit;

    virtual void init() {
        temp_vec.clear();
        temp_profit_map.clear();
        temp_profit = 0;
        result.clear();
        profit = 0;
    }

    virtual void _solve(const UDGraph<shared_ptr<DeltaNode>> &delta_graph, const vector<shared_ptr<DeltaNode>> &nodes, size_t idx);

public:
    Solver() {}

    virtual vector<shared_ptr<DeltaNode>> solve(
        const UDGraph<shared_ptr<DeltaNode>> &delta_graph) = 0;
};

// Implement naive solver algorithm to compare performance.
class NaiveSolver : public Solver{
private:

public:
    virtual vector<shared_ptr<DeltaNode>> solve(
        const UDGraph<shared_ptr<DeltaNode>> &delta_graph) override;
};

class OptSolver : public Solver {
private:

public:
    virtual vector<shared_ptr<DeltaNode>> solve(
        const UDGraph<shared_ptr<DeltaNode>> &delta_graph) override;
};

class DimFirstSolver : public Solver {
private:
    vector<string> loop_vars;
public:
    DimFirstSolver(const vector<string> &loop_vars) : loop_vars(loop_vars) {}
    DimFirstSolver(const vector<shared_ptr<ir::DeclRef>> &loop_vars) {
        for (const auto &var : loop_vars) {
            this->loop_vars.push_back(var->to_string());
        }
    }
    virtual vector<shared_ptr<DeltaNode>> solve(
        const UDGraph<shared_ptr<DeltaNode>> &delta_graph) override;
};

class SearchSpaceTooLarge : public std::runtime_error {
public:
    explicit SearchSpaceTooLarge(const std::string& msg)
        : std::runtime_error(msg) {}
};

#endif
