#include "Solver/MatchSolver.h"
#include "string"
#include <boost/rational.hpp>

// DeltaNode::DeltaNode(shared_ptr<ir::ExprTreeNode> left, shared_ptr<ir::ExprTreeNode> right, bool commutable)
//     : arr_left(left), arr_right(right) {
// #ifdef DEBUG
//     llvm::outs() << "Constructing DeltaNode: " << arr_left->to_string(false) << " -> " << arr_right->to_string(false) << "\n";
//     llvm::outs() << commutable << "\n";
// #endif
//     std::map<string, int> left_indices, right_indices;
//     if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(arr_left->getExpression())) {
//         left_indices = arr_ref->getDisp();
//     }
//     if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(arr_right->getExpression())) {
//         right_indices = arr_ref->getDisp();
//     }

//     if (commutable) { // Make sure the left array is "smaller" than the right array.
//         std::set<string> keys;
//         for (const auto &left : left_indices) {
//             keys.insert(left.first);
//         }
//         for (const auto &right : right_indices) {
//             keys.insert(right.first);
//         }

//         if (arr_left->to_string(true) > arr_right->to_string(true)) {
//             std::swap(this->arr_left, this->arr_right);
//             std::swap(left_indices, right_indices);
//         } else if (arr_left->to_string(true) == arr_right->to_string(true)) {
//             for (const auto &key : keys) {
//                 auto left_iter = left_indices.find(key);
//                 auto right_iter = right_indices.find(key);
//                 if (left_iter != left_indices.end() && right_iter != right_indices.end()) {
//                     if (left_iter->second > right_iter->second) {
//                         std::swap(this->arr_left, this->arr_right);
//                         std::swap(left_indices, right_indices);
//                         break;
//                     }
//                 } else if (right_iter != right_indices.end()) {
//                     std::swap(this->arr_left, this->arr_right);
//                     std::swap(left_indices, right_indices);
//                     break;
//                 } else {
//                     break;
//                 }
//             }
//         }
//     }

//     for (const auto &left : left_indices) {
//         if (right_indices.find(left.first) != right_indices.end()) {
//             delta[left.first] = right_indices[left.first] - left.second;
//         }
//     }

//     hash_number = std::hash<string>()(this->to_string());
    
//     // Only i-dim
//     // auto left_src = arr_left->getExpression();
//     // auto right_src = arr_right->getExpression();
//     // string aux_str = "";
//     // if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(left_src)) {
//     //     auto loop_vars = arr_ref->getParentLoop()->getLoopVars();
//     //     for (size_t idx = 1; idx < loop_vars.size(); idx++) {
//     //         aux_str += std::to_string(left_indices[loop_vars[idx]->to_string()]) + "_";
//     //     }
//     // }
//     // if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(right_src)) {
//     //     auto loop_vars = arr_ref->getParentLoop()->getLoopVars();
//     //     for (size_t idx = 1; idx < loop_vars.size(); idx++) {
//     //         aux_str += std::to_string(right_indices[loop_vars[idx]->to_string()]) + "_";
//     //     }
//     // }
//     // hash_number = std::hash<string>()(this->to_string() + aux_str);
// }

