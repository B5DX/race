// Each array index supports one loop variable with coefficient 1.
#include "Solver/Extractor.h"
#include "Solver/IVInfo.h"
#include "Solver/MatchSolver.h"
#include "llvm/Support/raw_ostream.h"

using namespace ir;

namespace extract {

void collectCandidates(shared_ptr<ExprTree> tree, std::map<string, vector<TreeNodeVec>> &result, const set<string> &modified_vars) {
    for (auto node : *tree) {
        if (node->isLeafNode()) {
            continue;
        }

        TreeNodeVec cur_cand;
        for (auto child : node->getChildren()) {
            if (child->isLeafNode()) {
                if (auto arr_ref = llvm_pointer_cast<ArrayRef>(child->getExpression())) {
                    if (arr_ref->isValid() &&
                        modified_vars.find(arr_ref->getArrayName()->to_string()) == modified_vars.end()) {
                        cur_cand.push_back(child);
                    }
                } else { // Number or scalar variable.
                    if (auto decl = llvm_pointer_cast<DeclRef>(child->getExpression())) {
                        if (modified_vars.find(decl->to_string()) == modified_vars.end()) {
                            cur_cand.push_back(child);
                        }
                    } else { // Number.
                        cur_cand.push_back(child);
                    }
                }
            }
        }
        if (cur_cand.size() > 1) {
            assert(node->isOperatorNode());
            result[node->to_string()].push_back(cur_cand);
        }
    }
}

// Each array index supports one loop variable with coefficient 1.
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
            info_manager.registerArray(iv_name, iter.second[0]);
        } else {
            llvm::errs() << "Unsupported operator: " << l_tree_node->getParent()->getExpression()->to_string() << "\n";
            throw std::runtime_error("Candidate parent must be a binary operator");
        }
        // Replace old array with new array.
        for (const auto &node : iter.second) {
            auto l_tree_node = node->getLeft();
            auto r_tree_node = node->getRight();
            auto parent = l_tree_node->getParent();
            parent->eraseChild(l_tree_node);
            parent->eraseChild(r_tree_node);
            auto new_arr = info_manager.getRefExpr(iv_name, node);
            auto new_node = make_shared<ir::ExprTreeNode>(new_arr, parent);
            parent->addChild(new_node);
            if (parent->getChildren().size() == 1) {
                auto pp = parent->getParent();
                if (pp != nullptr) {
                    size_t idx = pp->eraseChild(parent);
                    pp->addChild(new_node, idx);
                    new_node->setParent(pp);
                } else { // root node is replaced.
                    parent->eraseChild(new_node);
                    parent->setExpression(new_arr);
                }
            }
        }
    }
}

