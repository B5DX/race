#include "Solver/IVInfo.h"
#include <iostream>
#include <map>
#include <queue>
#include <set>

void IVInfoManager::registerNativeArray(const shared_ptr<ir::ExprTree> &tree) {
    // Native array's operands, disp, index_vars are empty.
    for (const auto &tree_node : *(tree)) {
        auto expr = tree_node->getExpression();
        if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(expr)) {
            auto array_name = arr_ref->getArrayName()->to_string();
            if (!hasIVInfo(array_name)) {
                IVInfo iv_info;
                iv_info.referenced_cnt = 0;
                for (const auto &s : arr_ref->getSubscripts()) {
                    if (s.isAipb()) {
                        iv_info.index_vars.push_back(s.getI()->to_string());
                    }
                }
                iv_info_map[array_name] = iv_info;
            }
        } else if (ir::llvm_isa<ir::DeclRef>(expr) || ir::llvm_isa<ir::Number>(expr)) { // DeclRef or Number
            auto name = expr->to_string();
            if (!hasIVInfo(name)) {
                iv_info_map[name] = IVInfo();
            }
        }
        // None of the above, which means it's an sin or cos in CallOp, do nothing.
    }
}

void IVInfoManager::registerArray(string array, shared_ptr<DeltaNode> delta_node) {
    if (hasIVInfo(array))
        return;

    using Subs = vector<ir::Subscript>;
    IVInfo info;
    shared_ptr<ir::ExprTreeNode> l_expr_node = delta_node->getLeft();
    shared_ptr<ir::ExprTreeNode> r_expr_node = delta_node->getRight();
    info.op = l_expr_node->getParent()->to_string();
    info.referenced_cnt = 0;
    info.subs.push_back(Subs());
    info.subs.push_back(Subs());
    auto l_expr = l_expr_node->getExpression();
    auto r_expr = r_expr_node->getExpression();

    Subs l_subs, r_subs;
    std::map<string, int> shift_coef;
    std::map<string, rational> l_firstIdxOffset, r_firstIdxOffset;
    // Set operands, left and right's reference count. Get data for subsequent processing.
    if (auto l_arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(l_expr)) {
        l_firstIdxOffset = l_arr_ref->getFirstIdxOffset();
        string l_arr_name = l_arr_ref->getArrayName()->to_string();
        l_subs = l_arr_ref->getSubscripts();
        iv_info_map[l_arr_name].referenced_cnt++;
        iv_info_map[l_arr_name].parent.push_back(array);
        info.operands.push_back(l_arr_name);
    } else {
        string l_arr_name = l_expr->to_string();
        iv_info_map[l_arr_name].referenced_cnt++;
        iv_info_map[l_arr_name].parent.push_back(array);
        info.operands.push_back(l_arr_name);
    }
    if (auto r_arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(r_expr)) {
        r_firstIdxOffset = r_arr_ref->getFirstIdxOffset();
        string r_arr_name = r_arr_ref->getArrayName()->to_string();
        r_subs = r_arr_ref->getSubscripts();
        iv_info_map[r_arr_name].referenced_cnt++;
        iv_info_map[r_arr_name].parent.push_back(array);
        info.operands.push_back(r_arr_name);
    } else {
        string r_arr_name = r_expr->to_string();
        iv_info_map[r_arr_name].referenced_cnt++;
        iv_info_map[r_arr_name].parent.push_back(array);
        info.operands.push_back(r_arr_name);
    }

    // Set shift_coef
    for (const auto &l_str_off : l_firstIdxOffset) {
        if (r_firstIdxOffset.find(l_str_off.first) == r_firstIdxOffset.end()) {
            shift_coef[l_str_off.first] = l_str_off.second.numerator() / l_str_off.second.denominator();
        } else {
            const auto &r_offset = r_firstIdxOffset[l_str_off.first];
            rational mid_offset = (l_str_off.second + r_offset) / 2;
            shift_coef[l_str_off.first] = mid_offset.numerator() / mid_offset.denominator();
        }
    }
    for (const auto &r_str_off : r_firstIdxOffset) {
        if (l_firstIdxOffset.find(r_str_off.first) == l_firstIdxOffset.end()) {
            shift_coef[r_str_off.first] = r_str_off.second.numerator() / r_str_off.second.denominator();
        }
    }
    // Set the subscripts of represent expression.
    for (auto &l_sub : l_subs) {
        if (l_sub.isAipb()) {
            l_sub.setB(l_sub.getB() - shift_coef[l_sub.getI()->to_string()] * l_sub.getA());
        }
    }
    for (auto &r_sub : r_subs) {
        if (r_sub.isAipb()) {
            r_sub.setB(r_sub.getB() - shift_coef[r_sub.getI()->to_string()] * r_sub.getA());
        }
    }
    info.subs[0] = l_subs;
    info.subs[1] = r_subs;

    if (parent_for) { // Empty parent_for means there are only DeclRefs or Numbers in all trees, no ArrayRef.
        const auto &loop_vars = parent_for->getLoopVars();
        for (auto var : loop_vars) {
            if (l_firstIdxOffset.find(var->to_string()) != l_firstIdxOffset.end() || r_firstIdxOffset.find(var->to_string()) != r_firstIdxOffset.end()) {
                info.index_vars.push_back(var->to_string());
            }
        }
    }

    iv_info_map[array] = info;
}