DeltaNode::DeltaNode(shared_ptr<ir::ExprTreeNode> left, shared_ptr<ir::ExprTreeNode> right, bool commutable)
    : arr_left(left), arr_right(right) {
#ifdef DEBUG
    llvm::outs() << "Constructing DeltaNode: " << arr_left->to_string(false) << " -> " << arr_right->to_string(false) << "\n";
    llvm::outs() << commutable << "\n";
#endif
    size_t left_rpi = 0, right_rpi = 0;
    if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(arr_left->getExpression())) {
        left_rpi = arr_ref->getRPI();
    } else {
        left_rpi = std::hash<string>()(arr_left->to_string());
    }
    if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(arr_right->getExpression())) {
        right_rpi = arr_ref->getRPI();
    } else {
        right_rpi = std::hash<string>()(arr_right->to_string());
    }

    auto left_arf = ir::llvm_pointer_cast<ir::ArrayRef>(arr_left->getExpression());
    auto right_arf = ir::llvm_pointer_cast<ir::ArrayRef>(arr_right->getExpression());
    using ExprForm = ir::Subscript::ExprForm;

    if (commutable) { // Make sure the left array is "smaller" than the right array.
        if (left_rpi > right_rpi) {
            std::swap(this->arr_left, this->arr_right);
            std::swap(left_rpi, right_rpi);
            std::swap(left_arf, right_arf);
        } else if (left_rpi == right_rpi) {
            if (left_arf) { // Left and right are both ArrayRef. Here the difference can only be in the constant term.
                for (size_t i = 0; i < left_arf->getSubscripts().size(); i++) {
                    auto left_sub = left_arf->getSubscripts()[i];
                    auto right_sub = right_arf->getSubscripts()[i];
                    if (left_sub.getB() > right_sub.getB()) {
                        std::swap(this->arr_left, this->arr_right);
                        std::swap(left_rpi, right_rpi);
                        std::swap(left_arf, right_arf);
                        break;
                    }
                }
            }
            // Else, left and right are both number or scalar. There is no need to swap.
        }
    }

    string hash_str = std::to_string(left_rpi) + "$" + std::to_string(right_rpi);
    if (left_arf && right_arf) {
        std::map<string, boost::rational<int>> left_firstIdxOffset, right_firstIdxOffset;
        left_firstIdxOffset = left_arf->getFirstIdxOffset();
        right_firstIdxOffset = right_arf->getFirstIdxOffset();
        // Compute the delta.
        for (auto str_rantional : left_firstIdxOffset) {
            if (right_firstIdxOffset.find(str_rantional.first) != right_firstIdxOffset.end()) {
                boost::rational<int> delta = right_firstIdxOffset[str_rantional.first] - str_rantional.second;
                this->delta[str_rantional.first] = delta;
                hash_str += "$" + std::to_string(delta.numerator()) + "/" + std::to_string(delta.denominator());
            }
        }
    }
    hash_number = std::hash<string>()(hash_str);
    // if (left_arf) {
    //     std::cout << left_arf->to_string() << " -> ";
    // } else {
    //     std::cout << arr_left->to_string() << " -> ";
    // }
    // if (right_arf) {
    //     std::cout << right_arf->to_string() << ": " << hash_str << std::endl;
    // } else {
    //     std::cout << arr_right->to_string() << ": " << hash_str << std::endl;
    // }
    // std::cout << hash_str << std::endl;
    
    // Only i-dim
    // auto left_src = arr_left->getExpression();
    // auto right_src = arr_right->getExpression();
    // string aux_str = "";
    // if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(left_src)) {
    //     auto loop_vars = arr_ref->getParentLoop()->getLoopVars();
    //     for (size_t idx = 1; idx < loop_vars.size(); idx++) {
    //         aux_str += std::to_string(left_indices[loop_vars[idx]->to_string()]) + "_";
    //     }
    // }
    // if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(right_src)) {
    //     auto loop_vars = arr_ref->getParentLoop()->getLoopVars();
    //     for (size_t idx = 1; idx < loop_vars.size(); idx++) {
    //         aux_str += std::to_string(right_indices[loop_vars[idx]->to_string()]) + "_";
    //     }
    // }
    // hash_number = std::hash<string>()(this->to_string() + aux_str);
}

