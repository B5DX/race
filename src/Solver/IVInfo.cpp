#include "Solver/IVInfo.h"
#include <iostream>
#include <queue>
#include <map>
#include <set>
using Disp = IVInfo::Disp;

void IVInfoManager::registerNativeArray(const shared_ptr<ir::ExprTree> &tree) {
    // Native array's operands, disp, index_vars are empty.
    for (const auto &tree_node : *(tree)) {
        auto expr = tree_node->getExpression();
        if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(expr)) {
            auto array_name = arr_ref->getArrayName()->to_string();
            if (!hasIVInfo(array_name)) {
                IVInfo iv_info;
                iv_info.referenced_cnt = 0;
                std::map<string, int> disp = arr_ref->getDisp();
                for (const auto &d : disp) {
                    iv_info.lower_bounds[d.first] = d.second;
                    iv_info.upper_bounds[d.first] = d.second;
                }
                iv_info_map[array_name] = iv_info;
            } else {
                auto &info = iv_info_map[array_name];
                auto disp = arr_ref->getDisp();
                for (const auto &d : disp) {
                    info.lower_bounds[d.first] = std::min(info.lower_bounds[d.first], d.second);
                    info.upper_bounds[d.first] = std::max(info.upper_bounds[d.first], d.second);
                }
            }
        } else if (ir::llvm_isa<ir::DeclRef>(expr) || ir::llvm_isa<ir::Number>(expr)) { // DeclRef or Number
            auto name = expr->to_string();
            if (!hasIVInfo(name)) {
                IVInfo iv_info;
                iv_info.referenced_cnt = 0;
                iv_info_map[name] = iv_info;
            }
        }
    }
}

void IVInfoManager::registerArray(string array, shared_ptr<ir::ExprTreeNode> sub_expr_node) {
    if (hasIVInfo(array))
        return;

    IVInfo info;
    info.referenced_cnt = 0;
    info.op = sub_expr_node->getParent()->to_string();
    auto sub_expr = sub_expr_node->getExpression();

    info.disp.push_back(vector<std::pair<string, int>>());
    std::map<string, int> lb, ub, delta;
    // Set operand, operand's reference count. Get data for disp, upper and lower bounds.
    if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(sub_expr)) {
        lb = iv_info_map[arr_ref->getArrayName()->to_string()].lower_bounds;
        ub = iv_info_map[arr_ref->getArrayName()->to_string()].upper_bounds;
        iv_info_map[arr_ref->getArrayName()->to_string()].referenced_cnt++;
        info.operands.push_back(arr_ref->getArrayName()->to_string());
        delta = arr_ref->getDisp();
    } else {
        lb = iv_info_map[sub_expr->to_string()].lower_bounds;
        ub = iv_info_map[sub_expr->to_string()].upper_bounds;
        iv_info_map[sub_expr->to_string()].referenced_cnt++;
        info.operands.push_back(sub_expr->to_string());
    }
    for (const auto &d : lb) {
        info.lower_bounds[d.first] = d.second;
        info.upper_bounds[d.first] = ub[d.first];
    }

    // Set disp equal to operand's indices.
    // e.g. registerArray(T, sin(A[j][i+1]) will register T[j][i] = sin(A[j][i]), instead of T[j][i] = sin(A[j][i+1]).
    if (parent_for) { // Empty parent_for means there are only DeclRefs or Numbers in all trees, no ArrayRef.
        const auto &loop_vars = parent_for->getLoopVars();
        for (auto var : loop_vars) {
            bool used_var = false;
            if (delta.find(var->to_string()) != delta.end()) {
                info.disp[0].push_back({var->to_string(), 0});
                used_var = true;
            }
            if (used_var) {
                info.index_vars.push_back(var->to_string());
            }
        }
    }
    iv_info_map[array] = info;
}