void IVInfoManager::registerUsage(shared_ptr<ir::ExprTreeNode> expr_node) {
    string array = expr_node->to_string();
    if (!hasIVInfo(array) || isNativeArray(array)) {
        std::cout << "Register Usage Error: " << array << std::endl;
        exit(0);
    }
    auto &info = iv_info_map[array];
    auto cur_expr = expr_node->getExpression();
    if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(cur_expr)) {
        auto subs = arr_ref->getSubscripts();
        if (info.used_max_delta.empty()) { // First time to registerUsage for this array.
            for (const auto &sub : subs) {
                if (sub.isAipb()) {
                    string idx_str = sub.getI()->to_string();
                    info.used_max_delta[idx_str] = sub.getB();
                    info.used_min_delta[idx_str] = sub.getB();
                }
            }
        } else {
            for (const auto &sub : subs) {
                if (sub.isAipb()) {
                    string idx_str = sub.getI()->to_string();
                    info.used_max_delta[idx_str] = std::max(info.used_max_delta[idx_str], sub.getB());
                    info.used_min_delta[idx_str] = std::min(info.used_min_delta[idx_str], sub.getB());
                }
            }
        }
    } else {
        return;
    }
}

shared_ptr<ir::Expression> IVInfoManager::getRefExpr(string array, shared_ptr<DeltaNode> delta_node) {
    if (!hasIVInfo(array)) {
        std::cout << array << "is not registered but getRefExpr is called." << std::endl;
        exit(0);
    }
    shared_ptr<ir::ExprTreeNode> l_expr_node = delta_node->getLeft();
    shared_ptr<ir::ExprTreeNode> r_expr_node = delta_node->getRight();
    auto l_expr = l_expr_node->getExpression();
    auto r_expr = r_expr_node->getExpression();
    std::map<string, int> cur_expr_delta;
    auto &info = iv_info_map[array];
    auto exprs = {l_expr, r_expr};
    for (size_t i = 0; i < 2; i++) {
        auto expr = exprs.begin()[i];
        if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(expr)) {
            auto child_arr_sub = arr_ref->getFirstIdxOffset();
            for (const auto &this_sub : info.subs[i]) {
                if (this_sub.isAipb()) {
                    string idx_str = this_sub.getI()->to_string();
                    if (cur_expr_delta.find(idx_str) == cur_expr_delta.end()) {
                        auto delta = child_arr_sub[idx_str] - rational(this_sub.getB(), this_sub.getA());
                        if (delta.denominator() != 1) {
                            std::cout << "Error: getRefExpr: " << array << std::endl;
                            exit(0);
                        }
                        cur_expr_delta[idx_str] = delta.numerator();
                    }
                }
            }
        }
    }

    if (cur_expr_delta.empty()) { // No ArrayRef in l_expr or r_expr.
        return std::make_shared<ir::DeclRef>(array);
    } else {
        vector<ir::Subscript> subs;
        for (const auto &idx_str : iv_info_map[array].index_vars) {
            subs.push_back(
                ir::Subscript(ir::Subscript::aipb, 1, make_shared<ir::DeclRef>(idx_str), cur_expr_delta[idx_str]));
        }
        return std::make_shared<ir::ArrayRef>(array, subs, parent_for);
    }
}

vector<string> IVInfoManager::getTopologicalOrder(std::set<string> explicit_iv) {
    vector<string> res;
    std::map<string, bool> visited;
    for (const auto &iv : iv_info_map) {
        visited[iv.first] = false;
    }
    std::function<bool(string)> isValid = [&](string iv) {
        if (isNativeArray(iv) || visited[iv]) {
            return false;
        }
        return true;
    };

    std::function<void(string)> dfs = [&](string cur) {
        visited[cur] = true;
        for (string next : iv_info_map[cur].operands) {
            if (isValid(next)) {
                dfs(next);
            }
        }
        if (explicit_iv.empty()) { // If explicit_iv is empty, all the intermediate variables are retained.
            res.push_back(cur);
        } else { // If explicit_iv is not empty, only push the explicit ivs.
            if (explicit_iv.find(cur) != explicit_iv.end()) {
                res.push_back(cur);
            }
        }
    };
    for (auto str_info : iv_info_map) {
        if (isValid(str_info.first)) {
            dfs(str_info.first);
        }
    }
    std::reverse(res.begin(), res.end());
    return res;
}

std::map<string, vector<string>> IVInfoManager::getNewOperandsMap(std::set<string> explicit_iv) {
    std::map<string, vector<string>> new_operands_map;
    for (const auto &cur_iv_str : explicit_iv) {
        auto cur_info = iv_info_map[cur_iv_str];
        std::queue<string> q;
        for (const auto &operand : cur_info.operands) {
            q.push(operand);
        }
        while (!q.empty()) {
            auto top = q.front();
            q.pop();
            if (isNativeArray(top)) {
                continue;
            }
            if (explicit_iv.find(top) != explicit_iv.end()) {
                new_operands_map[cur_iv_str].push_back(top);
            } else { // If top is implicit, expand its operands. Native array has no operands so it will push nothing.
                auto top_info = iv_info_map[top];
                for (const auto &operand : top_info.operands) {
                    q.push(operand);
                }
            }
        }
    }
    // Print
    // for (const auto &iter : new_operands_map) {
    //     std::cout << "New Operands of " << iter.first << ": ";
    //     for (const auto &operand : iter.second) {
    //         std::cout << operand << ", ";
    //     }
    //     std::cout << std::endl;
    // }
    return new_operands_map;
}

