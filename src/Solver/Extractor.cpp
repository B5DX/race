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
            auto new_arr = info_manager.getRefExpr(iv_name, l_tree_node, r_tree_node);
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

// Recursively build ForStmt. The innermost loop will contain the assignment and all the ArrayRef in assign->getRHS()'s parent loop will be set to the innermost loop.
shared_ptr<ir::ForStmt> buildForStmt(vector<string>::reverse_iterator outer_loop_var, vector<string>::reverse_iterator inner_loop_var, IVInfoManager &info_manager,
                                     const string &array, const std::map<string, ForStmt *> &loop_var2for, shared_ptr<Assignment> &assign, ForStmt *parent_for) {
    if (outer_loop_var == inner_loop_var)
        return nullptr;

    auto info = info_manager.getIVInfo(array);
    auto lb = info.used_min_delta[*outer_loop_var];
    auto ub = info.used_max_delta[*outer_loop_var];
    auto lb_expr = ir::buildExpression(loop_var2for.at(*outer_loop_var)->getBegin(), lb);
    auto ub_expr = ir::buildExpression(loop_var2for.at(*outer_loop_var)->getEnd(), ub);
    shared_ptr<ir::ForStmt> cur_loop = std::make_shared<ir::ForStmt>(*outer_loop_var, lb_expr, ub_expr, parent_for);
    shared_ptr<ir::ForStmt> inner_loop = buildForStmt(outer_loop_var + 1, inner_loop_var, info_manager, array, loop_var2for, assign, cur_loop.get());
    if (!inner_loop) { // Innermost loop.
        ir::for_each_expr<ir::ArrayRef>(assign->getRHS(), [cur_loop](shared_ptr<ir::ArrayRef> arr_ref) {
            arr_ref->setParentLoop(cur_loop.get());
        });
        cur_loop->getBody().push_back(assign);
    } else {
        cur_loop->getBody().push_back(inner_loop);
    }
    return cur_loop;
}

void insertAssignment(shared_ptr<ir::Block> outer_block, IVInfoManager &info_manager, shared_ptr<Assignment> assign) {
    auto compressed_arr_ref = llvm_pointer_cast<ArrayRef>(assign->getLHS());
    ForStmt *parent_for = info_manager.getParentFor();
    auto ordered_disp = compressed_arr_ref->getOrderedDisp();
    string outmost_idx_var = ordered_disp.back().first;
    std::map<string, ForStmt *> loop_var2for;
    while (parent_for) {
        auto var = parent_for->getLoopVar()->to_string();
        if (var == outmost_idx_var) {
            loop_var2for[var] = parent_for;
            parent_for = parent_for->getParentFor();
            break;
        }
        parent_for = parent_for->getParentFor();
        loop_var2for[var] = parent_for;
    }

    vector<string> loop_vars;
    std::transform(ordered_disp.begin(), ordered_disp.end(), std::back_inserter(loop_vars), [](const auto &p) { return p.first; });
    auto new_for = buildForStmt(loop_vars.rbegin(), loop_vars.rend(),
                                info_manager, compressed_arr_ref->getArrayName()->to_string(), loop_var2for, assign, parent_for);       
    if (!parent_for) { // Insert before outmost loop.
        auto &body = outer_block->getBody();
        body.insert(body.begin(), new_for);        
    } else {
        auto &body = parent_for->getBody().getBody();
        body.insert(body.begin(), new_for);
    }
}