void IVInfoManager::registerArray(string array, shared_ptr<ir::ExprTreeNode> l_expr_node, shared_ptr<ir::ExprTreeNode> r_expr_node) {
    if (hasIVInfo(array))
        return;

    IVInfo info;
    info.op = l_expr_node->getParent()->to_string();
    info.referenced_cnt = 0;
    info.disp.push_back(vector<std::pair<string, int>>());
    info.disp.push_back(vector<std::pair<string, int>>());
    auto l_expr = l_expr_node->getExpression();
    auto r_expr = r_expr_node->getExpression();

    std::map<string, int> l_ub, l_lb, r_ub, r_lb;
    std::map<string, int> l_delta, r_delta;
    vector<std::pair<string, int>> l_disp, r_disp;
    // Set operands, left and right's reference count. Get data for disp, upper and lower bounds.
    if (auto l_arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(l_expr)) {
        auto &l_info = iv_info_map[l_arr_ref->getArrayName()->to_string()];
        l_ub = l_info.upper_bounds;
        l_lb = l_info.lower_bounds;
        l_delta = l_arr_ref->getDisp();
        l_disp = l_arr_ref->getOrderedDisp();
        l_info.referenced_cnt++;
        info.operands.push_back(l_arr_ref->getArrayName()->to_string());
    } else {
        info.operands.push_back(l_expr->to_string());
        iv_info_map[l_expr->to_string()].referenced_cnt++;
    }
    if (auto r_arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(r_expr)) {
        auto &r_info = iv_info_map[r_arr_ref->getArrayName()->to_string()];
        r_ub = r_info.upper_bounds;
        r_lb = r_info.lower_bounds;
        r_delta = r_arr_ref->getDisp();
        r_disp = r_arr_ref->getOrderedDisp();
        r_info.referenced_cnt++;
        info.operands.push_back(r_arr_ref->getArrayName()->to_string());
    } else {
        info.operands.push_back(r_expr->to_string());
        iv_info_map[r_expr->to_string()].referenced_cnt++;
    }

    // Set disp
    // Merge disp and shift to middle. Shifted delta is written to l_delta and r_delta.
    // e.g. registerArray(T, A[j][i+1]+B[j][i+3]) will register T[j][i]=A[j][i-1]+B[j][i+1], instead of T[j][i]=A[j][i]+B[j][i+2] etc.
    for (auto &d : l_delta) {
        if (r_delta.find(d.first) == r_delta.end()) {
            d.second = 0;
        } else {
            int shift = (r_delta[d.first] + l_delta[d.first]) / 2;
            d.second -= shift;
            r_delta[d.first] -= shift;
        }
    }
    for (auto &d : r_delta) {
        if (l_delta.find(d.first) == l_delta.end()) {
            d.second = 0;
        }
    }
    if (parent_for) { // Empty parent_for means there are only DeclRefs or Numbers in all trees, no ArrayRef.
        const auto &loop_vars = parent_for->getLoopVars();
        for (auto var : loop_vars) {
            bool used_var = false;
            if (l_delta.find(var->to_string()) != l_delta.end() || r_delta.find(var->to_string()) != r_delta.end()) {
                info.index_vars.push_back(var->to_string());
            }
        }
        for (const auto &d : l_disp) {
            info.disp[0].push_back({d.first, l_delta[d.first]});
        }
        for (const auto &d : r_disp) {
            info.disp[1].push_back({d.first, r_delta[d.first]});
        }
    }

    // Set upper and lower bounds
    std::set<string> union_dims;
    for (const auto &d : l_ub) {
        union_dims.insert(d.first);
    }
    for (const auto &d : r_ub) {
        union_dims.insert(d.first);
    }
    for (auto it = union_dims.begin(); it != union_dims.end(); ++it) {
        int max_v = -1e9;
        int min_v = 1e9;
        if (l_lb.find(*it) != l_lb.end()) {
            max_v = std::max(max_v, l_lb[*it] - l_delta[*it]);
        }
        if (r_lb.find(*it) != r_lb.end()) {
            max_v = std::max(max_v, r_lb[*it] - r_delta[*it]);
        }
        if (l_ub.find(*it) != l_ub.end()) {
            min_v = std::min(min_v, l_ub[*it] - l_delta[*it]);
        }
        if (r_ub.find(*it) != r_ub.end()) {
            min_v = std::min(min_v, r_ub[*it] - r_delta[*it]);
        }
        info.lower_bounds[*it] = max_v;
        info.upper_bounds[*it] = min_v;
    }
    iv_info_map[array] = info;
}

void IVInfoManager::registerUsage(shared_ptr<ir::ExprTreeNode> expr_node) {
    string array = expr_node->to_string();
    if (!hasIVInfo(array)) {
        throw std::runtime_error("Array not registered but used.");
    }
    auto &info = iv_info_map[array];
    auto cur_expr = expr_node->getExpression();
    if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(cur_expr)) {
        auto delta = arr_ref->getDisp();
        if (info.used_max_delta.empty()) { // First time to registerUsage for this array.
            info.used_max_delta = delta;
            info.used_min_delta = delta;
        } else {
            for (const auto &d : delta) {
                info.used_max_delta[d.first] = std::max(info.used_max_delta[d.first], d.second);
                info.used_min_delta[d.first] = std::min(info.used_min_delta[d.first], d.second);
            }
        }
    } else {
        return;
    }
}

