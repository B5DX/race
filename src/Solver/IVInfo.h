#ifndef IVINFO_HPP
#define IVINFO_HPP

#include "IR/ExprTree.h"

/**
 * @brief IVInfo stores the information of Intermidiate or native ArrayRef. Number and DeclRef are also supported.
 * The object contains the following information:
 * 1. op: the operator of the expression, e.g. "+", "-", "*", "/".
 * 2. operands: the operands of the expression. If this is a native array, operands is empty.
 * 3. disp: the displacement of each operand. If the operand is a DeclRef or Number, disp is empty.
 * 4. index_vars: the index_vars of the array. Used in generating expression.
 * 5. referenced_cnt: the count of references of the array .
 * 6. lower_bounds: the lower bounds of each dimension.
 * 7. upper_bounds: the upper bounds of each dimension.
 * 
 * For example, T[j][i] = A[i+1] + B[j][i-1], T is an intermediate array, A and B are native arrays. Then IVInfoManager will map T to an IVInfo object.
 * The object will be:
 * op: "+"
 * operands: {"A", "B"}
 * disp: {{("i", 1)}, {("i", -1), ("j", 0)}}
 * index_vars: {"i", "j"}
 */
struct IVInfo {
    // shared_ptr<ir::Expression> iv_expr = nullptr;
    using Disp = vector<std::pair<string, int>>; // e.g. A[j+1][i+2] -> {("i", 2), ("j", 1)}.
    string op;
    vector<string> operands; // assert(operands.size() == disp.size()). If operand[i] is DeclRef or Number, disp[i] is empty.
    vector<Disp> disp;
    vector<string> index_vars; // e.g. T[i][j] (=A[i]+B[j]) -> {"i", "j"}. Used in generating expression.
    int referenced_cnt;
    std::map<string, int> lower_bounds;
    std::map<string, int> upper_bounds;

    // Record the dim-delta where this iv is used. Empty for native array, DeclRef and Number.
    // e.g. T is used T[j+1][i-1] and T[j-1][i], then used_max_delta["j"] = 1, used_min_delta["j"] = -1.
    std::map<string, int> used_max_delta;
    std::map<string, int> used_min_delta;
};

class IVInfoManager {
private:
    std::map<string, IVInfo> iv_info_map;
    ir::ForStmt *parent_for;

public:
    IVInfoManager() : parent_for(nullptr) {}
    // IVInfoManager(ir::ForStmt *parent_for) : parent_for(parent_for) {}

    void addIVInfo(const string &iv_name, const IVInfo &iv_info) {
        iv_info_map[iv_name] = iv_info;
    }

    IVInfo getIVInfo(const string &iv_name) {
        return iv_info_map[iv_name];
    }

    void setParentFor(ir::ForStmt *parent_for) {
        this->parent_for = parent_for;
    }

    ir::ForStmt *getParentFor() {
        return parent_for;
    }

    bool hasIVInfo(const string &iv_name) {
        return iv_info_map.find(iv_name) != iv_info_map.end();
    }

    bool isNativeArray(const string &array) {
        return iv_info_map[array].operands.empty();
    }

    bool isArrayRef(const string &array) {
        for (const auto &disp : iv_info_map[array].disp) {
            if (!disp.empty()) {
                return true;
            }
        }
        return false;
    }

    void registerNativeArray(const shared_ptr<ir::ExprTree> &tree);

    // Register an array with only one operand.
    void registerArray(string array, shared_ptr<ir::ExprTreeNode> sub_expr);
    
    // Register an array with two operands.
    void registerArray(string array, shared_ptr<ir::ExprTreeNode> l_expr, shared_ptr<ir::ExprTreeNode> r_expr);

    // Register an used intermediate array after the extraction. This function is used for post-processing.
    void registerUsage(shared_ptr<ir::ExprTreeNode> expr_node);

    /**
     * @brief Get the ArrayRef or DeclRef Expression of an array with one operand.
     * e.g. T[j][i] (= sin(A[j][i])) is registered. getRefExpr("T", A[j-1][i+1]) will return T[j-1][i+1].
     */
    shared_ptr<ir::Expression> getRefExpr(string array, shared_ptr<ir::ExprTreeNode> sub_expr);
    /**
     * @brief Get the ArrayRef or DeclRef Expression of an array with two operands.
     * e.g. T[j][i] (= A[i+1] + B[j][i-1]) is registered. getRefExpr("T", A[i], B[j+1][i-2]) will return T[j+1][i-1].
     */
    shared_ptr<ir::Expression> getRefExpr(string array, shared_ptr<ir::ExprTreeNode> l_expr_node, shared_ptr<ir::ExprTreeNode> r_expr_node);

    /**
     * @brief Get the construct expression of an array.
     * e.g. T[j][i] (= A[i+1] + B[j][i-1]) is registered. getConstructRHS("T") will return A[i+1]+B[j][i-1].
     */
    shared_ptr<ir::Expression> getConstructRHS(const string &array, std::map<string, int> outer_const_disp = {});

    std::map<string, int> getUsedMaxDelta(const string &array) {
        return iv_info_map[array].used_max_delta;
    }

    std::map<string, int> getUsedMinDelta(const string &array) {
        return iv_info_map[array].used_min_delta;
    }

    vector<string> IVTopologicalSort();

    vector<string> getIndices(const string &array) {
        return iv_info_map[array].index_vars;
    }

    std::set<string> getIVArray() {
        std::set<string> ret;
        for (const auto &iter : iv_info_map) {
            if (!isNativeArray(iter.first)) {
                ret.insert(iter.first);
            }
        }
        return ret;
    }

    void printInfo();
};

#endif