void postProcess(shared_ptr<ir::AVNBlock> avn_block, IVInfoManager &info_manager, Forest extracted_expr_tree) {
    std::set<string> iv_arrays = info_manager.getIVArray();
    std::set<string> used_iv;
    for (auto tree : extracted_expr_tree) {
        for (auto node : *tree) {
            if (iv_arrays.find(node->to_string()) != iv_arrays.end()) {
                used_iv.insert(node->to_string());
                info_manager.registerUsage(node);
            }
        }
    }
    std::set<string> explicit_iv;
    info_manager.generatePrecomputeLoop(avn_block, used_iv, explicit_iv);

    // Replace the iv-arf in the original expression with the compressed subscripts.
    // Conduct the replacement in the extracted_expr_tree since the expressions are not dumped yet.
    auto innermost_for = avn_block->getInnermostFor();
    for (auto &tree : extracted_expr_tree) {
        for (auto node : *tree) {
            if (!node->isLeafNode()) {
                continue;
            }
            string var_name = node->to_string();
            if (!info_manager.hasIVInfo(var_name) || info_manager.isNativeArray(var_name)) {
                continue;
            }
            if (auto arf = llvm_pointer_cast<ArrayRef>(node->getExpression())) {
                std::map<string, int> shift;
                auto ori_subs = arf->getSubscripts();
                for (const auto &sub : ori_subs) {
                    if (sub.isAipb()) {
                        string idx_str = sub.getI()->to_string();
                        shift[idx_str] = sub.getB();
                    }
                }
                auto new_arf = info_manager.getContractedArf(var_name, innermost_for.get(), shift);
                if (auto decl = llvm_pointer_cast<DeclRef>(new_arf)) {
                    node->setExpression(decl);
                } else {
                    node->setExpression(llvm_pointer_cast<ArrayRef>(new_arf));
                }
            } else { // DeclRef or Number. No need to replace.
                continue;
            }
        }
    }

    // TODO: The loop fusion for split circle is not supported.

    std::map<string, ForStmt *> idx_for_map;
    idx_for_map[innermost_for->getLoopVar()->to_string()] = innermost_for.get();
    ir::ForStmt *cur_for = innermost_for.get();
    while (cur_for->getParentFor()) {
        cur_for = cur_for->getParentFor();
        idx_for_map[cur_for->getLoopVar()->to_string()] = cur_for;
    }
    for (const auto &iv : explicit_iv) {
        auto temp_arf = info_manager.getContractedConstructLHS(iv, nullptr, false);
        const auto &info = info_manager.getIVInfo(iv);
        if (auto arf = llvm_pointer_cast<ArrayRef>(temp_arf)) {
            // Add AVNVarDecl.
            string alloc_str = "double " + iv;

            // Add AVNVarAlloc. e.g. allocate(array(js-2:je+5, 0:2))
            vector<string> alloc_size;
            for (auto idx : arf->getIndices()) {
                string idx_str;
                bool if_mod = false;
                if (auto decl = llvm_pointer_cast<DeclRef>(idx)) {
                    idx_str = decl->to_string();
                } else if (auto bop = llvm_pointer_cast<BinaryOp>(idx)) {
                    idx_str = bop->getLHS()->to_string();
                    if (llvm_isa<ModOp>(bop)) {
                        if_mod = true;
                    }
                }
                // Push current range to alloc_size.
                if (if_mod) {
                    long long mod = info.range_circle->index_mod.at(idx_str);
                    alloc_size.push_back(std::to_string(mod));
                } else {
                    int max_delta = info.used_max_delta.at(idx_str);
                    int min_delta = info.used_min_delta.at(idx_str);

                    alloc_size.push_back(
                        ir::buildExpression(idx_for_map[idx_str]->getEnd(), max_delta + 1)->to_string());
                }
            }
            alloc_str += "[" + alloc_size.back();
            for (auto iter = alloc_size.rbegin() + 1; iter != alloc_size.rend(); iter++) {
                alloc_str += "][" + *iter;
            }
            alloc_str += "];";
            avn_block->addAVNVarDecl(alloc_str);
        } else {
            avn_block->addAVNVarDecl("double " + iv + ";");
        }
    }
}

