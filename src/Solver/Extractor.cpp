#include "Solver/Extractor.h"
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

/**
 * @brief Replace the matched pairs in match_cands with new ArrayRef.
 * 
 * @return shared_ptr<ArrayRef> The new Intermediate Variable Array.
 */
// shared_ptr<ir::ArrayRef> replaceMatchCand(const vector<TreeNodeVec> &match_cands, int &iv_cnt, int avn_cnt) {
//     // Create new Array.
//     // ! disp should be DefinedSpaces, while naive 0 indexes are created here.
    
//     // Get union of dimensions.
//     set<int> dims;
//     for (auto i : match_cands[0]) {
//         for (auto disp : i->getDisplacement()) {
//             dims.insert(disp.first);
//         }
//     }
    
//     vector<shared_ptr<Expression>> disp;
//     for (int i = 0; i < dims.size(); i++) {
//         disp.push_back(make_shared<IntLiteral>(0));
//     }
//     shared_ptr<Array> array = make_shared<Array>("past_arr_" + to_string(avn_cnt) + "_" + to_string(iv_cnt), disp, 0);
//     array->setDefiningSpace(make_shared<Space>("tmp_space" + to_string(iv_cnt), dims.size()));
//     iv_cnt++;
    
//     for (auto match : match_cands) {
// #ifdef DEBUG
//         cout << "MatchCandidate {" << endl;
//         cout << match[0]->toString(false) << endl;
//         cout << match[1]->toString(false) << endl;
//         cout << "}" << endl;
// #endif
//         auto parent = match[0]->getParent();
//         parent->eraseChild(match[0]);
//         parent->eraseChild(match[1]);
        
//         // Create new ArrayRef.
//         shared_ptr<ArrayRef> arf = nullptr;
//         auto new_arr_dim = IVInfoManager::mergeDisp(match[0]->getDisplacement(), match[1]->getDisplacement());
//         vector<shared_ptr<SpaceRef>> space_refs;
//         for (auto i : new_arr_dim) {
//             // If SpaceRef.isConstant is true, the ArrayRef in generated code will have no index variable like i,j,k. So it is set to false.
//             space_refs.push_back(make_shared<SpaceRef>(make_shared<Add>(make_shared<Dim>(i.first), make_shared<IntLiteral>(i.second)), false));
//         }
//         arf = make_shared<ArrayRef>(array, nullptr, space_refs);
        
//         auto new_node = make_shared<ExprTreeNode>(arf, parent);
//         parent->addChild(new_node);
//         if (parent->getChildren().size() == 1) {
//             auto pp = parent->getParent();
//             if (pp != nullptr) {
//                 pp->eraseChild(parent);
//                 pp->addChild(new_node);
//                 new_node->setParent(pp);
//             }
//         }
//     }
//     return array;
// }

shared_ptr<ArrayRef> replaceMatchCand(const vector<shared_ptr<DeltaNode>> &match_cands, int &iv_cnt, int avn_cnt) {
    std::map<size_t, vector<shared_ptr<DeltaNode>>> hash_node_map;
    for (const auto &node : match_cands) {
        hash_node_map[node->getHash()].push_back(node);
    }
    for (const auto &iter : hash_node_map) {
        if (iter.second.size() < 2) { // TODO: Useless node. There may be some way to optimize in solve().
            continue;
        }
        // Create new ArrayRef.
        auto l_arr_node = iter.second[0]->getArrLeft()->getExpression();
        auto r_arr_node = iter.second[0]->getArrRight()->getExpression();
        // TODO: Unify ArrayRef DeclRef and Number.
    }
}

void extract(Forest expr_tree) {
    vector<string> op_list = {"+", "-", "*", "/"}; // TODO: support sin and cos.
    std::map<string, vector<TreeNodeVec>> candidates;
    for (auto op : op_list) {
        candidates[op] = vector<TreeNodeVec>();
    }

    for (auto tree : expr_tree) {
        tree->hoist();
        // tree->printTree();
        collectCandidates(tree, candidates);
    }

    // print candidates
    // for (auto op : op_list) {
    //     llvm::outs() << op << ":\n";
    //     for (auto cand : candidates[op]) {
    //         for (auto node : cand) {
    //             llvm::outs() << node->to_string() << ", ";
    //         }
    //         llvm::outs() << "\n";
    //     }
    // }

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
    // std::cout << "-------------Graph-------------\n";
    // delta_graph.printGraph();

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
    const auto &result = solver.solve(delta_graph, filtered_nodes);
    
    std::cout << "-------------Result-------------\n";
    std::cout << filtered_nodes.size() << " nodes in total.\n";
    for (const auto &node : result) {
        std::cout << *node << "\n";
    }


}

} // namespace extract