// std::map<string, int> getShiftedDelta(const std::map<string, int> &cur_disp, const std::map<string, int> &base) {
//     std::map<string, int> res;
//     for (const auto &d : cur_disp) {
//         if (base.find(d.first) != base.end()) {
//             res[d.first] = d.second - base.at(d.first);
//         }
//     }
//     return res;
// }

shared_ptr<ir::Expression> IVInfoManager::getRefExpr(string array, shared_ptr<ir::ExprTreeNode> l_expr_node, shared_ptr<ir::ExprTreeNode> r_expr_node) {
    if (!hasIVInfo(array)) {
        std::cout << array << ":" << l_expr_node->to_string() << ", " << r_expr_node->to_string() << std::endl;
        throw std::runtime_error("ir::ArrayRef not registered.");
    }
    auto l_expr = l_expr_node->getExpression();
    auto r_expr = r_expr_node->getExpression();
    std::map<string, int> cur_expr_delta;
    auto &info = iv_info_map[array];
    if (auto l_arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(l_expr)) {
        auto delta = l_arr_ref->getDisp();
        for (const auto &d : info.disp[0]) {
            cur_expr_delta[d.first] = delta[d.first] - d.second;
        }
    }
    if (auto r_arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(r_expr)) {
        auto delta = r_arr_ref->getDisp();
        for (const auto &d : info.disp[1]) {
            cur_expr_delta[d.first] = delta[d.first] - d.second;
        }
    }
    if (cur_expr_delta.empty()) { // No ArrayRef in l_expr or r_expr.
        return std::make_shared<ir::DeclRef>(array);    
    } else {
        vector<std::pair<string, int>> disp;
        for (const auto &idx : iv_info_map[array].index_vars) {
            disp.push_back({idx, cur_expr_delta[idx]});
            // *The following code may incur errors when compressing iv_array. Since the reference here may merged into another iv_array and may not be used in the final expression.
            // if (info.used_max_delta.find(idx) == info.used_max_delta.end()) { // First time to getRefExpr for this array.
            //     info.used_max_delta[idx] = cur_expr_delta[idx];
            //     info.used_min_delta[idx] = cur_expr_delta[idx];
            // } else { // Update used_max_delta and used_min_delta.
            //     info.used_max_delta[idx] = std::max(info.used_max_delta[idx], cur_expr_delta[idx]);
            //     info.used_min_delta[idx] = std::min(info.used_min_delta[idx], cur_expr_delta[idx]);
            // }
        }
        return std::make_shared<ir::ArrayRef>(array, disp, parent_for);
    }
}