void IVInfoManager::buildRangeCircleGraph(std::set<string> explicit_iv) {
    // Recursively convey the range information from the highest level to the lowest level.
    // ! explicit_iv should not be passed to this function, since we need to convey the range information to all intermediate variables.
    auto topo_order = this->getTopologicalOrder();
    for (auto iter = topo_order.begin(); iter < topo_order.end(); iter++) {
        // Convey the range information from cur_iv to its operands.
        const auto &cur_iv_str = *iter;
        const auto &cur_iv = iv_info_map[cur_iv_str];
        for (size_t idx = 0; idx < cur_iv.operands.size(); idx++) {
            const auto &operand = cur_iv.operands[idx];
            if (isNativeArray(operand)) {
                continue;
            }
            // operand here is an intermediate variable. Its subscripts must all be "1*i+b" form.
            // range information is conveyed from cur_iv to its operand.
            auto &operand_info = iv_info_map[operand];
            for (const auto &sub : cur_iv.subs[idx]) {
                string idx_str = sub.getI()->to_string();
                if (operand_info.used_max_delta.find(idx_str) == operand_info.used_max_delta.end()) {
                    operand_info.used_min_delta[idx_str] = cur_iv.used_min_delta.at(idx_str) + sub.getB();
                    operand_info.used_max_delta[idx_str] = cur_iv.used_max_delta.at(idx_str) + sub.getB();
                } else {
                    operand_info.used_min_delta[idx_str] = std::min(operand_info.used_min_delta[idx_str],
                                                                    cur_iv.used_min_delta.at(idx_str) + sub.getB());
                    operand_info.used_max_delta[idx_str] = std::max(operand_info.used_max_delta[idx_str],
                                                                    cur_iv.used_max_delta.at(idx_str) + sub.getB());
                }
            }
        }
    }
#ifdef DEBUG
    this->printInfo();
#endif

    // Build range circle graph using the information conveyed above.
    // The key of the map is the range, and the value is the intermediate variables with the same range.
    std::map<
        std::map<string, std::pair<int, int>>,
        std::set<string>>
        range_circle_map;

    // Eliminate the omitted intermediate variables in the topo_order.
    // So the range_circle_map only contains the explicit ivs.
    topo_order = this->getTopologicalOrder(explicit_iv);
    for (const auto &cur_iv_str : topo_order) {
        auto cur_iv = iv_info_map[cur_iv_str];
        std::map<string, std::pair<int, int>> cur_iv_range;
        for (auto str_off : cur_iv.used_max_delta) {
            cur_iv_range[str_off.first] = {
                cur_iv.used_min_delta[str_off.first],
                cur_iv.used_max_delta[str_off.first]};
        }
        range_circle_map[cur_iv_range].insert(cur_iv_str);
    }

    // Get new operands information that expands the implicit ivs and omits invalid operands.
    std::map<string, vector<string>> new_operands_map = this->getNewOperandsMap(explicit_iv);
    std::map<string, std::set<string>> unique_new_operands_map;
    for (const auto &iter : new_operands_map) {
        unique_new_operands_map[iter.first] = std::set<string>(iter.second.begin(), iter.second.end());
    }

    // After setting the range circle map, we can get the range circle graph.
    std::map<string, shared_ptr<RangeCircle>> iv2circle;
    for (auto &range_ivs : range_circle_map) {
        auto &packed_range = range_ivs.first;
        auto &ivs = range_ivs.second;
        auto range_circle = make_shared<RangeCircle>(ivs, packed_range);
        for (auto iv : ivs) {
            iv2circle[iv] = range_circle;
            iv_info_map[iv].range_circle = range_circle;
        }
        this->range_circle_graph.addNode(range_circle);
    }
    for (const auto &iv_circle : iv2circle) {
        auto &iv = iv_circle.first;
        auto &circle = iv_circle.second;
        for (const auto &operands : unique_new_operands_map[iv]) {
            if (iv2circle[operands] != circle) {
                this->range_circle_graph.addEdge(circle, iv2circle[operands]);
            }
        }
    }
    // Finish building range circle graph.
#ifdef DEBUG
    // print range circle graph
    for (const auto &circle : this->range_circle_graph.getNodes()) {
        std::cout << "Circle " << circle << ": ";
        for (const auto &iv : circle->iv_arrays) {
            std::cout << iv << ", ";
        }
        std::cout << std::endl;
        std::cout << "Range-max-delta: ";
        for (const auto &r : circle->used_max_delta) {
            std::cout << "(" << r.first << ", " << r.second << "), ";
        }
        std::cout << std::endl;
        std::cout << "Range-min-delta: ";
        for (const auto &r : circle->used_min_delta) {
            std::cout << "(" << r.first << ", " << r.second << "), ";
        }
        std::cout << std::endl;
    }
    for (const auto &source : this->range_circle_graph.getNodes()) {
        for (const auto &sink : this->range_circle_graph.getNeighbors(source)) {
            std::cout << "Edge: " << source << " -> " << sink << std::endl;
        }
    }
#endif
}