// Forward substitution within one iteration. Keep assignments: a scalar may be
// live after the region or needed by the next iteration. Invalidate saved RHSs
// whenever a dependency changes, including dependencies in array subscripts.
void scalarReplacement(shared_ptr<ir::Block> assign_block) {
    std::map<string, shared_ptr<ir::Expression>> values;
    std::function<shared_ptr<Expression>(shared_ptr<Expression>)> substitute =
        [&](shared_ptr<Expression> expr) -> shared_ptr<Expression> {
        if (auto decl = llvm_pointer_cast<DeclRef>(expr)) {
            auto it = values.find(decl->to_string());
            return it == values.end() ? expr : it->second->clone();
        }
        if (auto bop = llvm_pointer_cast<BinaryOp>(expr)) {
            bop->setLHS(substitute(bop->getLHS()));
            bop->setRHS(substitute(bop->getRHS()));
        } else if (auto uop = llvm_pointer_cast<UnaryOp>(expr)) {
            uop->setOperand(substitute(uop->getOperand()));
        }
        // Array indices retain their integer types and are not substituted.
        return expr;
    };
    std::function<bool(shared_ptr<Expression>, const string &)> depends =
        [&](shared_ptr<Expression> expr, const string &name) {
        if (auto decl = llvm_pointer_cast<DeclRef>(expr)) return decl->to_string() == name;
        if (auto arf = llvm_pointer_cast<ArrayRef>(expr)) {
            if (arf->getArrayName()->to_string() == name) return true;
            for (auto idx : arf->getIndices()) if (depends(idx, name)) return true;
        } else if (auto bop = llvm_pointer_cast<BinaryOp>(expr)) {
            return depends(bop->getLHS(), name) || depends(bop->getRHS(), name);
        } else if (auto uop = llvm_pointer_cast<UnaryOp>(expr)) {
            return depends(uop->getOperand(), name);
        }
        return false;
    };
    for (auto stmt : *assign_block) {
        auto assign = llvm_pointer_cast<Assignment>(stmt);
        auto rhs = substitute(assign->getRHS());
        assign->setRHS(rhs);
        auto decl = llvm_pointer_cast<DeclRef>(assign->getLHS());
        if (!decl) { values.clear(); continue; } // A store may alias any saved load.
        const string name = decl->to_string();
        for (auto it = values.begin(); it != values.end();) {
            if (it->first == name || depends(it->second, name)) it = values.erase(it);
            else ++it;
        }
        if (!depends(rhs, name)) values[name] = rhs->clone();
    }
}

