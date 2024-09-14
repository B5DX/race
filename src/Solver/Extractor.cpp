#include "Solver/Extractor.h"
#include "llvm/Support/raw_ostream.h"

using namespace ir;

namespace extract {

void collectCandidates(shared_ptr<ExprTree> tree, std::map<string, vector<TreeNodeVec>> &result) {
    for (auto node : *tree) {
        if (node->isLeafNode()) {
            continue;
        }

        TreeNodeVec cur_cand;
        for (auto child : node->getChildren()) {
            if (child->isLeafNode()) {
                cur_cand.push_back(child);
            }
        }
        if (cur_cand.size() > 1) {
            assert(node->isOperatorNode());
            result[node->to_string()].push_back(cur_cand);
        }
    }
}

void extract(Forest expr_tree) {
    vector<string> op_list = {"+", "-", "*", "/", "sin", "cos"};
    std::map<string, vector<TreeNodeVec>> candidates;
    for (auto op : op_list) {
        candidates[op] = vector<TreeNodeVec>();
    }

    for (auto tree : expr_tree) {
        tree->hoist();
        tree->printTree();
        collectCandidates(tree, candidates);
    }

    // print candidates
    for (auto op : op_list) {
        llvm::outs() << op << ":\n";
        for (auto cand : candidates[op]) {
            for (auto node : cand) {
                llvm::outs() << node->to_string() << "\n";
            }
        }
    }

    UDGraph<shared_ptr<Node>> graph;
    for (auto op : op_list) {
        for (auto each_node_vec : candidates[op]) {
            std::map<size_t, vector<shared_ptr<Node>>> node_map;
            for (size_t i = 0; i < each_node_vec.size(); i++) {
                node_map[i] = vector<shared_ptr<Node>>();
            }
            for (size_t i = 0; i < each_node_vec.size(); i++) {
                for (size_t j = i + 1; j < each_node_vec.size(); j++) {
                    auto node = make_shared<Node>(each_node_vec[i], each_node_vec[j]);
                    graph.addNode(node);
                    node_map[i].push_back(node);
                    node_map[j].push_back(node);
                }
            }
            for (auto iter : node_map) {
                for (size_t i = 0; i < iter.second.size(); i++) {
                    for (size_t j = i + 1; j < iter.second.size(); j++) {
                        graph.addEdge(iter.second[i], iter.second[j]);
                    }
                }
            }
        } 
    }
    graph.printGraph();
}

Node::Node(shared_ptr<ir::ExprTreeNode> left, shared_ptr<ir::ExprTreeNode> right)
    : arr_left(left), arr_right(right) {
    auto left_indices = ir::llvm_pointer_cast<ir::ArrayRef>(arr_left->getExpression())->getDelta();
    auto right_indices = ir::llvm_pointer_cast<ir::ArrayRef>(arr_right->getExpression())->getDelta();

    std::set<string> keys;
    for (const auto &left : left_indices) {
        keys.insert(left.first);
    }
    for (const auto &right : right_indices) {
        keys.insert(right.first);
    }

    // Make sure the left array is "smaller" than the right array.
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

    for (const auto &left : left_indices) {
        if (right_indices.find(left.first) != right_indices.end()) {
            delta[left.first] = right_indices[left.first] - left.second;
        }
    }
}

string Node::to_string() const {
    string ret = arr_left->to_string() + " -> " + arr_right->to_string() + ": ";
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

bool Node::operator==(const Node &other) const {
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

} // namespace extract