// TODO: haven't support "--no-contraction" option.
void IVInfoManager::arrayContraction(shared_ptr<ir::AVNBlock> avn_block, std::set<string> explicit_iv) {
    this->buildRangeCircleGraph(explicit_iv);
    // Skip the array reference independent loop.
    // e.g. for t -> for j -> for i: T[j][i] = A[i+1] + B[j][i-1]. Loop t is independent.
    // Then loop t should be put outside the optimization and generation process.
    auto innermost_for = ir::llvm_pointer_cast<ir::ForStmt>(avn_block->getBlock()->front());
    while (true) {
        if (auto next = ir::llvm_pointer_cast<ir::ForStmt>(innermost_for->getBody()->front())) {
            innermost_for = next;
        } else {
            break;
        }
    }
    std::set<string> used_idx_vars;
    auto collect_used_idx_vars = [&](shared_ptr<ir::ArrayRef> arf) {
        for (const auto &sub : arf->getSubscripts()) {
            if (sub.isAipb()) {
                used_idx_vars.insert(sub.getI()->to_string());
            }
        }
    };
    for (auto &stmt : *innermost_for->getBody()) {
        auto assign = ir::llvm_pointer_cast<ir::Assignment>(stmt);
        if (!assign) {
            continue;
        }
        auto lhs = assign->getLHS();
        auto rhs = assign->getRHS();
        ir::for_each_expr<ir::ArrayRef>(lhs, collect_used_idx_vars);
        ir::for_each_expr<ir::ArrayRef>(rhs, collect_used_idx_vars);
    }
    // Skip the outermost loop that is not used by any array reference.
    shared_ptr<ir::Block> skipped_block = avn_block->getBlock();
    while (auto outer_for = ir::llvm_pointer_cast<ir::ForStmt>(skipped_block->front())) {
        string idx_var = outer_for->getLoopVar()->to_string();
        if (used_idx_vars.find(idx_var) == used_idx_vars.end()) {
            skipped_block = outer_for->getBody();
        } else {
            break;
        }
    }
    auto topo_order = this->range_circle_graph.getTopologicalOrder();
    // Compute the index_mod and insert location.
    for (auto &circle : topo_order) {
        shared_ptr<ir::Block> cur_block = skipped_block;
        // If contraction is disabled, keep all used index variables unchanged, 
        // i.e. keep index_mod = 0, which is set when initialized.
        if (!global_config.contraction) {
            while (auto cur_for = ir::llvm_pointer_cast<ir::ForStmt>(cur_block->front())) {
                string loop_var = cur_for->getLoopVar()->to_string();
                if (circle->used_max_delta.find(loop_var) != circle->used_max_delta.end()) {
                    circle->insert_loc = cur_block;
                    break;
                }
                cur_block = cur_for->getBody();
            }
            if (!circle->insert_loc) {
                circle->insert_loc = cur_block;
            }
            continue;
        }

        // If reach here, contraction is enabled.
        // For each circle, try to enter each loop as inner as possible.
        while (auto cur_for = ir::llvm_pointer_cast<ir::ForStmt>(cur_block->front())) {
            string loop_var = cur_for->getLoopVar()->to_string();
            // If vectorization needs to be utilized, the innermost loop should not be compressed.
            if (global_config.vectorization) {
                // cur_for is the innermost loop.
                if (!ir::llvm_isa<ir::ForStmt>(cur_for->getBody()->front())) {
                    //! Set index_mod of innermost loop only if the loop_var is used by the range circle.
                    if (circle->used_max_delta.find(loop_var) != circle->used_max_delta.end()) {
                        circle->index_mod[loop_var] = 0;
                    }
                    circle->insert_loc = cur_block;
                    break;
                }
            }
            if (circle->used_max_delta.find(loop_var) != circle->used_max_delta.end()) {
                // loop var is found, try to enter the loop.
                int max_delta = circle->used_max_delta[loop_var];
                int min_delta = circle->used_min_delta[loop_var];
                if (max_delta == min_delta) {
                    // matches this loop, enter it successfully.
                    circle->index_mod[loop_var] = 1;
                    cur_block = cur_for->getBody();
                    if (cur_block->empty()) {
                        std::cout << "Empty Block Error." << std::endl;
                        exit(0);
                    }
                    continue;
                } else {
                    // circle is splited. Stop trying.
                    circle->index_mod[loop_var] = max_delta - min_delta + 1;
                    circle->insert_loc = cur_block;
                    break;
                }
            } else {
                // loop var is not found, dump code here.
                circle->insert_loc = cur_block;
                break;
            }
            cur_block = cur_for->getBody();
            if (cur_block->empty()) {
                std::cout << "Empty Block Error." << std::endl;
                exit(0);
            }
        }
        if (!circle->insert_loc) {
            circle->insert_loc = cur_block;
        }
    }
}

