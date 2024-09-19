// TODO: Only support one loop var with coefficient 1 in each array index.
#include "Solver/Extractor.h"
#include "Solver/IVInfo.h"
#include "Solver/MatchSolver.h"
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

// TODO: Only support one loop var with coefficient 1 in each array index.
// TODO: support sin and cos.
void replaceMatchCand(const vector<shared_ptr<DeltaNode>> &match_cands, int &iv_cnt, int avn_cnt, IVInfoManager &info_manager) {
    std::map<size_t, vector<shared_ptr<DeltaNode>>> hash_node_map;
    for (const auto &node : match_cands) {
        hash_node_map[node->getHash()].push_back(node);
    }
    for (const auto &iter : hash_node_map) {
        if (iter.second.size() < 2) { // TODO: Useless node. There may be some way to optimize in solve().
            continue;
        }
        // Register new array.
        auto l_tree_node = iter.second[0]->getLeft();
        auto r_tree_node = iter.second[0]->getRight();
        auto l_expr = l_tree_node->getExpression();
        auto r_expr = r_tree_node->getExpression();
        string iv_name = "iv_" + std::to_string(avn_cnt) + "_" + std::to_string(iv_cnt++);
        if (auto iv_bop = llvm_pointer_cast<BinaryOp>(l_tree_node->getParent()->getExpression())) {
            info_manager.registerArray(iv_name, l_tree_node, r_tree_node);
        } else {
            throw std::runtime_error("UnaryOp haven't been supported yet.");
        }
        // Replace old array with new array.
        for (const auto &node : iter.second) {
            auto l_tree_node = node->getLeft();
            auto r_tree_node = node->getRight();
            auto parent = l_tree_node->getParent();
            parent->eraseChild(l_tree_node);
            parent->eraseChild(r_tree_node);
            auto new_arr = info_manager.getExpression(iv_name, l_tree_node, r_tree_node);
            auto new_node = make_shared<ir::ExprTreeNode>(new_arr, parent);
            parent->addChild(new_node);
            if (parent->getChildren().size() == 1) {
                auto pp = parent->getParent();
                if (pp != nullptr) {
                    pp->eraseChild(parent);
                    pp->addChild(new_node);
                    new_node->setParent(pp);
                } else { // root node is replaced.
                    parent->eraseChild(new_node);
                    parent->setExpression(new_arr);
                }
            }
        }
    }
}

// TODO: Only support one loop var with coefficient 1 in each array index.
void extract(Forest expr_trees, int avn_cnt) {
    vector<string> op_list = {"+", "-", "*", "/"}; // TODO: support sin and cos.
    std::map<string, vector<TreeNodeVec>> candidates;
    IVInfoManager info_manager;
    int iv_cnt = 0;
    ir::ForStmt *parent_for = nullptr;
    for (auto op : op_list) {
        candidates[op] = vector<TreeNodeVec>();
    }

    for (auto tree : expr_trees) {
        tree->hoist();
        if (!parent_for) { // set parent ForStmt.
            for (auto node : *tree) {
                if (auto arr_ref = llvm_pointer_cast<ArrayRef>(node->getExpression())) {
                    parent_for = arr_ref->getParentLoop();
                    info_manager.setParentFor(parent_for);
                    break;
                }
            }
        }
#ifdef DEBUG
        tree->printTree();
#endif
        info_manager.registerNativeArray(tree);
    }
#ifdef DEBUG
    info_manager.printInfo();
#endif

    while (true) { // Recursively extract.
        candidates.clear();
        for (const auto &tree : expr_trees) {
            collectCandidates(tree, candidates);
        }
        // print candidates
#ifdef DEBUG
        for (auto op : op_list) {
            llvm::outs() << op << ":\n";
            for (auto cand : candidates[op]) {
                for (auto node : cand) {
                    llvm::outs() << node->to_string() << ", ";
                }
                llvm::outs() << "\n";
            }
        }
#endif
        UDGraph<shared_ptr<DeltaNode>> delta_graph;
        bool commutable = true;
        // Begin to construct the delta-graph.
        for (auto op : op_list) {
            if (op == "-" || op == "/") {
                commutable = false;
            } else {
                commutable = true;
            }
            for (auto each_node_vec : candidates[op]) {
                std::map<size_t, vector<shared_ptr<DeltaNode>>> node_map;
                for (size_t i = 0; i < each_node_vec.size(); i++) {
                    node_map[i] = vector<shared_ptr<DeltaNode>>();
                }
                for (size_t i = 0; i < each_node_vec.size(); i++) {
                    for (size_t j = i + 1; j < each_node_vec.size(); j++) {
                        auto node = make_shared<DeltaNode>(each_node_vec[i], each_node_vec[j], commutable);
                        delta_graph.addNode(node);
                        node_map[i].push_back(node);
                        node_map[j].push_back(node);
                    }
                }
                for (auto iter : node_map) {
                    for (size_t i = 0; i < iter.second.size(); i++) {
                        for (size_t j = i + 1; j < iter.second.size(); j++) {
                            delta_graph.addEdge(iter.second[i], iter.second[j]);
                        }
                    }
                }
            }
        } // End of constructing the delta-delta_graph.
#ifdef DEBUG
        std::cout << "-------------Graph-------------\n";
        delta_graph.printGraph();
#endif
        // Solve the delta-graph.
        Solver solver;
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
#ifdef DEBUG
        std::cout << filtered_nodes.size() << " nodes in total.\n";
#endif

        const auto &result = solver.solve(delta_graph, filtered_nodes);

        if (result.empty())
            break;

#ifdef DEBUG
        std::cout << "-------------Result-------------\n";
        for (const auto &node : result) {
            std::cout << *node << "\n";
        }
#endif
        replaceMatchCand(result, iv_cnt, avn_cnt, info_manager);
#ifdef DEBUG
        for (const auto &tree : expr_trees) {
            tree->printTree();
        }
#endif
    }
    for (const auto &tree : expr_trees) {
        tree->deposit();
    }
}

} // namespace extract