string DeltaNode::to_string() const {
    string ret = arr_left->to_string() + " -> " + arr_right->to_string() + ": ";
    size_t idx = 0;
    for (const auto &pair : delta) {
        ret += pair.first + " " + std::to_string(pair.second.numerator()) 
            + "/" + std::to_string(pair.second.denominator()) + ", ";
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

bool DeltaNode::operator<(const DeltaNode &other) const {
    if (arr_left->to_string() != other.arr_left->to_string()) {
        return arr_left->to_string() < other.arr_left->to_string();
    }
    if (arr_right->to_string() != other.arr_right->to_string()) {
        return arr_right->to_string() < other.arr_right->to_string();
    }
    for (const auto &pair : delta) {
        if (other.delta.find(pair.first) == other.delta.end()) {
            return true;
        }
        if (other.delta.at(pair.first) != pair.second) {
            return pair.second < other.delta.at(pair.first);
        }
    }
    return false;
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

vector<shared_ptr<DeltaNode>> NaiveSolver::solve(const UDGraph<shared_ptr<DeltaNode>> &delta_graph) {
    std::map<size_t, vector<shared_ptr<DeltaNode>>> hash_node_map;
    for (const auto &node : delta_graph.getNodes()) {
        hash_node_map[node->getHash()].push_back(node);
    }
    vector<shared_ptr<DeltaNode>> filtered_nodes;
    for (const auto &iter : hash_node_map) {
        if (iter.second.size() > 1) {
            filtered_nodes.insert(filtered_nodes.end(), iter.second.begin(), iter.second.end());
        }
    }

    init();
    _solve(delta_graph, filtered_nodes, 0);
    return result;
}

// TODO: Performance improvement needed.
// 1. Isolating the connected components' computation may be helpful.
// 2. If nodes of certain hash number are all directly connected, then this hash number can be ignored.
// 3. If a connected component contains all the nodes of a certain hash number, then this hash number's extract can be solved independently.
// Tips: 连通分量+哈希值 可以优化。先找出所有连通分量，然后针对某个哈希值（例如0），找到所有包含0的连通分量，如果这些分量包含其他哈希值则继续递归，直到所有找到的连通分量之外都不再包含连通分量中的任何哈希值，则该子图可以独立计算。
vector<shared_ptr<DeltaNode>> OptSolver::solve(const UDGraph<shared_ptr<DeltaNode>> &delta_graph) {
    vector<UDGraph<shared_ptr<DeltaNode>>> connected_components = delta_graph.connectedComponents();
    // std::cout << "Connected components: " << connected_components.size() << "\n";
    // connected_component_hash_set[i] is the hash_number appeared in connected_components[i].
    vector<std::set<size_t>> connected_component_hash_set(connected_components.size());
    vector<bool> solved(connected_components.size(), false);
    // The result which will be returned.
    vector<shared_ptr<DeltaNode>> ret;
    // Set connected_component_hash_set.
    for (size_t i = 0; i < connected_components.size(); i++) {
        std::set<size_t> hash_numbers;
        for (const auto &node : connected_components[i].getNodes()) {
            hash_numbers.insert(node->getHash());
        }
        connected_component_hash_set[i] = hash_numbers;
    }
    // Solve each related components.
    // ! The connected_components will be changed. It shouldn't be used afterwards.
    for (size_t i = 0; i < connected_components.size(); i++) {
        if (solved[i]) {
            continue;
        }
        solved[i] = true;
        // component may be changed. So connected_components[i] shouldn't be used again.
        auto &component = connected_components[i];
        auto &hash_set = connected_component_hash_set[i];
        for (size_t j = i + 1; j < connected_components.size(); j++) {
            std::set<size_t> hash_set_intersection;
            std::set_intersection(hash_set.begin(), hash_set.end(),
                                  connected_component_hash_set[j].begin(), connected_component_hash_set[j].end(),
                                  std::inserter(hash_set_intersection, hash_set_intersection.begin()));
            if (hash_set_intersection.size() > 0) {
                // std::cout << "Connected component " << i << " and " << j << " have intersection.\n";
                std::set_union(hash_set.begin(), hash_set.end(),
                               connected_component_hash_set[j].begin(), connected_component_hash_set[j].end(),
                               std::inserter(hash_set, hash_set.begin()));
                solved[j] = true;
                // ! Here the component is changed, which is a reference to connected_components[i].
                component.concatGraph(connected_components[j]);
            }
        }

        std::map<size_t, vector<shared_ptr<DeltaNode>>> hash_node_map;
        for (const auto &node : component.getNodes()) {
            hash_node_map[node->getHash()].push_back(node);
        }
        vector<shared_ptr<DeltaNode>> filtered_nodes;
        for (const auto &iter : hash_node_map) {
            if (iter.second.size() > 1) {
                filtered_nodes.insert(filtered_nodes.end(), iter.second.begin(), iter.second.end());
            }
        }

        // std::cout << "Solving connected component " << i << " with " << filtered_nodes.size() << " nodes.\n";

        // Solve current connected component. Results will be stored in this->result.
        init();
        _solve(component, filtered_nodes, 0);
        ret.insert(ret.end(), result.begin(), result.end());
    }
    return ret;
}

vector<shared_ptr<DeltaNode>> DimFirstSolver::solve(const UDGraph<shared_ptr<DeltaNode>> &delta_graph) {
    vector<UDGraph<shared_ptr<DeltaNode>>> connected_components = delta_graph.connectedComponents();
    // std::cout << "Connected components: " << connected_components.size() << "\n";
    // connected_component_hash_set[i] is the hash_number appeared in connected_components[i].
    vector<std::set<size_t>> connected_component_hash_set(connected_components.size());
    vector<bool> solved(connected_components.size(), false);
    // The result which will be returned.
    vector<shared_ptr<DeltaNode>> ret;
    // Set connected_component_hash_set.
    for (size_t i = 0; i < connected_components.size(); i++) {
        std::set<size_t> hash_numbers;
        for (const auto &node : connected_components[i].getNodes()) {
            hash_numbers.insert(node->getHash());
        }
        connected_component_hash_set[i] = hash_numbers;
    }
    // Solve each related components.
    // ! The connected_components will be changed. It shouldn't be used afterwards.
    for (size_t i = 0; i < connected_components.size(); i++) {
        if (solved[i]) {
            continue;
        }
        solved[i] = true;
        // component may be changed. So connected_components[i] shouldn't be used again.
        auto &component = connected_components[i];
        auto &hash_set = connected_component_hash_set[i];
        for (size_t j = i + 1; j < connected_components.size(); j++) {
            std::set<size_t> hash_set_intersection;
            std::set_intersection(hash_set.begin(), hash_set.end(),
                                  connected_component_hash_set[j].begin(), connected_component_hash_set[j].end(),
                                  std::inserter(hash_set_intersection, hash_set_intersection.begin()));
            if (hash_set_intersection.size() > 0) {
                // std::cout << "Connected component " << i << " and " << j << " have intersection.\n";
                std::set_union(hash_set.begin(), hash_set.end(),
                               connected_component_hash_set[j].begin(), connected_component_hash_set[j].end(),
                               std::inserter(hash_set, hash_set.begin()));
                solved[j] = true;
                // ! Here the component is changed, which is a reference to connected_components[i].
                component.concatGraph(connected_components[j]);
            }
        }
        std::map<size_t, vector<shared_ptr<DeltaNode>>> hash_node_map;
        for (const auto &node : component.getNodes()) {
            hash_node_map[node->getHash()].push_back(node);
        }
        bool flag = false;
        // TODO: This can also be achieved by the priority computation in _solve function. Maybe a adjusted priority value can elegently solve this.
        // Search for results by loop variables' priority and non-zero deltas' count.
        // 1. Search for the innermost loop variable to solve.
        for (int loop_var_priority = 0; loop_var_priority < loop_vars.size(); loop_var_priority++) {
            // 2. Search for as many zero-delta loop variables as possible.
            for (int delta_cnt = 1; delta_cnt <= loop_vars.size(); delta_cnt++) {
                string cur_var = this->loop_vars[loop_var_priority];
                vector<shared_ptr<DeltaNode>> filtered_nodes;
                for (const auto &iter : hash_node_map) {
                    if (iter.second.size() > 1) {
                        auto delta = iter.second.front()->getDelta();
                        if (delta.find(cur_var) != delta.end() && delta.at(cur_var) == 0) {
                            int cur_cnt = 0;
                            for (const auto &cur_delta : delta) {
                                if (cur_delta.second != 0) {
                                    cur_cnt++;
                                }
                            }
                            // delta.at(cur_var) == 0. And the number of non-zero deltas is less than or equal to delta_cnt.
                            if (cur_cnt <= delta_cnt) {
                                filtered_nodes.insert(filtered_nodes.end(), iter.second.begin(), iter.second.end());
                            }
                        }
                    }
                }
                init();
                _solve(component, filtered_nodes, 0);
                if (result.size() > 0) {
                    ret.insert(ret.end(), result.begin(), result.end());
                    flag = true;
                    break;
                }
            }
            if (flag) {
                break;
            }
        }
        // If can't find any result by the loop variables' priority and non-zero deltas' count. Search for all nodes.
        if (!flag) {
            vector<shared_ptr<DeltaNode>> filtered_nodes;
            for (const auto &iter : hash_node_map) {
                if (iter.second.size() > 1) {
                    filtered_nodes.insert(filtered_nodes.end(), iter.second.begin(), iter.second.end());
                }
            }
            init();
            _solve(component, filtered_nodes, 0);
            ret.insert(ret.end(), result.begin(), result.end());
        }
    }
    return ret;
}