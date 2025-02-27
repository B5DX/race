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
                if (auto arr_ref = llvm_pointer_cast<ArrayRef>(child->getExpression())) {
                    if (arr_ref->isValid())
                        cur_cand.push_back(child);
                } else { // Number or scalar variable.
                    cur_cand.push_back(child);
                }
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
            info_manager.registerArray(iv_name, iter.second[0]);
        } else {
            llvm::outs() << "Unsupported operator: " << l_tree_node->getParent()->getExpression()->to_string() << "\n";
            exit(0);
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

    // TODO: array definition generation.
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
            auto dim_cnt = arf->getIndices().size();
            string dim_str = "(:";
            for (size_t i = 1; i < dim_cnt; i++) {
                dim_str += ",:";
            }
            dim_str += ")";
            avn_block->addAVNVarDecl("real(kind=8), allocatable :: " + iv + dim_str);

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
                    alloc_size.push_back("0:" + std::to_string(mod-1));
                } else {
                    int max_delta = info.used_max_delta.at(idx_str);
                    int min_delta = info.used_min_delta.at(idx_str);

                    alloc_size.push_back(
                        ir::buildExpression(idx_for_map[idx_str]->getBegin(), min_delta)->to_string() + ":" +
                        ir::buildExpression(idx_for_map[idx_str]->getEnd(), max_delta)->to_string());
                }
            }
            string alloc_str = "allocate(" + iv + "(" + alloc_size[0];
            for (size_t i = 1; i < alloc_size.size(); i++) {
                alloc_str += ", " + alloc_size[i];
            }
            alloc_str += "))";
            avn_block->addAVNVarAlloc(alloc_str);
        } else {
            avn_block->addAVNVarDecl("real(kind=8) :: " + iv);
        }
    }
}

void scalarReplacement(shared_ptr<ir::Block> assign_block) {
    std::map<string, shared_ptr<ir::Expression>> s2e;
    std::map<string, size_t> s2idx;
    vector<bool> del_idx(assign_block->size(), 0);

    auto replaceScalar = [&](shared_ptr<ir::Operator> op) {
        if (auto bop = ir::llvm_pointer_cast<ir::BinaryOp>(op)) {
            auto operand = bop->getLHS();
            if (auto decl = ir::llvm_pointer_cast<ir::DeclRef>(operand)) {
                string var_name = decl->to_string();
                if (s2e.find(var_name) != s2e.end()) {
                    bop->setLHS(s2e[var_name]->clone());
                    del_idx[s2idx[var_name]] = 1;
                }
            }
            operand = bop->getRHS();
            if (auto decl = ir::llvm_pointer_cast<ir::DeclRef>(operand)) {
                string var_name = decl->to_string();
                if (s2e.find(var_name) != s2e.end()) {
                    bop->setRHS(s2e[var_name]->clone());
                    del_idx[s2idx[var_name]] = 1;
                }
            }
        } else if (auto uop = ir::llvm_pointer_cast<ir::UnaryOp>(op)) {
            auto operand = uop->getOperand();
            if (auto decl = ir::llvm_pointer_cast<ir::DeclRef>(operand)) {
                string var_name = decl->to_string();
                if (s2e.find(var_name) != s2e.end()) {
                    uop->setOperand(s2e[var_name]->clone());
                    del_idx[s2idx[var_name]] = 1;
                }
            }
        }
    };
    
    for (size_t i = 0; i < assign_block->size(); i++) {
        auto assign = ir::llvm_pointer_cast<ir::Assignment>(assign_block->at(i));
        assert(assign);
        auto lhs = assign->getLHS();
        auto rhs = assign->getRHS();
        ir::for_each_expr<ir::Operator>(rhs, replaceScalar);
        if (auto decl = ir::llvm_pointer_cast<ir::DeclRef>(lhs)) {
            string var_name = decl->to_string();
            s2e[var_name] = rhs;
            s2idx[var_name] = i;
        }
    }

    // delete the unused scalar assignment.
    vector<shared_ptr<ir::Statement>> &body = assign_block->getBody();
    size_t idx_cnt = 0;
    for (auto iter = body.begin(); iter != body.end();) {
        if (del_idx[idx_cnt++]) {
            iter = body.erase(iter);
        } else {
            ++iter;
        }
    }
}

void extract(shared_ptr<ir::AVNBlock> avn_block, int avn_cnt) {
    // Build ExprTree for each assignment.
    Forest expr_trees;
    shared_ptr<ir::Block> assignment_block = avn_block->getInnermostFor()->getBody();
    if (global_config.scalar_replacement) {
        scalarReplacement(assignment_block);
    }
    for (const auto &stmt : *assignment_block) {
        auto assign = ir::llvm_pointer_cast<ir::Assignment>(stmt);
        assert(assign);
        expr_trees.push_back(std::make_shared<ir::ExprTree>(assign));
    }
    vector<string> op_list = {"+", "-", "*", "/", "call", "**"}; // sin and cos is supported by using CallOp.
    std::map<string, vector<TreeNodeVec>> candidates;
    IVInfoManager info_manager(assignment_block->getParentFor());
    int iv_cnt = 0;
    for (auto op : op_list) {
        candidates[op] = vector<TreeNodeVec>();
    }

    // TODO: To implement the 1st aggressive extraction, we can ban the hoist and set commutable to false.
    // TODO: The 1st aggressive extraction can be relaxed by allowing the "+" hoist while banning the "*" hoist(commutable should also be set accordingly).
    for (auto tree : expr_trees) {
        tree->hoist();
#ifdef DEBUG
        llvm::outs() << "--------printTree--------\n";
        tree->printTree();
#endif
        info_manager.registerNativeArray(tree);
    }
#ifdef DEBUG
    llvm::outs() << "--------printInfo--------\n";
    info_manager.printInfo();
#endif
    while (true) { // Recursively extract.
        candidates.clear();
        for (const auto &tree : expr_trees) {
            collectCandidates(tree, candidates);
        }
#ifdef DEBUG
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
#ifdef DEBUG
        // std::cout << "-------------Graph-------------\n";
        // delta_graph.printGraph();
#endif
        // Solve the delta-graph.
        vector<shared_ptr<DeltaNode>> result;
        shared_ptr<Solver> solver = std::make_shared<DimFirstSolver>(info_manager.getParentFor()->getLoopVars());
        // shared_ptr<Solver> solver = std::make_shared<OptSolver>();
        result = solver->solve(delta_graph);

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
        std::cout << "-------------After Replace-------------\n";
        for (const auto &tree : expr_trees) {
            tree->printTree();
        }
        info_manager.printInfo();
        std::cout << "-------------End of Extract-------------\n";
#endif
    }
#ifdef DEBUG
    std::cout << "-------------Post Process-------------\n";
#endif
    postProcess(avn_block, info_manager, expr_trees);

#ifdef DEBUG
    std::cout << "-------------Deposite-------------\n";
#endif
    // Deposit will apply the changes to the original AVN IR.
    for (const auto &tree : expr_trees) {
        tree->deposit();
    }
}

} // namespace extract