// Compress the dimension of the array. Generate array construct ForStmt.
void postProcess(shared_ptr<ir::Block> outer_block, IVInfoManager &info_manager, Forest extracted_expr_tree) {
    ForStmt *parent_for = info_manager.getParentFor();
    std::set<string> iv_arrays = info_manager.getIVArray();
    std::map<string, vector<shared_ptr<ExprTreeNode>>> used_iv;
    for (auto tree : extracted_expr_tree) {
        for (auto node : *tree) {
            if (iv_arrays.find(node->to_string()) != iv_arrays.end()) {
                used_iv[node->to_string()].push_back(node);
                info_manager.registerUsage(node);
            }
        }
    }
    auto topological_order = info_manager.IVTopologicalSort();

    // Process each array in topological order.
    for (auto iv_iter = topological_order.rbegin(); iv_iter != topological_order.rend(); ++iv_iter) {
        const auto &iv_array = *iv_iter;
#ifdef DEBUG
        std::cout << "------- PostProcess: " << iv_array << "-------" << std::endl;
#endif
        if (used_iv.find(iv_array) == used_iv.end()) {
            continue;
        }
        // Case 1: The array is pure DeclRef or Number. Insert it before outmost loop.
        if (!info_manager.isArrayRef(iv_array)) { // TODO: Add support for pure DeclRef and Number iv_array.
#ifdef DEBUG
            std::cout << "------- !Not ArrayRef: " << iv_array << "-------" << std::endl;
#endif
            auto &body = outer_block->getBody();
            auto assign = std::make_shared<ir::Assignment>(std::make_shared<ir::DeclRef>(iv_array), info_manager.getConstructRHS(iv_array));
            body.insert(body.begin(), assign);
        } 
        // Case 2: The array here must be ArrayRef.
        else {
            // Compress dimension.
            auto for_loop_vars = parent_for->getLoopVars();
            auto max_delta = info_manager.getUsedMaxDelta(iv_array);
            auto min_delta = info_manager.getUsedMinDelta(iv_array);
            string outmost_idx_var; // The outmost loop variable that iv_array refers to different index. e.g. {T[j][i+1],T[j][i]}'s outmost_idx_var is i, since both refer to j.
            size_t outmost_idx_var_pos = for_loop_vars.size();
            for (auto iter = for_loop_vars.rbegin(); iter != for_loop_vars.rend(); ++iter) {
                string idx_var = (*iter)->to_string();
                if (max_delta.find(idx_var) != max_delta.end()) {
                    if (max_delta[idx_var] == min_delta[idx_var]) {
                        continue;
                    } else {
                        outmost_idx_var = idx_var;
                        outmost_idx_var_pos = int(std::distance(iter, for_loop_vars.rend())) - 1;
                        break;
                    }
                }
            }
            // TODO: All used arrays have the same indices. Can be replaced by scalar.
            // Compress it to the innermost loop var.
            if (outmost_idx_var.empty()) {
                std::cout << "------- !Exactly the same iv array: " << iv_array << "-------" << std::endl;
                outmost_idx_var = for_loop_vars.front()->to_string();
                outmost_idx_var_pos = 0;
            }
            // Collect outer_const_idx.
            std::map<string, int> outer_const_idx; // The outer loop variables that iv_array refers to the same index, which can be compressed. e.g. {T[j][i+1],T[j][i]}'s outer_const_idx is j.
            auto front_arr_ref = llvm_pointer_cast<ArrayRef>(used_iv[iv_array].front()->getExpression());
            auto front_disp = front_arr_ref->getDisp();
            for (size_t i = outmost_idx_var_pos + 1; i < for_loop_vars.size(); i++) {
                string var = for_loop_vars[i]->to_string();
                if (front_disp.find(var) != front_disp.end()) {
                    outer_const_idx[var] = front_disp[var];
                }
            }
            // Replace the array with the compressed dim.
            for (auto node : used_iv[iv_array]) {
                auto src = node->getExpression();
                if (auto arr_ref = llvm_pointer_cast<ArrayRef>(src)) {
                    auto disp = arr_ref->getDisp();
                    vector<std::pair<string, int>> new_disp;
                    for (size_t i = 0; i <= outmost_idx_var_pos; i++) { // TODO: Remove the outer loop var. Keep the outmost_idx_var itself. Can be compressed more on outmost_idx_var.
                        string idx_var = for_loop_vars[i]->to_string();
                        if (disp.find(idx_var) != disp.end()) {
                            new_disp.push_back({idx_var, disp[idx_var]});
                        }
                    }
                    auto new_arr = make_shared<ArrayRef>(iv_array, new_disp, parent_for);
                    node->setExpression(new_arr);
                }
            }
            // Generate array construct ForStmt.
            auto construct_rhs = info_manager.getConstructRHS(iv_array, outer_const_idx);
            const auto info = info_manager.getIVInfo(iv_array);
            auto index_vars = info.index_vars;
            shared_ptr<ir::Expression> construct_lhs;
            if (index_vars.empty()) {
                construct_lhs = std::make_shared<ir::DeclRef>(iv_array);
            } else {
                vector<std::pair<string, int>> disp;
                for (const auto &idx : index_vars) {
                    if (outer_const_idx.find(idx) == outer_const_idx.end()) {
                        disp.push_back({idx, 0});
                    }
                }
                construct_lhs = std::make_shared<ir::ArrayRef>(iv_array, disp, parent_for);
            }
            auto assign = std::make_shared<ir::Assignment>(construct_lhs, construct_rhs);
            insertAssignment(outer_block, info_manager, assign);
        }
        
    }
}

// TODO: Only support one loop var with coefficient 1 in each array index.
void extract(shared_ptr<ir::Block> outer_block, Forest expr_trees, int avn_cnt) {
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
        std::cout << filtered_nodes.size() << " nodes in total.\n";
#ifdef DEBUG
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

    // ! lower_bounds and upper_bounds has errors.
    postProcess(outer_block, info_manager, expr_trees);

    // Deposit will apply the changes to the original AVN IR.
    for (const auto &tree : expr_trees) {
        tree->deposit();
    }
}

} // namespace extract