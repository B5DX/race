#include "Solver/MatchSolver.h"

DeltaNode::DeltaNode(shared_ptr<ir::ExprTreeNode> left, shared_ptr<ir::ExprTreeNode> right, bool commutable)
    : arr_left(left), arr_right(right) {
#ifdef DEBUG
    llvm::outs() << "Constructing DeltaNode: " << arr_left->to_string(false) << " -> " << arr_right->to_string(false) << "\n";
    llvm::outs() << commutable << "\n";
#endif
    std::map<string, int> left_indices, right_indices;
    if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(arr_left->getExpression())) {
        left_indices = arr_ref->getDelta();
    }
    if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(arr_right->getExpression())) {
        right_indices = arr_ref->getDelta();
    }

    if (commutable) { // Make sure the left array is "smaller" than the right array.
        std::set<string> keys;
        for (const auto &left : left_indices) {
            keys.insert(left.first);
        }
        for (const auto &right : right_indices) {
            keys.insert(right.first);
        }

        if (arr_left->to_string(true) > arr_right->to_string(true)) {
            std::swap(this->arr_left, this->arr_right);
            std::swap(left_indices, right_indices);
        } else if (arr_left->to_string(true) == arr_right->to_string(true)) {
            for (const auto &key : keys) {
                auto left_iter = left_indices.find(key);
                auto right_iter = right_indices.find(key);
                if (left_iter != left_indices.end() && right_iter != right_indices.end()) {
                    if (left_iter->second > right_iter->second) {
                        std::swap(this->arr_left, this->arr_right);
                        std::swap(left_indices, right_indices);
                        break;
                    }
                } else if (right_iter != right_indices.end()) {
                    std::swap(this->arr_left, this->arr_right);
                    std::swap(left_indices, right_indices);
                    break;
                } else {
                    break;
                }
            }
        }
    }

    for (const auto &left : left_indices) {
        if (right_indices.find(left.first) != right_indices.end()) {
            delta[left.first] = right_indices[left.first] - left.second;
        }
    }

    hash_number = std::hash<string>()(this->to_string());
}

string DeltaNode::to_string() const {
    string ret = arr_left->to_string() + " -> " + arr_right->to_string() + ": ";
    size_t idx = 0;
    for (const auto &pair : delta) {
        ret += pair.first + " " + std::to_string(pair.second) + ", ";
    }
    return ret;
    // llvm::outs() << arr_left->to_string() << " -> " << arr_right->to_string() << ": ";
    // for (const auto &pair : delta) {
    //     llvm::outs() << pair.first << " " << pair.second << ", ";
    // }
    // llvm::outs() << "\n";
}

bool DeltaNode::operator==(const DeltaNode &other) const {
    if (arr_left->to_string() != other.arr_left->to_string() ||
        arr_right->to_string() != other.arr_right->to_string()) {
        return false;
    }
    for (const auto &pair : delta) {
        if (other.delta.find(pair.first) == other.delta.end() ||
            other.delta.at(pair.first) != pair.second) {
            return false;
        }
    }
    return true;
}

std::ostream &operator<<(std::ostream &os, const DeltaNode &node) {
    os << node.to_string();
    return os;
}

void Solver::_solve(const UDGraph<shared_ptr<DeltaNode>> &delta_graph, const vector<shared_ptr<DeltaNode>> &nodes, size_t idx) {
    if (idx == nodes.size()) {
        if (temp_profit > profit) {
            profit = temp_profit;
            result = temp_vec;
        }
        return;
    }

    // If the remaining nodes can't exceed the current profit, return.
    if (temp_profit + nodes.size() - idx < profit)
        return;

    // Branch 1: choose current node:
    // If the current node connects of anyone in temp_vec, don't choose this node.
    bool flag = true;
    for (const auto &node : temp_vec) {
        if (delta_graph.adjacent(node, nodes[idx])) {
            flag = false;
            break;
        }
    }
    if (flag) {
        temp_vec.push_back(nodes[idx]);
        // if (temp_profit_map.find(nodes[idx]->getHash()) == temp_profit_map.end()) {
        //     temp_profit_map[nodes[idx]->getHash()] = 0;
        // }
        if (temp_profit_map[nodes[idx]->getHash()] >= 1) {
            temp_profit++;
        }
        ++temp_profit_map[nodes[idx]->getHash()];
        _solve(delta_graph, nodes, idx + 1);
        if (temp_profit_map[nodes[idx]->getHash()] > 1) {
            temp_profit--;
        }
        --temp_profit_map[nodes[idx]->getHash()];
        temp_vec.pop_back();
    }
    
    // Branch 2: don't choose current node:
    _solve(delta_graph, nodes, idx + 1);
}

// TODO: Performance improvement needed. 
// 1. Isolating the connected components' computation may be helpful. 
// 2. If nodes of certain hash number are all directly connected, then this hash number can be ignored.
// 3. If a connected component contains all the nodes of a certain hash number, then this hash number's extract can be solved independently.
// Tips: 连通分量+哈希值 可以优化。先找出所有连通分量，然后针对某个哈希值（例如0），找到所有包含0的连通分量，如果这些分量包含其他哈希值则继续递归，直到所有找到的连通分量之外都不再包含连通分量中的任何哈希值，则该子图可以独立计算。
vector<shared_ptr<DeltaNode>> Solver::solve(
    const UDGraph<shared_ptr<DeltaNode>> &delta_graph, const vector<shared_ptr<DeltaNode>> &nodes) {
    // Initialize the temporary variables.
    temp_vec.clear();
    temp_profit_map.clear();
    temp_profit = 0;
    result.clear();
    profit = 0;
    _solve(delta_graph, nodes, 0);
    return result;
}