void IVInfoManager::generatePrecomputeLoop(shared_ptr<ir::AVNBlock> avn_block, std::set<string> ori_used_iv, std::set<string> &explicit_iv) {
    using std::pair;
    using namespace ir;
    // 1: Add profitable ivs.
    for (const auto &str_info : iv_info_map) {
        const auto &iv = str_info.first;
        const auto &info = str_info.second;
        bool flag = false;
        // if (info.referenced_cnt > 2) flag = true;
        if (info.op == "call") flag = true;
        if (flag) {
            explicit_iv.insert(iv);
        }
    }
    // 2: All used_ivs must be explicit_iv
    this->ori_used_iv = ori_used_iv;
    for (const auto &s : ori_used_iv) {
        explicit_iv.insert(s);
    }
    // 3: Add large leaf counts ivs.
    if (global_config.max_leaves < INT32_MAX) {
        std::function<int(string)> getLeafCnt = [&](string iv) {
            if (isNativeArray(iv)) {
                return 1;
            }
            if (explicit_iv.find(iv) != explicit_iv.end()) {
                return 1;
            }
            int res = 0;
            for (const auto &operand : iv_info_map[iv].operands) {
                res += getLeafCnt(operand);
            }
            return res;
        };
        for (const auto &str_info : iv_info_map) {
            const auto &iv = str_info.first;
            if (getLeafCnt(iv) > global_config.max_leaves) {
                explicit_iv.insert(iv);
            }
        }
    }
    // 4: Erase the native arrays from explicit_iv.
    for (auto iter = explicit_iv.begin(); iter != explicit_iv.end();) {
        if (this->isNativeArray(*iter)) {
            iter = explicit_iv.erase(iter);
        } else {
            iter++;
        }
    }
    if (global_config.contraction) {
        // Begin Optimization 1:
        // If an auxiliary array has only one parent (including the original loop's usage parent),
        // it can be replaced by its representative expression.
        // The parent should be the real parent given explicit_iv, instead of the original parent.
        // So the current parent relation should be computed and stored in parent_of.
        auto temp_operands_map = this->getNewOperandsMap(explicit_iv);
        std::map<string, int> parent_size;
        for (const auto &iter : temp_operands_map) {
            for (const auto &operand : iter.second) {
                parent_size[operand]++;
            }
        }
        for (auto iter = explicit_iv.begin(); iter != explicit_iv.end();) {
            string array = *iter;
            auto &info = this->iv_info_map[array];
            if (parent_size[array] == 1 && (ori_used_iv.find(array) == ori_used_iv.end())) {
                iter = explicit_iv.erase(iter);
            } else {
                ++iter;
            }
        }
        // End Optimization 1.
    }

    // Preprocess end.

    this->arrayContraction(avn_block, explicit_iv);
    const auto &range_circle_graph = this->getRangeCircleGraph();
    const auto &range_circles = range_circle_graph.getTopologicalOrder();
    // Use this map to bind each range circle with its corresponding for statement.
    // This is used to avoid changing the structure of the original loop nest before finishing constructing all the precompute loops.
    std::map<shared_ptr<RangeCircle>, shared_ptr<Statement>> circle2for;

    // Use this map to bind each range circle with its corresponding pipeline block. This is used for the split loop case.
    // The value of this map is a pair of the insert location and the pipeline block.
    // New loops are added in aftermost steps and the perfect original loop structure is destroyed. So the insert location is recorded.
    std::map<shared_ptr<RangeCircle>, pair<shared_ptr<Block>, shared_ptr<Statement>>> circle2pipeline;

    auto avn_innermost_for = avn_block->getInnermostFor();
    
    for (const auto &range_circle : range_circles) {
        auto insert_loc = range_circle->insert_loc;
        auto generateAssignmentBlock = [&](shared_ptr<RangeCircle> range_circle,
                                           ForStmt *parent_for, bool is_pipeline) -> shared_ptr<Block> {
            auto block = std::make_shared<Block>(parent_for);
            auto iv_topo_order = this->getTopologicalOrder(range_circle->iv_arrays);
            for (auto riter = iv_topo_order.rbegin(); riter < iv_topo_order.rend(); riter++) {
                const auto &iv_array = *riter;
                shared_ptr<Assignment> assign;
                shared_ptr<Expression> lhs, rhs;
                lhs = this->getContractedConstructLHS(iv_array, parent_for, is_pipeline);
                rhs = this->getContractedConstructRHS(iv_array, parent_for, explicit_iv, is_pipeline);
                assign = std::make_shared<Assignment>(lhs, rhs);
                block->push_back(assign);
            }
            return block;
        };
        auto insert_before_for_stmt = llvm_pointer_cast<ForStmt>(insert_loc->front());
        if (!insert_before_for_stmt || range_circle->index_mod.empty()) { 
            // Insert in the innermost loop || iv is DeclRef or Number (its index_mod will be empty).
            // No loop structure is needed. Just generate the assignment block.
            circle2for[range_circle] = generateAssignmentBlock(range_circle, insert_loc->getParentFor(), false);
        } else {
            // Record the candidate_for whose index variable is used by this range circle from innermost to outermost.
            // i.e. the outmost loop is that last element of candidate_for.
            vector<ForStmt*> candidate_for;
            auto cur_for = avn_innermost_for.get();
            unsigned int mod = 0;
            while (cur_for) {
                string idx_str = cur_for->getLoopVar()->to_string();
                if (range_circle->index_mod.find(idx_str) != range_circle->index_mod.end()) {
                    candidate_for.push_back(cur_for);
                    // Only retain the outmost mod, which is the mod we only care about.
                    // Because if outmost mod is 0, the inner loop's mod must be 0.
                    // if > 1, the inner ones must be 0.
                    // It's not possible that the outmost mod is 1.
                    mod = range_circle->index_mod[idx_str]; 
                }

                if (cur_for == insert_before_for_stmt.get()) {
                    break;
                } else {
                    cur_for = cur_for->getParentFor();
                }
            }
            // If no candidate_for, it means that the loops' index after insert_loc has nothing to do with the range circle.
            // So this range circle should be compressed to a scalar.
            if (candidate_for.empty()) {
                circle2for[range_circle] = generateAssignmentBlock(range_circle, insert_loc->getParentFor(), false);
                continue;
            }
            // Else, generate the loop structure by the reverse order of candidate_for.
            string outer_idx_str = candidate_for.back()->getLoopVar()->to_string();
            if (mod == 1) {
                std::cout << "Error: mod should not be 1." << std::endl;
                exit(0);
            }
            pair<int, int> used_min_max = {range_circle->used_min_delta[outer_idx_str], range_circle->used_max_delta[outer_idx_str]};
            pair<shared_ptr<Expression>, shared_ptr<Expression>> for_min_max = {candidate_for.back()->getBegin(), candidate_for.back()->getEnd()};
            auto new_begin = buildExpression(for_min_max.first, used_min_max.first);
            shared_ptr<Expression> new_end;
            if (mod == 0) {
                new_end = buildExpression(for_min_max.second, used_min_max.second);
            } else { // mod == x, which means it should be splited (mod will not be 1 since the loop in the insert_loc bans the compression).
                new_end = buildExpression(for_min_max.first, used_min_max.second - 1);
            }
            // Build the outmost loop new_for.
            auto new_for = std::make_shared<ForStmt>(outer_idx_str, new_begin, new_end, insert_before_for_stmt->getParentFor());
            circle2for[range_circle] = new_for;
            
            // Build the inner loop structure.
            auto parent_for = new_for;
            for (auto riter = candidate_for.rbegin() + 1; riter < candidate_for.rend(); riter++) {
                auto for_stmt = *riter;
                string idx_str = for_stmt->getLoopVar()->to_string();
                used_min_max = {range_circle->used_min_delta[idx_str], range_circle->used_max_delta[idx_str]};
                for_min_max = {for_stmt->getBegin(), for_stmt->getEnd()};
                new_for = std::make_shared<ForStmt>(for_stmt->getLoopVar(), buildExpression(for_min_max.first, used_min_max.first),
                                                    buildExpression(for_min_max.second, used_min_max.second), parent_for.get());
                parent_for->getBody()->push_back(new_for);
                parent_for = new_for;
            }
            parent_for->setBody(generateAssignmentBlock(range_circle, parent_for.get(), false));

            // If mod > 1, generate pipeline block.
            if (mod > 1) {
                // Clone the body of the new for since the structure of pipeline block is the same as new_for's body.
                // Only the innermost assignmens need to be changed.
                new_for = llvm_pointer_cast<ForStmt>(circle2for[range_circle]);
                auto inner_region = llvm_pointer_cast<Block>(new_for->getBody()->clone());
                auto innermost_block = inner_region;
                while (true) {
                    if (auto inner_for = llvm_pointer_cast<ForStmt>(innermost_block->front())) {
                        innermost_block = inner_for->getBody();
                    } else {
                        break;
                    }
                }
                // Replace the innermost assignments with pipeline version.
                auto new_body = generateAssignmentBlock(range_circle, parent_for.get(), true);
                innermost_block->getBody().clear();
                for (auto assign : *new_body) {
                    innermost_block->push_back(assign);
                }
                // New loops are added in later steps and the perfect original loop structure is destroyed.
                // So the pipeline insert location is recorded.
                auto pipeline_insert_loc = llvm_pointer_cast<ForStmt>(insert_loc->getBody().front())->getBody();
                if (auto for_stmt = llvm_pointer_cast<ForStmt>(inner_region->front())) {
                    circle2pipeline[range_circle] = {pipeline_insert_loc, for_stmt};
                } else {
                    circle2pipeline[range_circle] = {pipeline_insert_loc, inner_region};
                }
            }
        }
    }
    // Now all the precompute loops are constructed. We can insert them into the original loop nest.
    auto insertBlock = [&](shared_ptr<Block> insert_loc, shared_ptr<Statement> stmt) -> void {
        if (auto for_stmt = llvm_pointer_cast<ForStmt>(stmt)) {
            insert_loc->push_front(for_stmt);
            for_stmt->setParentFor(insert_loc->getParentFor(), false);
        } else { // stmt is a Block of Assignment.
            auto block = llvm_pointer_cast<Block>(stmt);
            block->setParentFor(insert_loc->getParentFor(), false);
            auto body = block->getBody();
            for (auto riter = body.rbegin(); riter < body.rend(); riter++) {
                insert_loc->push_front(*riter);
            }
        }
    };
    for (const auto &range_circle : range_circles) {
        auto insert_loc = range_circle->insert_loc;
        auto stmt = circle2for[range_circle];
        insertBlock(insert_loc, stmt);

        // If pipeline block is generated, insert it.
        if (circle2pipeline.find(range_circle) != circle2pipeline.end()) {
            auto pipeline_block = circle2pipeline[range_circle];
            insertBlock(pipeline_block.first, pipeline_block.second);
        }
    }
}