shared_ptr<ir::Expression> IVInfoManager::getConstructRHS(const string &array, std::map<string, int> shift_disp) {
    if (!hasIVInfo(array)) {
        throw std::runtime_error("ir::ArrayRef not registered.");
    }
    if (isNativeArray(array)) {
        throw std::runtime_error("Native array has no construct expression.");
    }
    auto info = iv_info_map[array];
    shared_ptr<ir::Expression> l_expr, r_expr;
    if (this->isNativeArray(info.operands[0])) { // Recursive base: left operand is a native array.
        if (info.disp[0].empty()) { // l_expr is a DeclRef or Number.
            l_expr = std::make_shared<ir::DeclRef>(info.operands[0]);
        } else { // l_expr is an ArrayRef.
            if (shift_disp.empty()) { // No shift.
                l_expr = std::make_shared<ir::ArrayRef>(info.operands[0], info.disp[0], parent_for);
            } else { // Shift left operand by shift_disp.
                auto disp = info.disp[0];
                for (auto &d : disp) {
                    if (shift_disp.find(d.first) != shift_disp.end()) {
                        d.second += shift_disp[d.first];
                    }
                }
                l_expr = std::make_shared<ir::ArrayRef>(info.operands[0], disp, parent_for);
            }
        }
    } else { // Recursive step: left operand is an intermediate variable.
        if (shift_disp.empty()) { // No shift.
            std::map<string, int> l_expr_shift;
            for (const auto &d : info.disp[0]) {
                l_expr_shift[d.first] = d.second;
            }
            l_expr = getConstructRHS(info.operands[0], l_expr_shift);
        } else { // Shift left operand by shift_disp.
            auto l_expr_shift = shift_disp;
            for (auto &d : info.disp[0]) {
                if (l_expr_shift.find(d.first) != l_expr_shift.end()) {
                    l_expr_shift[d.first] += d.second;
                } else {
                    l_expr_shift[d.first] = d.second;
                }
            }
            l_expr = getConstructRHS(info.operands[0], l_expr_shift);
        }
    }

    // Do the same as above for r_expr
    if (info.operands.size() > 1) { // binary operator
        if (this->isNativeArray(info.operands[1])) {
            if (info.disp[1].empty()) {
                r_expr = std::make_shared<ir::DeclRef>(info.operands[1]);
            } else {
                if (shift_disp.empty()) {
                    r_expr = std::make_shared<ir::ArrayRef>(info.operands[1], info.disp[1], parent_for);
                } else {
                    auto disp = info.disp[1];
                    for (auto &d : disp) {
                        if (shift_disp.find(d.first) != shift_disp.end()) {
                            d.second += shift_disp[d.first];
                        }
                    }
                    r_expr = std::make_shared<ir::ArrayRef>(info.operands[1], disp, parent_for);
                }
            }
        } else {
            if (shift_disp.empty()) {
                std::map<string, int> r_expr_shift;
                for (const auto &d : info.disp[1]) {
                    r_expr_shift[d.first] = d.second;
                }
                r_expr = getConstructRHS(info.operands[1], r_expr_shift);
            } else {
                auto r_expr_shift = shift_disp;
                for (auto &d : info.disp[1]) {
                    if (r_expr_shift.find(d.first) != r_expr_shift.end()) {
                        r_expr_shift[d.first] += d.second;
                    } else {
                        r_expr_shift[d.first] = d.second;
                    }
                }
                r_expr = getConstructRHS(info.operands[1], r_expr_shift);
            }
        }
        return ir::buildBinaryOp(info.op, l_expr, r_expr);
    } else {
        return ir::buildUnaryOp(info.op, l_expr);
    }
}

vector<string> IVInfoManager::IVTopologicalSort() {
    vector<string> res;
    std::queue<string> iv_queue;
    std::map<string, int> iv_outdegree;
    std::map<string, std::set<string>> iv_backedge;
    for (auto arr : iv_info_map) {
        iv_outdegree[arr.first] = 0;
        std::set<string> unique_iv_expr(arr.second.operands.begin(), arr.second.operands.end());
        for (auto refered_arr : unique_iv_expr) {
            iv_backedge[refered_arr].insert(arr.first);
            iv_outdegree[arr.first] += 1;
        }
        if (iv_outdegree[arr.first] == 0) {
            iv_queue.push(arr.first);
        }
    }
    while (!iv_queue.empty()) {
        auto arr = iv_queue.front();
        iv_queue.pop();
        if (!isNativeArray(arr)) res.push_back(arr);
        for (auto parent : iv_backedge[arr]) {
            iv_outdegree[parent] -= 1;
            if (iv_outdegree[parent] == 0) {
                iv_queue.push(parent);
            }
        }
    }
    
#ifdef DEBUG
    for (auto i : res) {
        cout << i << ", ";
    }
    cout << endl;
#endif
    return res;
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
            std::cout << "], disp: [";
            for (auto d : arr.second.disp) {
                std::cout << "[";
                for (auto i : d)
                    std::cout << "(" << i.first << ", " << i.second << ") ";
                std::cout << "] ";
            }
            std::cout << "], index_vars: [";
            for (auto i : arr.second.index_vars)
                std::cout << i << " ";
            std::cout << "]" << std::endl;
        }
    }
    std::cout << "Referenced Count: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": " << arr.second.referenced_cnt << std::endl;
    }
    // print iv_info_map
    std::cout << "Upper/Lower Bounds: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": [";
        for (auto i : arr.second.lower_bounds)
            std::cout << "(" << i.first << ", " << i.second << ") ";
        std::cout << "], [";
        for (auto i : arr.second.upper_bounds)
            std::cout << "(" << i.first << ", " << i.second << ") ";
        std::cout << "]" << std::endl;
    }
    // print used_max/min_delta
    std::cout << "Used Max/Min Delta: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": [";
        for (auto i : arr.second.used_max_delta)
            std::cout << "(" << i.first << ", " << i.second << ") ";
        std::cout << "], [";
        for (auto i : arr.second.used_min_delta)
            std::cout << "(" << i.first << ", " << i.second << ") ";
        std::cout << "]" << std::endl;
    }
    std::cout << "------------" << std::endl;
}