void detection_loop(Forest &expr_trees, vector<string> &op_list, std::map<string, vector<TreeNodeVec>> &candidates, set<string> &modified_vars, IVInfoManager &info_manager, int &iv_cnt, int &avn_cnt) {
    while (true) { // Recursively extract.
        candidates.clear();
        for (const auto &tree : expr_trees) {
            collectCandidates(tree, candidates, modified_vars);
        }
#ifdef RACE_DEBUG
        llvm::outs() << "--------printCandidates--------\n";
        // print candidates
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
        // Build delta-graph.
        UDGraph<shared_ptr<DeltaNode>> delta_graph;
        bool commutable = global_config.commutable;
        // Begin to construct the delta-graph.
        for (auto op : op_list) {
            if (op == "-" || op == "/" || op == "call" || op == "**") {
                commutable = false;
            } else {
                commutable = global_config.commutable;
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
        // Solve the delta-graph.
        vector<shared_ptr<DeltaNode>> result;
        shared_ptr<Solver> solver = std::make_shared<DimFirstSolver>(info_manager.getParentFor()->getLoopVars());

#ifdef RACE_DEBUG
        std::cout << delta_graph.getNodes().size() << " candidates, ";
        size_t edge_cnt = 0;
        for (const auto &node : delta_graph.getNodes()) {
            edge_cnt += delta_graph.getNeighbors(node).size();
        }
        std::cout << edge_cnt/2 << " edges.\n";
#endif
        result = solver->solve(delta_graph);

        if (result.empty())
            break;

#ifdef RACE_DEBUG
        std::cout << "-------------Result-------------\n";
        for (const auto &node : result) {
            std::cout << *node << "\n";
        }
#endif
        replaceMatchCand(result, iv_cnt, avn_cnt, info_manager);
#ifdef RACE_DEBUG
        std::cout << "-------------After Replace-------------\n";
        for (const auto &tree : expr_trees) {
            tree->printTree();
        }
        info_manager.printInfo();
        std::cout << "-------------End of Extract-------------\n";
#endif
    }
}

void extract(shared_ptr<ir::AVNBlock> avn_block, int avn_cnt) {
    // Build ExprTree for each assignment.
    Forest expr_trees;
    shared_ptr<ir::Block> assignment_block = avn_block->getInnermostFor()->getBody();
    std::set<string> modified_vars;
    for (auto var : avn_block->getInnermostFor()->getLoopVars())
        modified_vars.insert(var->to_string());
    if (global_config.scalar_replacement) {
        scalarReplacement(assignment_block);
    }
    for (const auto &stmt : *assignment_block) {
        auto assign = ir::llvm_pointer_cast<ir::Assignment>(stmt);
        assert(assign);
        if (auto arf = ir::llvm_pointer_cast<ir::ArrayRef>(assign->getLHS())) {
            modified_vars.insert(arf->getArrayName()->to_string());
        } else if (auto decl = ir::llvm_pointer_cast<ir::DeclRef>(assign->getLHS())) {
            modified_vars.insert(decl->to_string());
        }
        expr_trees.push_back(std::make_shared<ir::ExprTree>(assign));
    }
    vector<string> op_list = {"+", "-", "*", "/", "call", "**"}; // sin and cos is supported by using CallOp.
    std::map<string, vector<TreeNodeVec>> candidates;
    shared_ptr<IVInfoManager> info_manager = make_shared<IVInfoManager>(assignment_block->getParentFor());
    int iv_cnt = 0;
    for (auto op : op_list) {
        candidates[op] = vector<TreeNodeVec>();
    }

    for (auto tree : expr_trees) {
        if (global_config.commutable) {
            tree->hoist();
        }
#ifdef RACE_DEBUG
        llvm::outs() << "--------printTree--------\n";
        tree->printTree();
#endif
        info_manager->registerNativeArray(tree);
    }
#ifdef RACE_DEBUG
    llvm::outs() << "--------printInfo--------\n";
    info_manager->printInfo();
#endif

    try {
        detection_loop(expr_trees, op_list, candidates, modified_vars, *info_manager, iv_cnt, avn_cnt);
    } catch (const SearchSpaceTooLarge& e) {
#ifdef RACE_DEBUG
        llvm::errs() << "Search space too large, fallback to no reassociation.\n";
#endif
        struct RestoreCommutability {
            bool original = global_config.commutable;
            ~RestoreCommutability() { global_config.commutable = original; }
        } restore;
        const bool original_commutable = restore.original;
        //* Failed because of too many delta nodes. Try no reassociation first and then reassociation.
        // Rebuild expr_trees and info_manager since the original ones may be modified by the solver.
        expr_trees.clear();
        for (const auto &stmt : *assignment_block) {
            auto assign = ir::llvm_pointer_cast<ir::Assignment>(stmt);
            assert(assign);
            expr_trees.push_back(std::make_shared<ir::ExprTree>(assign));
        }
        candidates.clear();
        info_manager = make_shared<IVInfoManager>(assignment_block->getParentFor());
        iv_cnt = 0;
        for (auto op : op_list) {
            candidates[op] = vector<TreeNodeVec>();
        }
        // No reassociation.
        global_config.commutable = false;
        for (auto tree : expr_trees) {
            if (global_config.commutable) {
                tree->hoist();
            }
            info_manager->registerNativeArray(tree);
        }
        detection_loop(expr_trees, op_list, candidates, modified_vars, *info_manager, iv_cnt, avn_cnt);
        // Reassociation.
        global_config.commutable = original_commutable;
        for (auto tree : expr_trees) {
            if (global_config.commutable) {
                tree->hoist();
            }
            info_manager->registerNativeArray(tree);
        }
        detection_loop(expr_trees, op_list, candidates, modified_vars, *info_manager, iv_cnt, avn_cnt);
    }

#ifdef RACE_DEBUG
    std::cout << "-------------Post Process-------------\n";
#endif
    postProcess(avn_block, *info_manager, expr_trees);

#ifdef RACE_DEBUG
    std::cout << "-------------Deposite-------------\n";
#endif
    // Deposit will apply the changes to the original AVN IR.
    for (const auto &tree : expr_trees) {
        tree->deposit();
    }
}

} // namespace extract