shared_ptr<ir::Expression>
IVInfoManager::getContractedConstructRHS(const string &array, ir::ForStmt *parent_for,
                                         const std::map<string, int> &shift_disp, const std::set<string> &explicit_iv) {
    if (!hasIVInfo(array) || isNativeArray(array)) {
        exit(0);
    }

    const auto &info = iv_info_map[array];
    vector<shared_ptr<ir::Expression>> res_operands = {nullptr, nullptr};
    // Construct left and right operands. The results are stored in res_operands.
    for (size_t i = 0; i < 2; i++) {
        shared_ptr<ir::Expression> &expr = res_operands[i];
        // Recursive base: left/right operand is a native array or used intermediate variable.
        if (this->isNativeArray(info.operands[i]) || explicit_iv.find(info.operands[i]) != explicit_iv.end()) {
            if (info.subs[i].empty()) { // expr is a DeclRef or Number.
                expr = std::make_shared<ir::DeclRef>(info.operands[i]);
            } else { // expr is an ArrayRef.
                // Firstly, set the correct subscripts of the represent expression of the operand.
                vector<ir::Subscript> res_subs;
                if (shift_disp.empty()) { // No shift.
                    res_subs = info.subs[i];
                } else { // Shift left operand by shift_disp.
                    auto subs = info.subs[i];
                    for (auto &s : subs) {
                        if (s.getI()) {
                            string idx_str = s.getI()->to_string();
                            if (shift_disp.find(idx_str) != shift_disp.end()) {
                                s.setB(s.getB() + shift_disp.at(idx_str) * s.getA());
                            }
                        }
                    }
                    res_subs = subs;
                }
                // Construct the represent expression.
                if (this->isNativeArray(info.operands[i])) { // Native array. Just generate in the simply manner.
                    expr = std::make_shared<ir::ArrayRef>(info.operands[i], res_subs, parent_for);
                } else { // expr is an iv array, range_circle is needed to generate the expression.
                    auto circle = iv_info_map[info.operands[i]].range_circle;
                    if (global_config.contraction) {
                        // Begin Optimization 2:
                        // An auxiliary array can be replaced by a scalar if all of its parents are in the same circle with it.
                        shared_ptr<RangeCircle> parent_circle = nullptr;
                        bool flag = true;
                        for (const string &parent : iv_info_map[info.operands[i]].parent) {
                            if (iv_info_map[parent].range_circle != circle) {
                                flag = false;
                                break;
                            }
                        }
                        //! If aa is used by the original loop, it can't be compressed to scalar.
                        if (this->ori_used_iv.find(info.operands[i]) != this->ori_used_iv.end()) {
                            flag = false;
                        }
                        // All parents are in the same circle with it.
                        if (flag) {
                            expr = std::make_shared<ir::DeclRef>(info.operands[i]);
                            continue;
                        }
                        // Else, generate the expression normally.
                        // End Optimization 2.
                    }
                    vector<shared_ptr<ir::Expression>> expr_subs;
                    expr_subs = circle->getExprSubByCircle(res_subs);
                    if (expr_subs.empty()) {
                        expr = std::make_shared<ir::DeclRef>(info.operands[i]);
                    } else {
                        expr = std::make_shared<ir::ArrayRef>(info.operands[i], expr_subs, parent_for, true);
                    }
                }
            }
        } else {                      // Recursive step: left/right operand is an intermediate variable and is not explicit.
            if (shift_disp.empty()) { // No shift.
                std::map<string, int> expr_shift;
                for (const auto &s : info.subs[i]) {
                    if (!s.isAipb()) { // Since operand is an intermediate variable, all subs are "1*i+b" form.
                        std::cout << "Error: getConstructRHS: " << array << std::endl;
                        exit(0);
                    }
                    expr_shift[s.getI()->to_string()] = s.getB();
                }
                expr = getContractedConstructRHS(info.operands[i], parent_for, expr_shift, explicit_iv);
            } else { // Shift left operand by shift_disp.
                auto expr_shift = shift_disp;
                for (auto &s : info.subs[i]) {
                    if (!s.isAipb()) {
                        std::cout << "Error: getConstructRHS: " << array << std::endl;
                        exit(0);
                    }
                    string idx_str = s.getI()->to_string();
                    if (expr_shift.find(idx_str) != expr_shift.end()) {
                        expr_shift[idx_str] += s.getB();
                    } else {
                        expr_shift[idx_str] = s.getB();
                    }
                }
                expr = getContractedConstructRHS(info.operands[i], parent_for, expr_shift, explicit_iv);
            }
        }
    }
    return ir::buildBinaryOp(info.op, res_operands[0], res_operands[1]);
}

