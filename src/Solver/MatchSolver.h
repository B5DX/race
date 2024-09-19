#ifndef MATCH_SOLVER_H
#define MATCH_SOLVER_H

#include "IR/ExprTree.h"
#include "Solver/Graph.hpp"
#include "common.h"

/**
 * @brief A node in the delta-graph. It represents the difference between two ExprTreeNode(may be ArrayRef, DeclRef or Number).
 * @note DeclRef and Number is considered as a special case of Array(ArrayRef with no index).
 */
class DeltaNode {
private:
    shared_ptr<ir::ExprTreeNode> arr_left;
    shared_ptr<ir::ExprTreeNode> arr_right;
    std::map<string, int> delta; // key: intersection of left and right's dimension name, value: difference between left and right.
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

    const std::map<string, int> &getDelta() const {
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
private:
    // Temporary variables for the recursive function.
    vector<shared_ptr<DeltaNode>> temp_vec;
    std::map<size_t, int> temp_profit_map; // key: hash number, value: count of the node.
    int temp_profit;
    // Variables for the final result.
    vector<shared_ptr<DeltaNode>> result;
    int profit;

    // int computeProfit() {
    //     int cnt = 0;
    //     for (const auto &pair : temp_profit_map) {
    //         cnt += pair.second - 1;
    //     }
    //     return cnt;
    // }

    void _solve(const UDGraph<shared_ptr<DeltaNode>> &delta_graph, const vector<shared_ptr<DeltaNode>> &nodes, size_t idx);

public:
    Solver() {}

    // TODO: Get the connected components first and then solve the problem.

    vector<shared_ptr<DeltaNode>> solve(
        const UDGraph<shared_ptr<DeltaNode>> &delta_graph, const vector<shared_ptr<DeltaNode>> &nodes);
};

#endif