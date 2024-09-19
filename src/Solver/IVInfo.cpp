#include "Solver/IVInfo.h"
#include <iostream>
using Disp = IVInfo::Disp;

void IVInfoManager::registerNativeArray(const shared_ptr<ir::ExprTree> &tree) {
    // Native array's operands, disp, indices are empty.
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
                info.indices.push_back(var->to_string());
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
    // Set operands, left and right's reference count. Get data for disp, upper and lower bounds.
    if (auto l_arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(l_expr)) {
        auto &l_info = iv_info_map[l_arr_ref->getArrayName()->to_string()];
        l_ub = l_info.upper_bounds;
        l_lb = l_info.lower_bounds;
        l_delta = l_arr_ref->getDisp();
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
        r_info.referenced_cnt++;
        info.operands.push_back(r_arr_ref->getArrayName()->to_string());
    } else {
        info.operands.push_back(r_expr->to_string());
        iv_info_map[r_expr->to_string()].referenced_cnt++;
    }

    // Set disp
    // Merge disp and shift to middle
    // e.g. registerArray(T, A[j][i+1]+B[j][i+3]) will register T[j][i]=A[j][i-1]+B[j][i+1], instead of T[j][i]=A[j][i]+B[j][i+2] etc.
    for (auto &d : l_delta) {
        if (r_delta.find(d.first) == r_delta.end()) {
            d.second = 0;
        } else {
            int shift = std::abs(r_delta[d.first] - l_delta[d.first]) >> 1;
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
            if (l_delta.find(var->to_string()) != l_delta.end()) {
                info.disp[0].push_back({var->to_string(), l_delta[var->to_string()]});
                used_var = true;
            }
            if (r_delta.find(var->to_string()) != r_delta.end()) {
                info.disp[1].push_back({var->to_string(), r_delta[var->to_string()]});
                used_var = true;
            }
            if (used_var) {
                info.indices.push_back(var->to_string());
            }
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

shared_ptr<ir::Expression> IVInfoManager::getExpression(string array, shared_ptr<ir::ExprTreeNode> l_expr_node, shared_ptr<ir::ExprTreeNode> r_expr_node) {
    if (!hasIVInfo(array)) {
        std::cout << array << ":" << l_expr_node->to_string() << ", " << r_expr_node->to_string() << std::endl;
        throw std::runtime_error("Array not registered.");
    }
    auto l_expr = l_expr_node->getExpression();
    auto r_expr = r_expr_node->getExpression();
    std::map<string, int> cur_expr_delta;
    auto info = iv_info_map[array];
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
        ;
    } else {
        vector<std::pair<string, int>> disp;
        for (const auto &idx : iv_info_map[array].indices) {
            disp.push_back({idx, cur_expr_delta[idx]});
        }
        return std::make_shared<ir::ArrayRef>(array, disp, parent_for);
    }
}

shared_ptr<ir::Expression> IVInfoManager::getConstructExpr(const string &array) {
    if (!hasIVInfo(array)) {
        throw std::runtime_error("Array not registered.");
    }
    auto info = iv_info_map[array];
    if (info.operands.empty()) {
        return std::make_shared<ir::DeclRef>(array);
    }
    shared_ptr<ir::Expression> l_expr, r_expr;
    if (this->isNativeArray(info.operands[0])) {
        if (info.disp[0].empty()) {
            l_expr = std::make_shared<ir::DeclRef>(info.operands[0]);
        } else {
            l_expr = std::make_shared<ir::ArrayRef>(info.operands[0], info.disp[0], parent_for);
        }
    } else {
        l_expr = getConstructExpr(info.operands[0]);
    }

    if (info.operands.size() > 1) { // binary operator
        if (this->isNativeArray(info.operands[1])) {
            if (info.disp[1].empty()) {
                r_expr = std::make_shared<ir::DeclRef>(info.operands[1]);
            } else {
                r_expr = std::make_shared<ir::ArrayRef>(info.operands[1], info.disp[1], parent_for);
            }
        } else {
            r_expr = getConstructExpr(info.operands[1]);
        }
        return ir::buildBinaryOp(info.op, l_expr, r_expr);
    } else {
        return ir::buildUnaryOp(info.op, l_expr);
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
            std::cout << "], disp: [";
            for (auto d : arr.second.disp) {
                std::cout << "[";
                for (auto i : d)
                    std::cout << "(" << i.first << ", " << i.second << ") ";
                std::cout << "] ";
            }
            std::cout << "], indices: [";
            for (auto i : arr.second.indices)
                std::cout << i << " ";
            std::cout << "]" << std::endl;
        }
    }
    std::cout << "Referenced Count: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": " << arr.second.referenced_cnt << std::endl;
    }
    // print iv_info_map
    std::cout << "Bound: " << std::endl;
    for (auto arr : iv_info_map) {
        std::cout << arr.first << ": [";
        for (auto i : arr.second.lower_bounds)
            std::cout << "(" << i.first << ", " << i.second << ") ";
        std::cout << "], [";
        for (auto i : arr.second.upper_bounds)
            std::cout << "(" << i.first << ", " << i.second << ") ";
        std::cout << "]" << std::endl;
    }
    std::cout << "------------" << std::endl;
}