shared_ptr<ir::Expression>
IVInfoManager::getContractedConstructRHS(const string &array, ir::ForStmt *parent_for,
                                         const std::set<string> &explicit_iv, bool is_pipeline) {
    if (!hasIVInfo(array) || isNativeArray(array)) {
        exit(0);
    }
    std::map<string, int> shift;
    auto range_circle = iv_info_map[array].range_circle;
    for (const auto &str_mod : range_circle->index_mod) {
        if (str_mod.second == 1) { // mod == 1 means used_min == used_max. Shift may be needed.
            // In this index, range_circle->used_min_delta and range_circle->used_max_delta are the same.
            if (range_circle->used_min_delta[str_mod.first] != 0) { // Shift is needed.
                shift[str_mod.first] = range_circle->used_min_delta[str_mod.first];
            }
        } else if (is_pipeline && str_mod.second > 1) {
            shift[str_mod.first] = range_circle->used_max_delta[str_mod.first];
        }
    }
    return getContractedConstructRHS(array, parent_for, shift, explicit_iv);
}

shared_ptr<ir::Expression>
IVInfoManager::getContractedArf(const string &array, ir::ForStmt *parent_for,
                                const std::map<string, int> &shift_disp) {
    if (!hasIVInfo(array) || isNativeArray(array)) {
        std::cout << "Error: getConstructLHS: " << array << std::endl;
        exit(0);
    }

    auto &info = iv_info_map[array];
    if (info.operands.empty()) {
        return std::make_shared<ir::DeclRef>(array);
    }

    auto circle = info.range_circle;
    if (global_config.contraction) {
        // Begin Optimization 2:
        shared_ptr<RangeCircle> parent_circle = nullptr;
        // If parent is empty, it must be used by original loop.
        //! The optimization can only be done when aa is not used by the original loop.
        //// if (!info.parent.empty()) {
        if (this->ori_used_iv.find(array) == this->ori_used_iv.end()) {
            bool if_opt = true;
            for (const string &parent : info.parent) {
                if (iv_info_map[parent].range_circle != circle) {
                    if_opt = false;
                    break;
                }
            }
            // All parents are in the same circle with it.
            if (if_opt) {
                return std::make_shared<ir::DeclRef>(array);
            }
        }
        // End Optimization 2.
    }

    vector<ir::Subscript> subs;
    for (const auto &idx_str : info.index_vars) {
        if (shift_disp.find(idx_str) != shift_disp.end()) {
            subs.push_back(ir::Subscript(ir::Subscript::aipb, 1, make_shared<ir::DeclRef>(idx_str), shift_disp.at(idx_str)));
        } else {
            subs.push_back(ir::Subscript(ir::Subscript::aipb, 1, make_shared<ir::DeclRef>(idx_str), 0));
        }
    }
    auto expr_subs = circle->getExprSubByCircle(subs);
    if (expr_subs.empty()) {
        return std::make_shared<ir::DeclRef>(array);
    } else {
        return std::make_shared<ir::ArrayRef>(array, expr_subs, parent_for, true);
    }
}

shared_ptr<ir::Expression> IVInfoManager::getContractedConstructLHS(const string &array, ir::ForStmt *parent_for, bool is_pipeline) {
    if (is_pipeline) {
        std::map<string, int> shift;
        auto range_circle = iv_info_map[array].range_circle;
        for (const auto &str_mod : range_circle->index_mod) {
            if (str_mod.second > 1) {
                shift[str_mod.first] = range_circle->used_max_delta[str_mod.first];
                break;
            }
        }
        return getContractedArf(array, parent_for, shift);
    } else {
        return getContractedArf(array, parent_for, {});
    }
}

void IVInfoManager::printInfo() {
    std::cout << "Registered Arrays: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << std::endl;
    }
    std::cout << "Intermediate Variables: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": ";
        if (isNativeArray(arr.first)) {
            std::cout << "native" << std::endl;
        } else {
            std::cout << arr.second.op << ": [";
            for (auto op : arr.second.operands)
                std::cout << op << " ";
            std::cout << "], subs: [";
            for (auto d : arr.second.subs) {
                std::cout << "[";
                for (auto i : d)
                    std::cout << "(" << i.to_string() << ") ";
                std::cout << "] ";
            }
            std::cout << "], index_vars: [";
            for (auto i : arr.second.index_vars)
                std::cout << i << " ";
            std::cout << "]" << std::endl;
            // std::cout << this->getConstructRHS(arr.first)->to_string() << std::endl;
        }
    }
    std::cout << "Referenced Count: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": " << arr.second.referenced_cnt << std::endl;
    }

    // print used_max/min_delta
    std::cout << "Used Min/Max Delta: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": [";
        for (auto i : arr.second.used_min_delta)
            std::cout << "(" << i.first << ", " << i.second << ") ";
        std::cout << "], [";
        for (auto i : arr.second.used_max_delta)
            std::cout << "(" << i.first << ", " << i.second << ") ";
        std::cout << "]" << std::endl;
    }

    // print parent
    std::cout << "Parent: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": [";
        for (auto i : arr.second.parent)
            std::cout << i << " ";
        std::cout << "]" << std::endl;
    }
    std::cout << "------------" << std::endl;
}
